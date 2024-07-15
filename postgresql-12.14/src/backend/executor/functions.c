                                                                            
   
               
                                         
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/functions.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "storage/proc.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

   
                                                                          
   
typedef struct
{
  DestReceiver pub;                                              
  Tuplestorestate *tstore;                                 
  MemoryContext cxt;                                      
  JunkFilter *filter;                                        
} DR_sqlfunction;

   
                                                                         
                                                                           
                                                  
   
                                                                              
                                                                           
                                                      
   
typedef enum
{
  F_EXEC_START,
  F_EXEC_RUN,
  F_EXEC_DONE
} ExecStatus;

typedef struct execution_state
{
  struct execution_state *next;
  ExecStatus status;
  bool setsResult;                                                  
  bool lazyEval;                                                 
  PlannedStmt *stmt;                          
  QueryDesc *qd;                                    
} execution_state;

   
                                                              
                                                                 
   
                                                                        
                                                                             
                                 
   
                                                                             
                                                                         
                                                                           
                                                                              
                                                                              
                                                                              
                                                                             
                                               
   
typedef struct
{
  char *fname;                                     
  char *src;                                            

  SQLFunctionParseInfoPtr pinfo;                                     

  Oid rettype;                                
  int16 typlen;                                      
  bool typbyval;                                                
  bool returnsSet;                                         
  bool returnsTuple;                                            
  bool shutdown_reg;                                            
  bool readonly_func;                                      
  bool lazyEval;                                                   

  ParamListInfo paramLI;                                           

  Tuplestorestate *tstore;                                        

  JunkFilter *junkFilter;                                            

     
                                                                           
                                                                           
                                                                            
                                                       
     
  List *func_state;

  MemoryContext fcontext;                                               
                                               

  LocalTransactionId lxid;                                   
  SubTransactionId subxid;                                     
} SQLFunctionCache;

typedef SQLFunctionCache *SQLFunctionCachePtr;

   
                                                                           
                                                                              
                                                                             
   
typedef struct SQLFunctionParseInfo
{
  char *fname;                          
  int nargs;                                      
  Oid *argtypes;                                          
  char **argnames;                                             
                                                                   
  Oid collation;                                           
} SQLFunctionParseInfo;

                                    
static Node *
sql_fn_param_ref(ParseState *pstate, ParamRef *pref);
static Node *
sql_fn_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var);
static Node *
sql_fn_make_param(SQLFunctionParseInfoPtr pinfo, int paramno, int location);
static Node *
sql_fn_resolve_param_name(SQLFunctionParseInfoPtr pinfo, const char *paramname, int location);
static List *
init_execution_state(List *queryTree_list, SQLFunctionCachePtr fcache, bool lazyEvalOK);
static void
init_sql_fcache(FmgrInfo *finfo, Oid collation, bool lazyEvalOK);
static void
postquel_start(execution_state *es, SQLFunctionCachePtr fcache);
static bool
postquel_getnext(execution_state *es, SQLFunctionCachePtr fcache);
static void
postquel_end(execution_state *es);
static void
postquel_sub_params(SQLFunctionCachePtr fcache, FunctionCallInfo fcinfo);
static Datum
postquel_get_single_result(TupleTableSlot *slot, FunctionCallInfo fcinfo, SQLFunctionCachePtr fcache, MemoryContext resultcontext);
static void
sql_exec_error_callback(void *arg);
static void
ShutdownSQLFunction(Datum arg);
static void
sqlfunction_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static bool
sqlfunction_receive(TupleTableSlot *slot, DestReceiver *self);
static void
sqlfunction_shutdown(DestReceiver *self);
static void
sqlfunction_destroy(DestReceiver *self);

   
                                                                           
   
                                                                  
   
                                                                           
                          
   
SQLFunctionParseInfoPtr
prepare_sql_fn_parse_info(HeapTuple procedureTuple, Node *call_expr, Oid inputCollation)
{
  SQLFunctionParseInfoPtr pinfo;
  Form_pg_proc procedureStruct = (Form_pg_proc)GETSTRUCT(procedureTuple);
  int nargs;

  pinfo = (SQLFunctionParseInfoPtr)palloc0(sizeof(SQLFunctionParseInfo));

                                                                    
  pinfo->fname = pstrdup(NameStr(procedureStruct->proname));

                                           
  pinfo->collation = inputCollation;

     
                                                                        
                        
     
  pinfo->nargs = nargs = procedureStruct->pronargs;
  if (nargs > 0)
  {
    Oid *argOidVect;
    int argnum;

    argOidVect = (Oid *)palloc(nargs * sizeof(Oid));
    memcpy(argOidVect, procedureStruct->proargtypes.values, nargs * sizeof(Oid));

    for (argnum = 0; argnum < nargs; argnum++)
    {
      Oid argtype = argOidVect[argnum];

      if (IsPolymorphicType(argtype))
      {
        argtype = get_call_expr_argtype(call_expr, argnum);
        if (argtype == InvalidOid)
        {
          ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("could not determine actual type of argument declared %s", format_type_be(argOidVect[argnum]))));
        }
        argOidVect[argnum] = argtype;
      }
    }

    pinfo->argtypes = argOidVect;
  }

     
                                             
     
  if (nargs > 0)
  {
    Datum proargnames;
    Datum proargmodes;
    int n_arg_names;
    bool isNull;

    proargnames = SysCacheGetAttr(PROCNAMEARGSNSP, procedureTuple, Anum_pg_proc_proargnames, &isNull);
    if (isNull)
    {
      proargnames = PointerGetDatum(NULL);                      
    }

    proargmodes = SysCacheGetAttr(PROCNAMEARGSNSP, procedureTuple, Anum_pg_proc_proargmodes, &isNull);
    if (isNull)
    {
      proargmodes = PointerGetDatum(NULL);                      
    }

    n_arg_names = get_func_input_arg_names(proargnames, proargmodes, &pinfo->argnames);

                                                              
    if (n_arg_names < nargs)
    {
      pinfo->argnames = NULL;
    }
  }
  else
  {
    pinfo->argnames = NULL;
  }

  return pinfo;
}

   
                                                      
   
void
sql_fn_parser_setup(struct ParseState *pstate, SQLFunctionParseInfoPtr pinfo)
{
  pstate->p_pre_columnref_hook = NULL;
  pstate->p_post_columnref_hook = sql_fn_post_column_ref;
  pstate->p_paramref_hook = sql_fn_param_ref;
                                          
  pstate->p_ref_hook_state = (void *)pinfo;
}

   
                                                          
   
static Node *
sql_fn_post_column_ref(ParseState *pstate, ColumnRef *cref, Node *var)
{
  SQLFunctionParseInfoPtr pinfo = (SQLFunctionParseInfoPtr)pstate->p_ref_hook_state;
  int nnames;
  Node *field1;
  Node *subfield = NULL;
  const char *name1;
  const char *name2 = NULL;
  Node *param;

     
                                                                   
                                                                      
                                                     
     
  if (var != NULL)
  {
    return NULL;
  }

               
                               
     
                           
                                                
                                                           
                                                
                                                               
                      
                                                        
                                                            
     
                                                                           
                                                                      
               
     
  nnames = list_length(cref->fields);

  if (nnames > 3)
  {
    return NULL;
  }

  if (IsA(llast(cref->fields), A_Star))
  {
    nnames--;
  }

  field1 = (Node *)linitial(cref->fields);
  Assert(IsA(field1, String));
  name1 = strVal(field1);
  if (nnames > 1)
  {
    subfield = (Node *)lsecond(cref->fields);
    Assert(IsA(subfield, String));
    name2 = strVal(subfield);
  }

  if (nnames == 3)
  {
       
                                                                           
                                                                        
                                                    
       
    if (strcmp(name1, pinfo->fname) != 0)
    {
      return NULL;
    }

    param = sql_fn_resolve_param_name(pinfo, name2, cref->location);

    subfield = (Node *)lthird(cref->fields);
    Assert(IsA(subfield, String));
  }
  else if (nnames == 2 && strcmp(name1, pinfo->fname) == 0)
  {
       
                                                                          
                                               
       
    param = sql_fn_resolve_param_name(pinfo, name2, cref->location);

    if (param)
    {
                                                              
      subfield = NULL;
    }
    else
    {
                                                              
      param = sql_fn_resolve_param_name(pinfo, name1, cref->location);
    }
  }
  else
  {
                                                             
    param = sql_fn_resolve_param_name(pinfo, name1, cref->location);
  }

  if (!param)
  {
    return NULL;               
  }

  if (subfield)
  {
       
                                                                          
                                                                      
               
       
    param = ParseFuncOrColumn(pstate, list_make1(subfield), list_make1(param), pstate->p_last_srf, NULL, false, cref->location);
  }

  return param;
}

   
                                                                
   
static Node *
sql_fn_param_ref(ParseState *pstate, ParamRef *pref)
{
  SQLFunctionParseInfoPtr pinfo = (SQLFunctionParseInfoPtr)pstate->p_ref_hook_state;
  int paramno = pref->number;

                                       
  if (paramno <= 0 || paramno > pinfo->nargs)
  {
    return NULL;                               
  }

  return sql_fn_make_param(pinfo, paramno, pref->location);
}

   
                                                                   
   
static Node *
sql_fn_make_param(SQLFunctionParseInfoPtr pinfo, int paramno, int location)
{
  Param *param;

  param = makeNode(Param);
  param->paramkind = PARAM_EXTERN;
  param->paramid = paramno;
  param->paramtype = pinfo->argtypes[paramno - 1];
  param->paramtypmod = -1;
  param->paramcollid = get_typcollation(param->paramtype);
  param->location = location;

     
                                                                     
                                                                             
                                                       
     
  if (OidIsValid(pinfo->collation) && OidIsValid(param->paramcollid))
  {
    param->paramcollid = pinfo->collation;
  }

  return (Node *)param;
}

   
                                                                       
                                                                   
                                               
   
static Node *
sql_fn_resolve_param_name(SQLFunctionParseInfoPtr pinfo, const char *paramname, int location)
{
  int i;

  if (pinfo->argnames == NULL)
  {
    return NULL;
  }

  for (i = 0; i < pinfo->nargs; i++)
  {
    if (pinfo->argnames[i] && strcmp(pinfo->argnames[i], paramname) == 0)
    {
      return sql_fn_make_param(pinfo, i + 1, location);
    }
  }

  return NULL;
}

   
                                                                    
   
                                                                          
                                                                             
   
static List *
init_execution_state(List *queryTree_list, SQLFunctionCachePtr fcache, bool lazyEvalOK)
{
  List *eslist = NIL;
  execution_state *lasttages = NULL;
  ListCell *lc1;

  foreach (lc1, queryTree_list)
  {
    List *qtlist = lfirst_node(List, lc1);
    execution_state *firstes = NULL;
    execution_state *preves = NULL;
    ListCell *lc2;

    foreach (lc2, qtlist)
    {
      Query *queryTree = lfirst_node(Query, lc2);
      PlannedStmt *stmt;
      execution_state *newes;

                                    
      if (queryTree->commandType == CMD_UTILITY)
      {
                                                   
        stmt = makeNode(PlannedStmt);
        stmt->commandType = CMD_UTILITY;
        stmt->canSetTag = queryTree->canSetTag;
        stmt->utilityStmt = queryTree->utilityStmt;
        stmt->stmt_location = queryTree->stmt_location;
        stmt->stmt_len = queryTree->stmt_len;
      }
      else
      {
        stmt = pg_plan_query(queryTree, CURSOR_OPT_PARALLEL_OK, NULL);
      }

         
                                                                        
                                                         
         
      if (stmt->commandType == CMD_UTILITY)
      {
        if (IsA(stmt->utilityStmt, CopyStmt) && ((CopyStmt *)stmt->utilityStmt)->filename == NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot COPY to/from client in a SQL function")));
        }

        if (IsA(stmt->utilityStmt, TransactionStmt))
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                         
                             errmsg("%s is not allowed in a SQL function", CreateCommandTag(stmt->utilityStmt))));
        }
      }

      if (fcache->readonly_func && !CommandIsReadOnly(stmt))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                       
                           errmsg("%s is not allowed in a non-volatile function", CreateCommandTag((Node *)stmt))));
      }

      if (IsInParallelMode() && !CommandIsReadOnly(stmt))
      {
        PreventCommandIfParallelMode(CreateCommandTag((Node *)stmt));
      }

                                                        
      newes = (execution_state *)palloc(sizeof(execution_state));
      if (preves)
      {
        preves->next = newes;
      }
      else
      {
        firstes = newes;
      }

      newes->next = NULL;
      newes->status = F_EXEC_START;
      newes->setsResult = false;                         
      newes->lazyEval = false;                           
      newes->stmt = stmt;
      newes->qd = NULL;

      if (queryTree->canSetTag)
      {
        lasttages = newes;
      }

      preves = newes;
    }

    eslist = lappend(eslist, firstes);
  }

     
                                                                            
                                                                         
                                                 
     
                                                                             
                                                                           
                                                                             
           
     
                                                                           
                                                                         
                                                        
     
  if (lasttages && fcache->junkFilter)
  {
    lasttages->setsResult = true;
    if (lazyEvalOK && lasttages->stmt->commandType == CMD_SELECT && !lasttages->stmt->hasModifyingCTE)
    {
      fcache->lazyEval = lasttages->lazyEval = true;
    }
  }

  return eslist;
}

   
                                                      
   
static void
init_sql_fcache(FmgrInfo *finfo, Oid collation, bool lazyEvalOK)
{
  Oid foid = finfo->fn_oid;
  MemoryContext fcontext;
  MemoryContext oldcontext;
  Oid rettype;
  HeapTuple procedureTuple;
  Form_pg_proc procedureStruct;
  SQLFunctionCachePtr fcache;
  List *raw_parsetree_list;
  List *queryTree_list;
  List *flat_query_list;
  ListCell *lc;
  Datum tmp;
  bool isNull;

     
                                                                         
                                                             
     
  fcontext = AllocSetContextCreate(finfo->fn_mcxt, "SQL function", ALLOCSET_DEFAULT_SIZES);

  oldcontext = MemoryContextSwitchTo(fcontext);

     
                                                                            
                                                                             
                             
     
  fcache = (SQLFunctionCachePtr)palloc0(sizeof(SQLFunctionCache));
  fcache->fcontext = fcontext;
  finfo->fn_extra = (void *)fcache;

     
                                                                     
     
  procedureTuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(foid));
  if (!HeapTupleIsValid(procedureTuple))
  {
    elog(ERROR, "cache lookup failed for function %u", foid);
  }
  procedureStruct = (Form_pg_proc)GETSTRUCT(procedureTuple);

     
                                                                             
                                          
     
  fcache->fname = pstrdup(NameStr(procedureStruct->proname));
  MemoryContextSetIdentifier(fcontext, fcache->fname);

     
                                                                             
                                                          
     
  rettype = procedureStruct->prorettype;

  if (IsPolymorphicType(rettype))
  {
    rettype = get_fn_expr_rettype(finfo);
    if (rettype == InvalidOid)                                      
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("could not determine actual result type for function declared to return type %s", format_type_be(procedureStruct->prorettype))));
    }
  }

  fcache->rettype = rettype;

                                                           
  get_typlenbyval(rettype, &fcache->typlen, &fcache->typbyval);

                                                        
  fcache->returnsSet = procedureStruct->proretset;

                                                
  fcache->readonly_func = (procedureStruct->provolatile != PROVOLATILE_VOLATILE);

     
                                                                         
                                                                       
                               
     
  fcache->pinfo = prepare_sql_fn_parse_info(procedureTuple, finfo->fn_expr, collation);

     
                                                   
     
  tmp = SysCacheGetAttr(PROCOID, procedureTuple, Anum_pg_proc_prosrc, &isNull);
  if (isNull)
  {
    elog(ERROR, "null prosrc for function %u", foid);
  }
  fcache->src = TextDatumGetCString(tmp);

     
                                                                          
                                                                       
                                                                          
                                                                         
                                                                           
                                                                            
                                                                         
                                                                          
     
                                                                            
                                                                             
                                                                       
                  
     
  raw_parsetree_list = pg_parse_query(fcache->src);

  queryTree_list = NIL;
  flat_query_list = NIL;
  foreach (lc, raw_parsetree_list)
  {
    RawStmt *parsetree = lfirst_node(RawStmt, lc);
    List *queryTree_sublist;

    queryTree_sublist = pg_analyze_and_rewrite_params(parsetree, fcache->src, (ParserSetupHook)sql_fn_parser_setup, fcache->pinfo, NULL);
    queryTree_list = lappend(queryTree_list, queryTree_sublist);
    flat_query_list = list_concat(flat_query_list, list_copy(queryTree_sublist));
  }

  check_sql_fn_statements(flat_query_list);

     
                                                                         
                                                                          
                                                                             
                                                                             
                                
     
                                                                             
                                                                            
                                                                          
                                                                             
                                                                           
                                                                       
                                                
     
                                                                        
                                                                             
                                                           
     
  fcache->returnsTuple = check_sql_fn_retval(foid, rettype, flat_query_list, NULL, &fcache->junkFilter);

  if (fcache->returnsTuple)
  {
                                                      
    BlessTupleDesc(fcache->junkFilter->jf_resultSlot->tts_tupleDescriptor);
  }
  else if (fcache->returnsSet && type_is_rowtype(fcache->rettype))
  {
       
                                                                          
                                                                       
                                                                        
                                                                        
       
    lazyEvalOK = true;
  }

                                 
  fcache->func_state = init_execution_state(queryTree_list, fcache, lazyEvalOK);

                                                            
  fcache->lxid = MyProc->lxid;
  fcache->subxid = GetCurrentSubTransactionId();

  ReleaseSysCache(procedureTuple);

  MemoryContextSwitchTo(oldcontext);
}

                                                    
static void
postquel_start(execution_state *es, SQLFunctionCachePtr fcache)
{
  DestReceiver *dest;

  Assert(es->qd == NULL);

                                                                
  Assert(ActiveSnapshotSet());

     
                                                                        
                                          
     
  if (es->setsResult)
  {
    DR_sqlfunction *myState;

    dest = CreateDestReceiver(DestSQLFunction);
                                                                 
    myState = (DR_sqlfunction *)dest;
    Assert(myState->pub.mydest == DestSQLFunction);
    myState->tstore = fcache->tstore;
    myState->cxt = CurrentMemoryContext;
    myState->filter = fcache->junkFilter;
  }
  else
  {
    dest = None_Receiver;
  }

  es->qd = CreateQueryDesc(es->stmt, fcache->src, GetActiveSnapshot(), InvalidSnapshot, dest, fcache->paramLI, es->qd ? es->qd->queryEnv : NULL, 0);

                                             
  if (es->qd->operation != CMD_UTILITY)
  {
       
                                                                        
                                                                        
                                                               
                                                                      
                                                                           
       
    int eflags;

    if (es->lazyEval)
    {
      eflags = EXEC_FLAG_SKIP_TRIGGERS;
    }
    else
    {
      eflags = 0;                                      
    }
    ExecutorStart(es->qd, eflags);
  }

  es->status = F_EXEC_RUN;
}

                                                                          
                                          
static bool
postquel_getnext(execution_state *es, SQLFunctionCachePtr fcache)
{
  bool result;

  if (es->qd->operation == CMD_UTILITY)
  {
    ProcessUtility(es->qd->plannedstmt, fcache->src, PROCESS_UTILITY_QUERY, es->qd->params, es->qd->queryEnv, es->qd->dest, NULL);
    result = true;                        
  }
  else
  {
                                                            
    uint64 count = (es->lazyEval) ? 1 : 0;

    ExecutorRun(es->qd, ForwardScanDirection, count, !fcache->returnsSet || !es->lazyEval);

       
                                                                         
                                 
       
    result = (count == 0 || es->qd->estate->es_processed == 0);
  }

  return result;
}

                                                     
static void
postquel_end(execution_state *es)
{
                                                                
  es->status = F_EXEC_DONE;

                                             
  if (es->qd->operation != CMD_UTILITY)
  {
    ExecutorFinish(es->qd);
    ExecutorEnd(es->qd);
  }

  es->qd->dest->rDestroy(es->qd->dest);

  FreeQueryDesc(es->qd);
  es->qd = NULL;
}

                                                              
static void
postquel_sub_params(SQLFunctionCachePtr fcache, FunctionCallInfo fcinfo)
{
  int nargs = fcinfo->nargs;

  if (nargs > 0)
  {
    ParamListInfo paramLI;
    Oid *argtypes = fcache->pinfo->argtypes;

    if (fcache->paramLI == NULL)
    {
      paramLI = makeParamList(nargs);
      fcache->paramLI = paramLI;
    }
    else
    {
      paramLI = fcache->paramLI;
      Assert(paramLI->numParams == nargs);
    }

    for (int i = 0; i < nargs; i++)
    {
      ParamExternData *prm = &paramLI->params[i];

         
                                                                    
                                                                         
                                                                     
                                                                        
                                                                    
                                                                    
                                                                      
                                                                 
                                                                         
                                                                 
         
      prm->isnull = fcinfo->args[i].isnull;
      prm->value = MakeExpandedObjectReadOnly(fcinfo->args[i].value, prm->isnull, get_typlen(argtypes[i]));
      prm->pflags = 0;
      prm->ptype = argtypes[i];
    }
  }
  else
  {
    fcache->paramLI = NULL;
  }
}

   
                                                                            
                                                                           
           
   
static Datum
postquel_get_single_result(TupleTableSlot *slot, FunctionCallInfo fcinfo, SQLFunctionCachePtr fcache, MemoryContext resultcontext)
{
  Datum value;
  MemoryContext oldcontext;

     
                                                                            
                                                                             
                                                                         
                                                                          
     
  oldcontext = MemoryContextSwitchTo(resultcontext);

  if (fcache->returnsTuple)
  {
                                                    
    fcinfo->isnull = false;
    value = ExecFetchSlotHeapTupleDatum(slot);
  }
  else
  {
       
                                                                          
                                                                          
       
    value = slot_getattr(slot, 1, &(fcinfo->isnull));

    if (!fcinfo->isnull)
    {
      value = datumCopy(value, fcache->typbyval, fcache->typlen);
    }
  }

  MemoryContextSwitchTo(oldcontext);

  return value;
}

   
                                                     
   
Datum
fmgr_sql(PG_FUNCTION_ARGS)
{
  SQLFunctionCachePtr fcache;
  ErrorContextCallback sqlerrcontext;
  MemoryContext oldcontext;
  bool randomAccess;
  bool lazyEvalOK;
  bool is_first;
  bool pushed_snapshot;
  execution_state *es;
  TupleTableSlot *slot;
  Datum result;
  List *eslist;
  ListCell *eslc;

     
                                                 
     
  sqlerrcontext.callback = sql_exec_error_callback;
  sqlerrcontext.arg = fcinfo->flinfo;
  sqlerrcontext.previous = error_context_stack;
  error_context_stack = &sqlerrcontext;

                          
  if (fcinfo->flinfo->fn_retset)
  {
    ReturnSetInfo *rsi = (ReturnSetInfo *)fcinfo->resultinfo;

       
                                                                          
                                                                        
                                                                          
                                                                 
       
    if (!rsi || !IsA(rsi, ReturnSetInfo) || (rsi->allowedModes & SFRM_ValuePerCall) == 0 || (rsi->allowedModes & SFRM_Materialize) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
    }
    randomAccess = rsi->allowedModes & SFRM_Materialize_Random;
    lazyEvalOK = !(rsi->allowedModes & SFRM_Materialize_Preferred);
  }
  else
  {
    randomAccess = false;
    lazyEvalOK = true;
  }

     
                                                                             
                            
     
  fcache = (SQLFunctionCachePtr)fcinfo->flinfo->fn_extra;

  if (fcache != NULL)
  {
    if (fcache->lxid != MyProc->lxid || !SubTransactionIsActive(fcache->subxid))
    {
                                         
      fcinfo->flinfo->fn_extra = NULL;
      MemoryContextDelete(fcache->fcontext);
      fcache = NULL;
    }
  }

  if (fcache == NULL)
  {
    init_sql_fcache(fcinfo->flinfo, PG_GET_COLLATION(), lazyEvalOK);
    fcache = (SQLFunctionCachePtr)fcinfo->flinfo->fn_extra;
  }

     
                                                                         
                                                                        
                                                                            
                                                                          
                                                           
     
  oldcontext = MemoryContextSwitchTo(fcache->fcontext);

     
                                                                        
                  
     
  eslist = fcache->func_state;
  es = NULL;
  is_first = true;
  foreach (eslc, eslist)
  {
    es = (execution_state *)lfirst(eslc);

    while (es && es->status == F_EXEC_DONE)
    {
      is_first = false;
      es = es->next;
    }

    if (es)
    {
      break;
    }
  }

     
                                                                             
                                                        
     
  if (is_first && es && es->status == F_EXEC_START)
  {
    postquel_sub_params(fcache, fcinfo);
  }

     
                                                                          
                                         
     
  if (!fcache->tstore)
  {
    fcache->tstore = tuplestore_begin_heap(randomAccess, false, work_mem);
  }

     
                                                                            
                                                                             
     
                                      
     
                                                                            
     
                                                                       
                                                                           
                                                                             
                                                                        
                                                                          
                                                                            
                                                                             
                                                                     
                                                                          
     
  pushed_snapshot = false;
  while (es)
  {
    bool completed;

    if (es->status == F_EXEC_START)
    {
         
                                                                      
                                                                       
                                                                 
                                                                      
         
      if (!fcache->readonly_func)
      {
        CommandCounterIncrement();
        if (!pushed_snapshot)
        {
          PushActiveSnapshot(GetTransactionSnapshot());
          pushed_snapshot = true;
        }
        else
        {
          UpdateActiveSnapshotCommandId();
        }
      }

      postquel_start(es, fcache);
    }
    else if (!fcache->readonly_func && !pushed_snapshot)
    {
                                                                  
      PushActiveSnapshot(es->qd->snapshot);
      pushed_snapshot = true;
    }

    completed = postquel_getnext(es, fcache);

       
                                                                         
                                                                          
                                                                      
                                                                        
                                                                          
                                                       
       
    if (completed || !fcache->returnsSet)
    {
      postquel_end(es);
    }

       
                                                                 
                                                                       
                                                                       
                                                                   
                                                                    
                                                                     
                                                                  
       
    if (es->status != F_EXEC_DONE)
    {
      break;
    }

       
                                                                         
       
    es = es->next;
    while (!es)
    {
      eslc = lnext(eslc);
      if (!eslc)
      {
        break;                      
      }

      es = (execution_state *)lfirst(eslc);

         
                                                                       
                                                                       
                                                                         
                    
         
      if (pushed_snapshot)
      {
        PopActiveSnapshot();
        pushed_snapshot = false;
      }
    }
  }

     
                                                                            
     
  if (fcache->returnsSet)
  {
    ReturnSetInfo *rsi = (ReturnSetInfo *)fcinfo->resultinfo;

    if (es)
    {
         
                                                                     
              
         
      Assert(es->lazyEval);
                                                                       
      Assert(fcache->junkFilter);
      slot = fcache->junkFilter->jf_resultSlot;
      if (!tuplestore_gettupleslot(fcache->tstore, true, false, slot))
      {
        elog(ERROR, "failed to fetch lazy-eval tuple");
      }
                                                                     
      result = postquel_get_single_result(slot, fcinfo, fcache, oldcontext);
                                                           
                                                                       
      tuplestore_clear(fcache->tstore);

         
                                             
         
      rsi->isDone = ExprMultipleResult;

         
                                                                        
                            
         
      if (!fcache->shutdown_reg)
      {
        RegisterExprContextCallback(rsi->econtext, ShutdownSQLFunction, PointerGetDatum(fcache));
        fcache->shutdown_reg = true;
      }
    }
    else if (fcache->lazyEval)
    {
         
                                                        
         
      tuplestore_clear(fcache->tstore);

         
                                         
         
      rsi->isDone = ExprEndResult;

      fcinfo->isnull = true;
      result = (Datum)0;

                                                        
      if (fcache->shutdown_reg)
      {
        UnregisterExprContextCallback(rsi->econtext, ShutdownSQLFunction, PointerGetDatum(fcache));
        fcache->shutdown_reg = false;
      }
    }
    else
    {
         
                                                                        
                                                                         
                                
         
      rsi->returnMode = SFRM_Materialize;
      rsi->setResult = fcache->tstore;
      fcache->tstore = NULL;
                                                         
      if (fcache->junkFilter)
      {
        rsi->setDesc = CreateTupleDescCopy(fcache->junkFilter->jf_cleanTupType);
      }

      fcinfo->isnull = true;
      result = (Datum)0;

                                                        
      if (fcache->shutdown_reg)
      {
        UnregisterExprContextCallback(rsi->econtext, ShutdownSQLFunction, PointerGetDatum(fcache));
        fcache->shutdown_reg = false;
      }
    }
  }
  else
  {
       
                                                                        
       
    if (fcache->junkFilter)
    {
                                                                       
      slot = fcache->junkFilter->jf_resultSlot;
      if (tuplestore_gettupleslot(fcache->tstore, true, false, slot))
      {
        result = postquel_get_single_result(slot, fcinfo, fcache, oldcontext);
      }
      else
      {
        fcinfo->isnull = true;
        result = (Datum)0;
      }
    }
    else
    {
                                                                  
      Assert(fcache->rettype == VOIDOID);
      fcinfo->isnull = true;
      result = (Datum)0;
    }

                                                         
    tuplestore_clear(fcache->tstore);
  }

                                          
  if (pushed_snapshot)
  {
    PopActiveSnapshot();
  }

     
                                                                             
                                                            
     
  if (es == NULL)
  {
    foreach (eslc, fcache->func_state)
    {
      es = (execution_state *)lfirst(eslc);
      while (es)
      {
        es->status = F_EXEC_START;
        es = es->next;
      }
    }
  }

  error_context_stack = sqlerrcontext.previous;

  MemoryContextSwitchTo(oldcontext);

  return result;
}

   
                                                                  
   
static void
sql_exec_error_callback(void *arg)
{
  FmgrInfo *flinfo = (FmgrInfo *)arg;
  SQLFunctionCachePtr fcache = (SQLFunctionCachePtr)flinfo->fn_extra;
  int syntaxerrposition;

     
                                                                        
                              
     
  if (fcache == NULL || fcache->fname == NULL)
  {
    return;
  }

     
                                                                           
     
  syntaxerrposition = geterrposition();
  if (syntaxerrposition > 0 && fcache->src != NULL)
  {
    errposition(0);
    internalerrposition(syntaxerrposition);
    internalerrquery(fcache->src);
  }

     
                                                                            
                                                                             
                                                                  
                                                                             
                    
     
  if (fcache->func_state)
  {
    execution_state *es;
    int query_num;
    ListCell *lc;

    es = NULL;
    query_num = 1;
    foreach (lc, fcache->func_state)
    {
      es = (execution_state *)lfirst(lc);
      while (es)
      {
        if (es->qd)
        {
          errcontext("SQL function \"%s\" statement %d", fcache->fname, query_num);
          break;
        }
        es = es->next;
      }
      if (es)
      {
        break;
      }
      query_num++;
    }
    if (es == NULL)
    {
         
                                                                     
                                            
         
      errcontext("SQL function \"%s\"", fcache->fname);
    }
  }
  else
  {
       
                                                                           
                                                                       
                                                          
       
    errcontext("SQL function \"%s\" during startup", fcache->fname);
  }
}

   
                                                                            
                                        
   
static void
ShutdownSQLFunction(Datum arg)
{
  SQLFunctionCachePtr fcache = (SQLFunctionCachePtr)DatumGetPointer(arg);
  execution_state *es;
  ListCell *lc;

  foreach (lc, fcache->func_state)
  {
    es = (execution_state *)lfirst(lc);
    while (es)
    {
                                            
      if (es->status == F_EXEC_RUN)
      {
                                                                   
        if (!fcache->readonly_func)
        {
          PushActiveSnapshot(es->qd->snapshot);
        }

        postquel_end(es);

        if (!fcache->readonly_func)
        {
          PopActiveSnapshot();
        }
      }

                                                            
      es->status = F_EXEC_START;
      es = es->next;
    }
  }

                                         
  if (fcache->tstore)
  {
    tuplestore_end(fcache->tstore);
  }
  fcache->tstore = NULL;

                                                 
  fcache->shutdown_reg = false;
}

   
                           
   
                                                                             
                      
   
void
check_sql_fn_statements(List *queryTreeList)
{
  ListCell *lc;

  foreach (lc, queryTreeList)
  {
    Query *query = lfirst_node(Query, lc);

       
                                                               
                                                                          
                                                                          
                                                                        
                                           
       
    if (query->commandType == CMD_UTILITY && IsA(query->utilityStmt, CallStmt))
    {
      CallStmt *stmt = castNode(CallStmt, query->utilityStmt);
      HeapTuple tuple;
      int numargs;
      Oid *argtypes;
      char **argnames;
      char *argmodes;
      int i;

      tuple = SearchSysCache1(PROCOID, ObjectIdGetDatum(stmt->funcexpr->funcid));
      if (!HeapTupleIsValid(tuple))
      {
        elog(ERROR, "cache lookup failed for function %u", stmt->funcexpr->funcid);
      }
      numargs = get_func_arg_info(tuple, &argtypes, &argnames, &argmodes);
      ReleaseSysCache(tuple);

      for (i = 0; i < numargs; i++)
      {
        if (argmodes && (argmodes[i] == PROARGMODE_INOUT || argmodes[i] == PROARGMODE_OUT))
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("calling procedures with output arguments is not supported in SQL functions")));
        }
      }
    }
  }
}

   
                                                                             
   
                                                                        
                                                                          
                                                                        
                                                                          
                                                                       
   
                                                                             
                                                                             
                                                                              
                                                   
   
                                                                           
                                                                               
                                                                                
                                 
   
                                                                             
                                                                   
   
                                                                      
                                        
                                                                             
                                                                             
                                                       
                                                                          
                                                               
                                                                           
                                                                               
                                                                           
                                                                          
                                                                             
                                                                             
                                                              
   
                                                                             
                                                                            
                                                                            
                
   
bool
check_sql_fn_retval(Oid func_id, Oid rettype, List *queryTreeList, bool *modifyTargetList, JunkFilter **junkFilter)
{
  Query *parse;
  List **tlist_ptr;
  List *tlist;
  int tlistlen;
  char fn_typtype;
  Oid restype;
  ListCell *lc;

  AssertArg(!IsPolymorphicType(rettype));

  if (modifyTargetList)
  {
    *modifyTargetList = false;                               
  }
  if (junkFilter)
  {
    *junkFilter = NULL;                                        
  }

     
                                                                            
                                                       
     
  if (rettype == VOIDOID)
  {
    return false;
  }

     
                                                                            
                                                                          
                     
     
  parse = NULL;
  foreach (lc, queryTreeList)
  {
    Query *q = lfirst_node(Query, lc);

    if (q->canSetTag)
    {
      parse = q;
    }
  }

     
                                                                      
                                                                        
                                                             
     
                                                                           
                                                                             
                                                                     
                                                                       
               
     
  if (parse && parse->commandType == CMD_SELECT)
  {
    tlist_ptr = &parse->targetList;
    tlist = parse->targetList;
  }
  else if (parse && (parse->commandType == CMD_INSERT || parse->commandType == CMD_UPDATE || parse->commandType == CMD_DELETE) && parse->returningList)
  {
    tlist_ptr = &parse->returningList;
    tlist = parse->returningList;
  }
  else
  {
                                                                     
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("return type mismatch in function declared to return %s", format_type_be(rettype)), errdetail("Function's final statement must be SELECT or INSERT/UPDATE/DELETE RETURNING.")));
    return false;                          
  }

     
                                                                           
           
     

     
                                                          
     
  tlistlen = ExecCleanTargetListLength(tlist);

  fn_typtype = get_typtype(rettype);

  if (fn_typtype == TYPTYPE_BASE || fn_typtype == TYPTYPE_DOMAIN || fn_typtype == TYPTYPE_ENUM || fn_typtype == TYPTYPE_RANGE)
  {
       
                                                                      
                                                                  
                                                              
       
    TargetEntry *tle;

    if (tlistlen != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("return type mismatch in function declared to return %s", format_type_be(rettype)), errdetail("Final statement must return exactly one column.")));
    }

                                                                     
    tle = (TargetEntry *)linitial(tlist);
    Assert(!tle->resjunk);

    restype = exprType((Node *)tle->expr);
    if (!IsBinaryCoercible(restype, rettype))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("return type mismatch in function declared to return %s", format_type_be(rettype)), errdetail("Actual return type is %s.", format_type_be(restype))));
    }
    if (modifyTargetList && restype != rettype)
    {
      tle->expr = (Expr *)makeRelabelType(tle->expr, rettype, -1, get_typcollation(rettype), COERCE_IMPLICIT_CAST);
                                                                       
      if (tle->ressortgroupref != 0 || parse->setOperations)
      {
        *modifyTargetList = true;
      }
    }

                                      
    if (junkFilter)
    {
      *junkFilter = ExecInitJunkFilter(tlist, MakeSingleTupleTableSlot(NULL, &TTSOpsMinimalTuple));
    }
  }
  else if (fn_typtype == TYPTYPE_COMPOSITE || rettype == RECORDOID)
  {
       
                          
       
                                                                      
                                                                           
                                                                          
                                                                          
                                                                          
       
    TupleDesc tupdesc;
    int tupnatts;                                             
    int tuplogcols;                                        
    int colindex;                               
    List *newtlist;                                  
    List *junkattrs;                             

       
                                                                         
                                                                       
                                                                       
                                                                           
                                       
       
                                                                       
                                                                          
                                                                           
                                
       
    if (tlistlen == 1)
    {
      TargetEntry *tle = (TargetEntry *)linitial(tlist);

      Assert(!tle->resjunk);
      restype = exprType((Node *)tle->expr);
      if (IsBinaryCoercible(restype, rettype))
      {
        if (modifyTargetList && restype != rettype)
        {
          tle->expr = (Expr *)makeRelabelType(tle->expr, rettype, -1, get_typcollation(rettype), COERCE_IMPLICIT_CAST);
                                                                  
          if (tle->ressortgroupref != 0 || parse->setOperations)
          {
            *modifyTargetList = true;
          }
        }
                                          
        if (junkFilter)
        {
          TupleTableSlot *slot = MakeSingleTupleTableSlot(NULL, &TTSOpsMinimalTuple);

          *junkFilter = ExecInitJunkFilter(tlist, slot);
        }
        return false;                                
      }
    }

       
                                                                      
                                                   
       
    if (get_func_result_type(func_id, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
    {
         
                                                                        
                                                         
         
      if (junkFilter)
      {
        TupleTableSlot *slot;

        slot = MakeSingleTupleTableSlot(NULL, &TTSOpsMinimalTuple);
        *junkFilter = ExecInitJunkFilter(tlist, slot);
      }
      return true;
    }
    Assert(tupdesc);

       
                                                                         
                                                                          
                                                                        
                                                    
       
    tupnatts = tupdesc->natts;
    tuplogcols = 0;                                           
    colindex = 0;
    newtlist = NIL;                                              
    junkattrs = NIL;

    foreach (lc, tlist)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(lc);
      Form_pg_attribute attr;
      Oid tletype;
      Oid atttype;

      if (tle->resjunk)
      {
        if (modifyTargetList)
        {
          junkattrs = lappend(junkattrs, tle);
        }
        continue;
      }

      do
      {
        colindex++;
        if (colindex > tupnatts)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("return type mismatch in function declared to return %s", format_type_be(rettype)), errdetail("Final statement returns too many columns.")));
        }
        attr = TupleDescAttr(tupdesc, colindex - 1);
        if (attr->attisdropped && modifyTargetList)
        {
          Expr *null_expr;

                                                              
          null_expr = (Expr *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), (Datum)0, true,             
              true            );
          newtlist = lappend(newtlist, makeTargetEntry(null_expr, colindex, NULL, false));
                                                      
          if (parse->setOperations)
          {
            *modifyTargetList = true;
          }
        }
      } while (attr->attisdropped);
      tuplogcols++;

      tletype = exprType((Node *)tle->expr);
      atttype = attr->atttypid;
      if (!IsBinaryCoercible(tletype, atttype))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("return type mismatch in function declared to return %s", format_type_be(rettype)), errdetail("Final statement returns %s instead of %s at column %d.", format_type_be(tletype), format_type_be(atttype), tuplogcols)));
      }
      if (modifyTargetList)
      {
        if (tletype != atttype)
        {
          tle->expr = (Expr *)makeRelabelType(tle->expr, atttype, -1, get_typcollation(atttype), COERCE_IMPLICIT_CAST);
                                                                  
          if (tle->ressortgroupref != 0 || parse->setOperations)
          {
            *modifyTargetList = true;
          }
        }
        tle->resno = colindex;
        newtlist = lappend(newtlist, tle);
      }
    }

                                                                
    for (colindex++; colindex <= tupnatts; colindex++)
    {
      if (!TupleDescAttr(tupdesc, colindex - 1)->attisdropped)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("return type mismatch in function declared to return %s", format_type_be(rettype)), errdetail("Final statement returns too few columns.")));
      }
      if (modifyTargetList)
      {
        Expr *null_expr;

                                                            
        null_expr = (Expr *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), (Datum)0, true,             
            true            );
        newtlist = lappend(newtlist, makeTargetEntry(null_expr, colindex, NULL, false));
                                                    
        if (parse->setOperations)
        {
          *modifyTargetList = true;
        }
      }
    }

    if (modifyTargetList)
    {
                                                         
      foreach (lc, junkattrs)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(lc);

        tle->resno = colindex++;
      }
                                                   
      *tlist_ptr = list_concat(newtlist, junkattrs);
    }

                                      
    if (junkFilter)
    {
      TupleTableSlot *slot = MakeSingleTupleTableSlot(NULL, &TTSOpsMinimalTuple);

      *junkFilter = ExecInitJunkFilterConversion(tlist, CreateTupleDescCopy(tupdesc), slot);
    }

                                                          
    return true;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_FUNCTION_DEFINITION), errmsg("return type %s is not supported for SQL functions", format_type_be(rettype))));
  }

  return false;
}

   
                                                                          
   
DestReceiver *
CreateSQLFunctionDestReceiver(void)
{
  DR_sqlfunction *self = (DR_sqlfunction *)palloc0(sizeof(DR_sqlfunction));

  self->pub.receiveSlot = sqlfunction_receive;
  self->pub.rStartup = sqlfunction_startup;
  self->pub.rShutdown = sqlfunction_shutdown;
  self->pub.rDestroy = sqlfunction_destroy;
  self->pub.mydest = DestSQLFunction;

                                                    

  return (DestReceiver *)self;
}

   
                                            
   
static void
sqlfunction_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
             
}

   
                                             
   
static bool
sqlfunction_receive(TupleTableSlot *slot, DestReceiver *self)
{
  DR_sqlfunction *myState = (DR_sqlfunction *)self;

                              
  slot = ExecFilterJunk(myState->filter, slot);

                                                    
  tuplestore_puttupleslot(myState->tstore, slot);

  return true;
}

   
                                         
   
static void
sqlfunction_shutdown(DestReceiver *self)
{
             
}

   
                                                       
   
static void
sqlfunction_destroy(DestReceiver *self)
{
  pfree(self);
}
