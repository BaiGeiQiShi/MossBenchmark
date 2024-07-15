                                                                            
   
             
                                                                    
   
                                                                       
                                                   
   
   
                                                                
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "access/xact.h"
#include "catalog/pg_type.h"
#include "commands/createas.h"
#include "commands/prepare.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/analyze.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_type.h"
#include "rewrite/rewriteHandler.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

   
                                                                
                                                             
                                                                         
                                                                 
   
static HTAB *prepared_queries = NULL;

static void
InitQueryHashTable(void);
static ParamListInfo
EvaluateParams(PreparedStatement *pstmt, List *params, const char *queryString, EState *estate);
static Datum
build_regtype_array(Oid *param_types, int num_params);

   
                                               
   
void
PrepareQuery(PrepareStmt *stmt, const char *queryString, int stmt_location, int stmt_len)
{
  RawStmt *rawstmt;
  CachedPlanSource *plansource;
  Oid *argtypes = NULL;
  int nargs;
  Query *query;
  List *query_list;
  int i;

     
                                                                         
                         
     
  if (!stmt->name || stmt->name[0] == '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PSTATEMENT_DEFINITION), errmsg("invalid statement name: must not be empty")));
  }

     
                                                                          
                     
     
                                                                           
                                                                        
     
  rawstmt = makeNode(RawStmt);
  rawstmt->stmt = (Node *)copyObject(stmt->query);
  rawstmt->stmt_location = stmt_location;
  rawstmt->stmt_len = stmt_len;

     
                                                                             
                                           
     
  plansource = CreateCachedPlan(rawstmt, queryString, CreateCommandTag(stmt->query));

                                                         
  nargs = list_length(stmt->argtypes);

  if (nargs)
  {
    ParseState *pstate;
    ListCell *l;

       
                                                                           
                                                      
       
    pstate = make_parsestate(NULL);
    pstate->p_sourcetext = queryString;

    argtypes = (Oid *)palloc(nargs * sizeof(Oid));
    i = 0;

    foreach (l, stmt->argtypes)
    {
      TypeName *tn = lfirst(l);
      Oid toid = typenameTypeId(pstate, tn);

      argtypes[i++] = toid;
    }
  }

     
                                                                       
                                                                  
                                                                      
     
  query = parse_analyze_varparams(rawstmt, queryString, &argtypes, &nargs);

     
                                                     
     
  for (i = 0; i < nargs; i++)
  {
    Oid argtype = argtypes[i];

    if (argtype == InvalidOid || argtype == UNKNOWNOID)
    {
      ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_DATATYPE), errmsg("could not determine data type of parameter $%d", i + 1)));
    }
  }

     
                                                                            
     
  switch (query->commandType)
  {
  case CMD_SELECT:
  case CMD_INSERT:
  case CMD_UPDATE:
  case CMD_DELETE:
            
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_INVALID_PSTATEMENT_DEFINITION), errmsg("utility statements cannot be prepared")));
    break;
  }

                                                                     
  query_list = QueryRewrite(query);

                                              
  CompleteCachedPlan(plansource, query_list, NULL, argtypes, nargs, NULL, NULL, CURSOR_OPT_PARALLEL_OK,                          
      true);                                                                                                              

     
                       
     
  StorePreparedStatement(stmt->name, plansource, true);
}

   
                                                               
   
                                                                      
                                                                            
                                                                         
                           
   
                                                                            
                                                                        
                                                                         
                                                                       
                                           
   
void
ExecuteQuery(ExecuteStmt *stmt, IntoClause *intoClause, const char *queryString, ParamListInfo params, DestReceiver *dest, char *completionTag)
{
  PreparedStatement *entry;
  CachedPlan *cplan;
  List *plan_list;
  ParamListInfo paramLI = NULL;
  EState *estate = NULL;
  Portal portal;
  char *query_string;
  int eflags;
  long count;

                                    
  entry = FetchPreparedStatement(stmt->name, true);

                                                     
  if (!entry->plansource->fixed_result)
  {
    elog(ERROR, "EXECUTE does not support variable-result cached plans");
  }

                                   
  if (entry->plansource->num_params > 0)
  {
       
                                                                          
                                                                          
                                                                        
                    
       
    estate = CreateExecutorState();
    estate->es_param_list_info = params;
    paramLI = EvaluateParams(entry, stmt->params, queryString, estate);
  }

                                               
  portal = CreateNewPortal();
                                                                           
  portal->visible = false;

                                                                   
  query_string = MemoryContextStrdup(portal->portalContext, entry->plansource->query_string);

                                                                
  cplan = GetCachedPlan(entry->plansource, paramLI, false, NULL);
  plan_list = cplan->stmt_list;

     
                                                                     
                                                                            
     
  PortalDefineQuery(portal, NULL, query_string, entry->plansource->commandTag, plan_list, cplan);

     
                                                                       
                                                                            
                                                                       
                                                                           
                                                                             
                                                                            
                                                                         
                                                                         
     
                                                                             
                                                                 
     
  if (intoClause)
  {
    PlannedStmt *pstmt;

    if (list_length(plan_list) != 1)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("prepared statement is not a SELECT")));
    }
    pstmt = linitial_node(PlannedStmt, plan_list);
    if (pstmt->commandType != CMD_SELECT)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("prepared statement is not a SELECT")));
    }

                                
    eflags = GetIntoRelEFlags(intoClause);

                                                                
    if (intoClause->skipData)
    {
      count = 0;
    }
    else
    {
      count = FETCH_ALL;
    }
  }
  else
  {
                           
    eflags = 0;
    count = FETCH_ALL;
  }

     
                                    
     
  PortalStart(portal, paramLI, eflags, GetActiveSnapshot());

  (void)PortalRun(portal, count, false, true, dest, dest, completionTag);

  PortalDrop(portal, false);

  if (estate)
  {
    FreeExecutorState(estate);
  }

                                                                  
}

   
                                                  
   
                                                   
                                                                    
                                                
                                  
   
                                                                    
                                                                              
                           
   
static ParamListInfo
EvaluateParams(PreparedStatement *pstmt, List *params, const char *queryString, EState *estate)
{
  Oid *param_types = pstmt->plansource->param_types;
  int num_params = pstmt->plansource->num_params;
  int nparams = list_length(params);
  ParseState *pstate;
  ParamListInfo paramLI;
  List *exprstates;
  ListCell *l;
  int i;

  if (nparams != num_params)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("wrong number of parameters for prepared statement \"%s\"", pstmt->stmt_name), errdetail("Expected %d parameters but got %d.", num_params, nparams)));
  }

                                   
  if (num_params == 0)
  {
    return NULL;
  }

     
                                                                             
                                                         
     
  params = copyObject(params);

  pstate = make_parsestate(NULL);
  pstate->p_sourcetext = queryString;

  i = 0;
  foreach (l, params)
  {
    Node *expr = lfirst(l);
    Oid expected_type_id = param_types[i];
    Oid given_type_id;

    expr = transformExpr(pstate, expr, EXPR_KIND_EXECUTE_PARAMETER);

    given_type_id = exprType(expr);

    expr = coerce_to_target_type(pstate, expr, given_type_id, expected_type_id, -1, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);

    if (expr == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("parameter $%d of type %s cannot be coerced to the expected type %s", i + 1, format_type_be(given_type_id), format_type_be(expected_type_id)), errhint("You will need to rewrite or cast the expression.")));
    }

                                                             
    assign_expr_collations(pstate, expr);

    lfirst(l) = expr;
    i++;
  }

                                             
  exprstates = ExecPrepareExprList(params, estate);

  paramLI = makeParamList(num_params);

  i = 0;
  foreach (l, exprstates)
  {
    ExprState *n = (ExprState *)lfirst(l);
    ParamExternData *prm = &paramLI->params[i];

    prm->ptype = param_types[i];
    prm->pflags = PARAM_FLAG_CONST;
    prm->value = ExecEvalExprSwitchContext(n, GetPerTupleExprContext(estate), &prm->isnull);

    i++;
  }

  return paramLI;
}

   
                                               
   
static void
InitQueryHashTable(void)
{
  HASHCTL hash_ctl;

  MemSet(&hash_ctl, 0, sizeof(hash_ctl));

  hash_ctl.keysize = NAMEDATALEN;
  hash_ctl.entrysize = sizeof(PreparedStatement);

  prepared_queries = hash_create("Prepared Queries", 32, &hash_ctl, HASH_ELEM);
}

   
                                                                    
                                                                       
                                                                           
                
   
void
StorePreparedStatement(const char *stmt_name, CachedPlanSource *plansource, bool from_sql)
{
  PreparedStatement *entry;
  TimestampTz cur_ts = GetCurrentStatementStartTimestamp();
  bool found;

                                               
  if (!prepared_queries)
  {
    InitQueryHashTable();
  }

                               
  entry = (PreparedStatement *)hash_search(prepared_queries, stmt_name, HASH_ENTER, &found);

                                       
  if (found)
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_PSTATEMENT), errmsg("prepared statement \"%s\" already exists", stmt_name)));
  }

                                    
  entry->plansource = plansource;
  entry->from_sql = from_sql;
  entry->prepare_time = cur_ts;

                                                                      
  SaveCachedPlan(plansource);
}

   
                                                                     
                                                                             
   
                                                                         
                               
   
PreparedStatement *
FetchPreparedStatement(const char *stmt_name, bool throwError)
{
  PreparedStatement *entry;

     
                                                                    
                                                              
     
  if (prepared_queries)
  {
    entry = (PreparedStatement *)hash_search(prepared_queries, stmt_name, HASH_FIND, NULL);
  }
  else
  {
    entry = NULL;
  }

  if (!entry && throwError)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PSTATEMENT), errmsg("prepared statement \"%s\" does not exist", stmt_name)));
  }

  return entry;
}

   
                                                                      
                                                                   
   
                                                                      
   
TupleDesc
FetchPreparedStatementResultDesc(PreparedStatement *stmt)
{
     
                                                                          
                                                                       
     
  Assert(stmt->plansource->fixed_result);
  if (stmt->plansource->resultDesc)
  {
    return CreateTupleDescCopy(stmt->plansource->resultDesc);
  }
  else
  {
    return NULL;
  }
}

   
                                                                     
                                                                         
               
   
                                                                            
                                                                           
               
   
List *
FetchPreparedStatementTargetList(PreparedStatement *stmt)
{
  List *tlist;

                                         
  tlist = CachedPlanGetTargetList(stmt->plansource, NULL);

                                                                
  return copyObject(tlist);
}

   
                                                              
                                
   
void
DeallocateQuery(DeallocateStmt *stmt)
{
  if (stmt->name)
  {
    DropPreparedStatement(stmt->name, true);
  }
  else
  {
    DropAllPreparedStatements();
  }
}

   
                                  
   
                                                                       
   
void
DropPreparedStatement(const char *stmt_name, bool showError)
{
  PreparedStatement *entry;

                                                                
  entry = FetchPreparedStatement(stmt_name, showError);

  if (entry)
  {
                                     
    DropCachedPlan(entry->plansource);

                                                
    hash_search(prepared_queries, entry->stmt_name, HASH_REMOVE, NULL);
  }
}

   
                               
   
void
DropAllPreparedStatements(void)
{
  HASH_SEQ_STATUS seq;
  PreparedStatement *entry;

                      
  if (!prepared_queries)
  {
    return;
  }

                       
  hash_seq_init(&seq, prepared_queries);
  while ((entry = hash_seq_search(&seq)) != NULL)
  {
                                     
    DropCachedPlan(entry->plansource);

                                                
    hash_search(prepared_queries, entry->stmt_name, HASH_REMOVE, NULL);
  }
}

   
                                                       
   
                                                                       
                                                                           
   
                                                                   
                                                                          
   
void
ExplainExecuteQuery(ExecuteStmt *execstmt, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv)
{
  PreparedStatement *entry;
  const char *query_string;
  CachedPlan *cplan;
  List *plan_list;
  ListCell *p;
  ParamListInfo paramLI = NULL;
  EState *estate = NULL;
  instr_time planstart;
  instr_time planduration;

  INSTR_TIME_SET_CURRENT(planstart);

                                    
  entry = FetchPreparedStatement(execstmt->name, true);

                                                     
  if (!entry->plansource->fixed_result)
  {
    elog(ERROR, "EXPLAIN EXECUTE does not support variable-result cached plans");
  }

  query_string = entry->plansource->query_string;

                                   
  if (entry->plansource->num_params)
  {
       
                                                                          
                                                                          
                                                                        
                    
       
    estate = CreateExecutorState();
    estate->es_param_list_info = params;
    paramLI = EvaluateParams(entry, execstmt->params, queryString, estate);
  }

                                                          
  cplan = GetCachedPlan(entry->plansource, paramLI, true, queryEnv);

  INSTR_TIME_SET_CURRENT(planduration);
  INSTR_TIME_SUBTRACT(planduration, planstart);

  plan_list = cplan->stmt_list;

                          
  foreach (p, plan_list)
  {
    PlannedStmt *pstmt = lfirst_node(PlannedStmt, p);

    if (pstmt->commandType != CMD_UTILITY)
    {
      ExplainOnePlan(pstmt, into, es, query_string, paramLI, queryEnv, &planduration);
    }
    else
    {
      ExplainOneUtility(pstmt->utilityStmt, into, es, query_string, paramLI, queryEnv);
    }

                                                                       

                                                      
    if (lnext(p) != NULL)
    {
      ExplainSeparatePlans(es);
    }
  }

  if (estate)
  {
    FreeExecutorState(estate);
  }

  ReleaseCachedPlan(cplan, true);
}

   
                                                                     
                                                                            
   
Datum
pg_prepared_statement(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not "
                                                                   "allowed in this context")));
  }

                                                 
  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

     
                                                                            
                                                     
     
  tupdesc = CreateTemplateTupleDesc(5);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "name", TEXTOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)2, "statement", TEXTOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)3, "prepare_time", TIMESTAMPTZOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)4, "parameter_types", REGTYPEARRAYOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)5, "from_sql", BOOLOID, -1, 0);

     
                                                                           
                                                                             
     
  tupstore = tuplestore_begin_heap(rsinfo->allowedModes & SFRM_Materialize_Random, false, work_mem);

                                           
  MemoryContextSwitchTo(oldcontext);

                                         
  if (prepared_queries)
  {
    HASH_SEQ_STATUS hash_seq;
    PreparedStatement *prep_stmt;

    hash_seq_init(&hash_seq, prepared_queries);
    while ((prep_stmt = hash_seq_search(&hash_seq)) != NULL)
    {
      Datum values[5];
      bool nulls[5];

      MemSet(nulls, 0, sizeof(nulls));

      values[0] = CStringGetTextDatum(prep_stmt->stmt_name);
      values[1] = CStringGetTextDatum(prep_stmt->plansource->query_string);
      values[2] = TimestampTzGetDatum(prep_stmt->prepare_time);
      values[3] = build_regtype_array(prep_stmt->plansource->param_types, prep_stmt->plansource->num_params);
      values[4] = BoolGetDatum(prep_stmt->from_sql);

      tuplestore_putvalues(tupstore, tupdesc, values, nulls);
    }
  }

                                          
  tuplestore_donestoring(tupstore);

  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  return (Datum)0;
}

   
                                                                      
                                                                      
                                                        
   
static Datum
build_regtype_array(Oid *param_types, int num_params)
{
  Datum *tmp_ary;
  ArrayType *result;
  int i;

  tmp_ary = (Datum *)palloc(num_params * sizeof(Datum));

  for (i = 0; i < num_params; i++)
  {
    tmp_ary[i] = ObjectIdGetDatum(param_types[i]);
  }

                                                              
  result = construct_array(tmp_ary, num_params, REGTYPEOID, 4, true, 'i');
  return PointerGetDatum(result);
}
