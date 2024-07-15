                                                                            
   
             
                                   
   
                                                                         
                                                                          
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/xact.h"
#include "catalog/pg_type.h"
#include "commands/createas.h"
#include "commands/defrem.h"
#include "commands/prepare.h"
#include "executor/nodeHash.h"
#include "foreign/fdwapi.h"
#include "jit/jit.h"
#include "nodes/extensible.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteHandler.h"
#include "storage/bufmgr.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/guc_tables.h"
#include "utils/json.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/tuplesort.h"
#include "utils/typcache.h"
#include "utils/xml.h"

                                                          
ExplainOneQuery_hook_type ExplainOneQuery_hook = NULL;

                                                                 
explain_get_index_name_hook_type explain_get_index_name_hook = NULL;

                                       
#define X_OPENING 0
#define X_CLOSING 1
#define X_CLOSE_IMMEDIATE 2
#define X_NOWHITESPACE 4

static void
ExplainOneQuery(Query *query, int cursorOptions, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv);
static void
report_triggers(ResultRelInfo *rInfo, bool show_relname, ExplainState *es);
static double
elapsed_time(instr_time *starttime);
static bool
ExplainPreScanNode(PlanState *planstate, Bitmapset **rels_used);
static void
ExplainNode(PlanState *planstate, List *ancestors, const char *relationship, const char *plan_name, ExplainState *es);
static void
show_plan_tlist(PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_expression(Node *node, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es);
static void
show_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es);
static void
show_scan_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_upper_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_sort_keys(SortState *sortstate, List *ancestors, ExplainState *es);
static void
show_merge_append_keys(MergeAppendState *mstate, List *ancestors, ExplainState *es);
static void
show_agg_keys(AggState *astate, List *ancestors, ExplainState *es);
static void
show_grouping_sets(PlanState *planstate, Agg *agg, List *ancestors, ExplainState *es);
static void
show_grouping_set_keys(PlanState *planstate, Agg *aggnode, Sort *sortnode, List *context, bool useprefix, List *ancestors, ExplainState *es);
static void
show_group_keys(GroupState *gstate, List *ancestors, ExplainState *es);
static void
show_sort_group_keys(PlanState *planstate, const char *qlabel, int nkeys, AttrNumber *keycols, Oid *sortOperators, Oid *collations, bool *nullsFirst, List *ancestors, ExplainState *es);
static void
show_sortorder_options(StringInfo buf, Node *sortexpr, Oid sortOperator, Oid collation, bool nullsFirst);
static void
show_tablesample(TableSampleClause *tsc, PlanState *planstate, List *ancestors, ExplainState *es);
static void
show_sort_info(SortState *sortstate, ExplainState *es);
static void
show_hash_info(HashState *hashstate, ExplainState *es);
static void
show_tidbitmap_info(BitmapHeapScanState *planstate, ExplainState *es);
static void
show_instrumentation_count(const char *qlabel, int which, PlanState *planstate, ExplainState *es);
static void
show_foreignscan_info(ForeignScanState *fsstate, ExplainState *es);
static void
show_eval_params(Bitmapset *bms_params, ExplainState *es);
static const char *
explain_get_index_name(Oid indexId);
static void
show_buffer_usage(ExplainState *es, const BufferUsage *usage);
static void
ExplainIndexScanDetails(Oid indexid, ScanDirection indexorderdir, ExplainState *es);
static void
ExplainScanTarget(Scan *plan, ExplainState *es);
static void
ExplainModifyTarget(ModifyTable *plan, ExplainState *es);
static void
ExplainTargetRel(Plan *plan, Index rti, ExplainState *es);
static void
show_modifytable_info(ModifyTableState *mtstate, List *ancestors, ExplainState *es);
static void
ExplainMemberNodes(PlanState **planstates, int nplans, List *ancestors, ExplainState *es);
static void
ExplainMissingMembers(int nplans, int nchildren, ExplainState *es);
static void
ExplainSubPlans(List *plans, List *ancestors, const char *relationship, ExplainState *es);
static void
ExplainCustomChildren(CustomScanState *css, List *ancestors, ExplainState *es);
static void
ExplainProperty(const char *qlabel, const char *unit, const char *value, bool numeric, ExplainState *es);
static void
ExplainDummyGroup(const char *objtype, const char *labelname, ExplainState *es);
static void
ExplainXMLTag(const char *tagname, int flags, ExplainState *es);
static void
ExplainJSONLineEnding(ExplainState *es);
static void
ExplainYAMLLineStarting(ExplainState *es);
static void
escape_yaml(StringInfo buf, const char *str);

   
                  
                                
   
void
ExplainQuery(ParseState *pstate, ExplainStmt *stmt, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv, DestReceiver *dest)
{
  ExplainState *es = NewExplainState();
  TupOutputState *tstate;
  List *rewritten;
  ListCell *lc;
  bool timing_set = false;
  bool summary_set = false;

                           
  foreach (lc, stmt->options)
  {
    DefElem *opt = (DefElem *)lfirst(lc);

    if (strcmp(opt->defname, "analyze") == 0)
    {
      es->analyze = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "verbose") == 0)
    {
      es->verbose = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "costs") == 0)
    {
      es->costs = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "buffers") == 0)
    {
      es->buffers = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "settings") == 0)
    {
      es->settings = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "timing") == 0)
    {
      timing_set = true;
      es->timing = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "summary") == 0)
    {
      summary_set = true;
      es->summary = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "format") == 0)
    {
      char *p = defGetString(opt);

      if (strcmp(p, "text") == 0)
      {
        es->format = EXPLAIN_FORMAT_TEXT;
      }
      else if (strcmp(p, "xml") == 0)
      {
        es->format = EXPLAIN_FORMAT_XML;
      }
      else if (strcmp(p, "json") == 0)
      {
        es->format = EXPLAIN_FORMAT_JSON;
      }
      else if (strcmp(p, "yaml") == 0)
      {
        es->format = EXPLAIN_FORMAT_YAML;
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unrecognized value for EXPLAIN option \"%s\": \"%s\"", opt->defname, p), parser_errposition(pstate, opt->location)));
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unrecognized EXPLAIN option \"%s\"", opt->defname), parser_errposition(pstate, opt->location)));
    }
  }

  if (es->buffers && !es->analyze)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("EXPLAIN option BUFFERS requires ANALYZE")));
  }

                                                               
  es->timing = (timing_set) ? es->timing : es->analyze;

                                                      
  if (es->timing && !es->analyze)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("EXPLAIN option TIMING requires ANALYZE")));
  }

                                                                
  es->summary = (summary_set) ? es->summary : es->analyze;

     
                                                                        
                                                                             
                                                                       
                  
     
                                                                             
                                                                            
                                                                         
                                                                         
                                   
     
  rewritten = QueryRewrite(castNode(Query, copyObject(stmt->query)));

                                
  ExplainBeginOutput(es);

  if (rewritten == NIL)
  {
       
                                                                      
                                                                          
       
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfoString(es->str, "Query rewrites to nothing\n");
    }
  }
  else
  {
    ListCell *l;

                            
    foreach (l, rewritten)
    {
      ExplainOneQuery(lfirst_node(Query, l), CURSOR_OPT_PARALLEL_OK, NULL, es, queryString, params, queryEnv);

                                                        
      if (lnext(l) != NULL)
      {
        ExplainSeparatePlans(es);
      }
    }
  }

                                
  ExplainEndOutput(es);
  Assert(es->indent == 0);

                     
  tstate = begin_tup_output_tupdesc(dest, ExplainResultDesc(stmt), &TTSOpsVirtual);
  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    do_text_output_multiline(tstate, es->str->data);
  }
  else
  {
    do_text_output_oneline(tstate, es->str->data);
  }
  end_tup_output(tstate);

  pfree(es->str->data);
}

   
                                                                      
   
ExplainState *
NewExplainState(void)
{
  ExplainState *es = (ExplainState *)palloc0(sizeof(ExplainState));

                                                                
  es->costs = true;
                              
  es->str = makeStringInfo();

  return es;
}

   
                       
                                                   
   
TupleDesc
ExplainResultDesc(ExplainStmt *stmt)
{
  TupleDesc tupdesc;
  ListCell *lc;
  Oid result_type = TEXTOID;

                                   
  foreach (lc, stmt->options)
  {
    DefElem *opt = (DefElem *)lfirst(lc);

    if (strcmp(opt->defname, "format") == 0)
    {
      char *p = defGetString(opt);

      if (strcmp(p, "xml") == 0)
      {
        result_type = XMLOID;
      }
      else if (strcmp(p, "json") == 0)
      {
        result_type = JSONOID;
      }
      else
      {
        result_type = TEXTOID;
      }
                                                                  
    }
  }

                                                                        
  tupdesc = CreateTemplateTupleDesc(1);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "QUERY PLAN", result_type, -1, 0);
  return tupdesc;
}

   
                     
                                                
   
                                                                                
   
static void
ExplainOneQuery(Query *query, int cursorOptions, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv)
{
                                                     
  if (query->commandType == CMD_UTILITY)
  {
    ExplainOneUtility(query->utilityStmt, into, es, queryString, params, queryEnv);
    return;
  }

                                                             
  if (ExplainOneQuery_hook)
  {
    (*ExplainOneQuery_hook)(query, cursorOptions, into, es, queryString, params, queryEnv);
  }
  else
  {
    PlannedStmt *plan;
    instr_time planstart, planduration;

    INSTR_TIME_SET_CURRENT(planstart);

                        
    plan = pg_plan_query(query, cursorOptions, params);

    INSTR_TIME_SET_CURRENT(planduration);
    INSTR_TIME_SUBTRACT(planduration, planstart);

                                               
    ExplainOnePlan(plan, into, es, queryString, params, queryEnv, &planduration);
  }
}

   
                       
                                                            
                                                                          
                                
   
                                                                                
   
                                                                   
                         
   
void
ExplainOneUtility(Node *utilityStmt, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv)
{
  if (utilityStmt == NULL)
  {
    return;
  }

  if (IsA(utilityStmt, CreateTableAsStmt))
  {
       
                                                                        
                                                                        
                                                            
       
    CreateTableAsStmt *ctas = (CreateTableAsStmt *)utilityStmt;
    List *rewritten;

    rewritten = QueryRewrite(castNode(Query, copyObject(ctas->query)));
    Assert(list_length(rewritten) == 1);
    ExplainOneQuery(linitial_node(Query, rewritten), CURSOR_OPT_PARALLEL_OK, ctas->into, es, queryString, params, queryEnv);
  }
  else if (IsA(utilityStmt, DeclareCursorStmt))
  {
       
                                    
       
                                                                        
                                                                        
                                                                         
                            
       
    DeclareCursorStmt *dcs = (DeclareCursorStmt *)utilityStmt;
    List *rewritten;

    rewritten = QueryRewrite(castNode(Query, copyObject(dcs->query)));
    Assert(list_length(rewritten) == 1);
    ExplainOneQuery(linitial_node(Query, rewritten), dcs->options, NULL, es, queryString, params, queryEnv);
  }
  else if (IsA(utilityStmt, ExecuteStmt))
  {
    ExplainExecuteQuery((ExecuteStmt *)utilityStmt, into, es, queryString, params, queryEnv);
  }
  else if (IsA(utilityStmt, NotifyStmt))
  {
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfoString(es->str, "NOTIFY\n");
    }
    else
    {
      ExplainDummyGroup("Notify", NULL, es);
    }
  }
  else
  {
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfoString(es->str, "Utility statements have no plan structure\n");
    }
    else
    {
      ExplainDummyGroup("Utility Statement", NULL, es);
    }
  }
}

   
                    
                                                                
                   
   
                                                                                
                                                                           
   
                                                                   
                                                                        
               
   
void
ExplainOnePlan(PlannedStmt *plannedstmt, IntoClause *into, ExplainState *es, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv, const instr_time *planduration)
{
  DestReceiver *dest;
  QueryDesc *queryDesc;
  instr_time starttime;
  double totaltime = 0;
  int eflags;
  int instrument_option = 0;

  Assert(plannedstmt->commandType != CMD_UTILITY);

  if (es->analyze && es->timing)
  {
    instrument_option |= INSTRUMENT_TIMER;
  }
  else if (es->analyze)
  {
    instrument_option |= INSTRUMENT_ROWS;
  }

  if (es->buffers)
  {
    instrument_option |= INSTRUMENT_BUFFERS;
  }

     
                                                                             
                                                                         
                                                                    
     
  INSTR_TIME_SET_CURRENT(starttime);

     
                                                                         
                                                 
     
  PushCopiedSnapshot(GetActiveSnapshot());
  UpdateActiveSnapshotCommandId();

     
                                                                            
                                                         
     
  if (into)
  {
    dest = CreateIntoRelDestReceiver(into);
  }
  else
  {
    dest = None_Receiver;
  }

                                        
  queryDesc = CreateQueryDesc(plannedstmt, queryString, GetActiveSnapshot(), InvalidSnapshot, dest, params, queryEnv, instrument_option);

                                
  if (es->analyze)
  {
    eflags = 0;                                      
  }
  else
  {
    eflags = EXEC_FLAG_EXPLAIN_ONLY;
  }
  if (into)
  {
    eflags |= GetIntoRelEFlags(into);
  }

                                                            
  ExecutorStart(queryDesc, eflags);

                                                    
  if (es->analyze)
  {
    ScanDirection dir;

                                                               
    if (into && into->skipData)
    {
      dir = NoMovementScanDirection;
    }
    else
    {
      dir = ForwardScanDirection;
    }

                      
    ExecutorRun(queryDesc, dir, 0L, true);

                         
    ExecutorFinish(queryDesc);

                                                                         
    totaltime += elapsed_time(&starttime);
  }

  ExplainOpenGroup("Query", NULL, true, es);

                                        
  ExplainPrintPlan(es, queryDesc);

  if (es->summary && planduration)
  {
    double plantime = INSTR_TIME_GET_DOUBLE(*planduration);

    ExplainPropertyFloat("Planning Time", "ms", 1000.0 * plantime, 3, es);
  }

                                            
  if (es->analyze)
  {
    ExplainPrintTriggers(es, queryDesc);
  }

     
                                                                         
                                                                        
                                                                             
                       
     
  if (es->costs)
  {
    ExplainPrintJITSummary(es, queryDesc);
  }

     
                                                                            
                                                                  
     
  INSTR_TIME_SET_CURRENT(starttime);

  ExecutorEnd(queryDesc);

  FreeQueryDesc(queryDesc);

  PopActiveSnapshot();

                                                                   
  if (es->analyze)
  {
    CommandCounterIncrement();
  }

  totaltime += elapsed_time(&starttime);

     
                                                                          
                                                                           
                                                                             
                                                             
     
  if (es->summary && es->analyze)
  {
    ExplainPropertyFloat("Execution Time", "ms", 1000.0 * totaltime, 3, es);
  }

  ExplainCloseGroup("Query", NULL, true, es);
}

   
                          
                                                                   
   
static void
ExplainPrintSettings(ExplainState *es)
{
  int num;
  struct config_generic **gucs;

                                                            
  if (!es->settings)
  {
    return;
  }

                                             
  gucs = get_explain_guc_options(&num);

  if (es->format != EXPLAIN_FORMAT_TEXT)
  {
    ExplainOpenGroup("Settings", "Settings", true, es);

    for (int i = 0; i < num; i++)
    {
      char *setting;
      struct config_generic *conf = gucs[i];

      setting = GetConfigOptionByName(conf->name, NULL, true);

      ExplainPropertyText(conf->name, setting, es);
    }

    ExplainCloseGroup("Settings", "Settings", true, es);
  }
  else
  {
    StringInfoData str;

                                                             
    if (num <= 0)
    {
      return;
    }

    initStringInfo(&str);

    for (int i = 0; i < num; i++)
    {
      char *setting;
      struct config_generic *conf = gucs[i];

      if (i > 0)
      {
        appendStringInfoString(&str, ", ");
      }

      setting = GetConfigOptionByName(conf->name, NULL, true);

      if (setting)
      {
        appendStringInfo(&str, "%s = '%s'", conf->name, setting);
      }
      else
      {
        appendStringInfo(&str, "%s = NULL", conf->name);
      }
    }

    ExplainPropertyText("Settings", str.data, es);
  }
}

   
                      
                                                                      
   
                                                                       
                                                                          
                                                                         
                                
   
                                           
   
void
ExplainPrintPlan(ExplainState *es, QueryDesc *queryDesc)
{
  Bitmapset *rels_used = NULL;
  PlanState *ps;

                                                                 
  Assert(queryDesc->plannedstmt != NULL);
  es->pstmt = queryDesc->plannedstmt;
  es->rtable = queryDesc->plannedstmt->rtable;
  ExplainPreScanNode(queryDesc->planstate, &rels_used);
  es->rtable_names = select_rtable_names_for_explain(es->rtable, rels_used);
  es->deparse_cxt = deparse_context_for_plan_rtable(es->rtable, es->rtable_names);
  es->printed_subplans = NULL;

     
                                                                           
                                                                       
                                                                          
                                                                          
     
  ps = queryDesc->planstate;
  if (IsA(ps, GatherState) && ((Gather *)ps->plan)->invisible)
  {
    ps = outerPlanState(ps);
  }
  ExplainNode(ps, NIL, NULL, NULL, es);

     
                                                                             
                                        
     
  ExplainPrintSettings(es);
}

   
                          
                                                                       
             
   
                                                                       
                                                                    
                     
   
void
ExplainPrintTriggers(ExplainState *es, QueryDesc *queryDesc)
{
  ResultRelInfo *rInfo;
  bool show_relname;
  int numrels = queryDesc->estate->es_num_result_relations;
  int numrootrels = queryDesc->estate->es_num_root_result_relations;
  List *routerels;
  List *targrels;
  int nr;
  ListCell *l;

  routerels = queryDesc->estate->es_tuple_routing_result_relations;
  targrels = queryDesc->estate->es_trig_target_relations;

  ExplainOpenGroup("Triggers", "Triggers", false, es);

  show_relname = (numrels > 1 || numrootrels > 0 || routerels != NIL || targrels != NIL);
  rInfo = queryDesc->estate->es_result_relations;
  for (nr = 0; nr < numrels; rInfo++, nr++)
  {
    report_triggers(rInfo, show_relname, es);
  }

  rInfo = queryDesc->estate->es_root_result_relations;
  for (nr = 0; nr < numrootrels; rInfo++, nr++)
  {
    report_triggers(rInfo, show_relname, es);
  }

  foreach (l, routerels)
  {
    rInfo = (ResultRelInfo *)lfirst(l);
    report_triggers(rInfo, show_relname, es);
  }

  foreach (l, targrels)
  {
    rInfo = (ResultRelInfo *)lfirst(l);
    report_triggers(rInfo, show_relname, es);
  }

  ExplainCloseGroup("Triggers", "Triggers", false, es);
}

   
                            
                                                                   
   
void
ExplainPrintJITSummary(ExplainState *es, QueryDesc *queryDesc)
{
  JitInstrumentation ji = {0};

  if (!(queryDesc->estate->es_jit_flags & PGJIT_PERFORM))
  {
    return;
  }

     
                                                                        
                                  
     
  if (queryDesc->estate->es_jit)
  {
    InstrJitAgg(&ji, &queryDesc->estate->es_jit->instr);
  }

                                                                     
  if (queryDesc->estate->es_jit_worker_instr)
  {
    InstrJitAgg(&ji, queryDesc->estate->es_jit_worker_instr);
  }

  ExplainPrintJIT(es, queryDesc->estate->es_jit_flags, &ji, -1);
}

   
                     
                                                 
   
                                                                             
                                                        
   
void
ExplainPrintJIT(ExplainState *es, int jit_flags, JitInstrumentation *ji, int worker_num)
{
  instr_time total_time;
  bool for_workers = (worker_num >= 0);

                                                     
  if (!ji || ji->created_functions == 0)
  {
    return;
  }

                            
  INSTR_TIME_SET_ZERO(total_time);
  INSTR_TIME_ADD(total_time, ji->generation_counter);
  INSTR_TIME_ADD(total_time, ji->inlining_counter);
  INSTR_TIME_ADD(total_time, ji->optimization_counter);
  INSTR_TIME_ADD(total_time, ji->emission_counter);

  ExplainOpenGroup("JIT", "JIT", true, es);

                                                            
  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    appendStringInfoSpaces(es->str, es->indent * 2);
    if (for_workers)
    {
      appendStringInfo(es->str, "JIT for worker %u:\n", worker_num);
    }
    else
    {
      appendStringInfo(es->str, "JIT:\n");
    }
    es->indent += 1;

    ExplainPropertyInteger("Functions", NULL, ji->created_functions, es);

    appendStringInfoSpaces(es->str, es->indent * 2);
    appendStringInfo(es->str, "Options: %s %s, %s %s, %s %s, %s %s\n", "Inlining", jit_flags & PGJIT_INLINE ? "true" : "false", "Optimization", jit_flags & PGJIT_OPT3 ? "true" : "false", "Expressions", jit_flags & PGJIT_EXPR ? "true" : "false", "Deforming", jit_flags & PGJIT_DEFORM ? "true" : "false");

    if (es->analyze && es->timing)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfo(es->str, "Timing: %s %.3f ms, %s %.3f ms, %s %.3f ms, %s %.3f ms, %s %.3f ms\n", "Generation", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->generation_counter), "Inlining", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->inlining_counter), "Optimization", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->optimization_counter), "Emission", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->emission_counter), "Total", 1000.0 * INSTR_TIME_GET_DOUBLE(total_time));
    }

    es->indent -= 1;
  }
  else
  {
    ExplainPropertyInteger("Worker Number", NULL, worker_num, es);
    ExplainPropertyInteger("Functions", NULL, ji->created_functions, es);

    ExplainOpenGroup("Options", "Options", true, es);
    ExplainPropertyBool("Inlining", jit_flags & PGJIT_INLINE, es);
    ExplainPropertyBool("Optimization", jit_flags & PGJIT_OPT3, es);
    ExplainPropertyBool("Expressions", jit_flags & PGJIT_EXPR, es);
    ExplainPropertyBool("Deforming", jit_flags & PGJIT_DEFORM, es);
    ExplainCloseGroup("Options", "Options", true, es);

    if (es->analyze && es->timing)
    {
      ExplainOpenGroup("Timing", "Timing", true, es);

      ExplainPropertyFloat("Generation", "ms", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->generation_counter), 3, es);
      ExplainPropertyFloat("Inlining", "ms", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->inlining_counter), 3, es);
      ExplainPropertyFloat("Optimization", "ms", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->optimization_counter), 3, es);
      ExplainPropertyFloat("Emission", "ms", 1000.0 * INSTR_TIME_GET_DOUBLE(ji->emission_counter), 3, es);
      ExplainPropertyFloat("Total", "ms", 1000.0 * INSTR_TIME_GET_DOUBLE(total_time), 3, es);

      ExplainCloseGroup("Timing", "Timing", true, es);
    }
  }

  ExplainCloseGroup("JIT", "JIT", true, es);
}

   
                      
                                                                        
   
                                                                       
                                           
   
   
void
ExplainQueryText(ExplainState *es, QueryDesc *queryDesc)
{
  if (queryDesc->sourceText)
  {
    ExplainPropertyText("Query Text", queryDesc->sourceText, es);
  }
}

   
                     
                                                            
   
static void
report_triggers(ResultRelInfo *rInfo, bool show_relname, ExplainState *es)
{
  int nt;

  if (!rInfo->ri_TrigDesc || !rInfo->ri_TrigInstrument)
  {
    return;
  }
  for (nt = 0; nt < rInfo->ri_TrigDesc->numtriggers; nt++)
  {
    Trigger *trig = rInfo->ri_TrigDesc->triggers + nt;
    Instrumentation *instr = rInfo->ri_TrigInstrument + nt;
    char *relname;
    char *conname = NULL;

                                             
    InstrEndLoop(instr);

       
                                                                      
                                           
       
    if (instr->ntuples == 0)
    {
      continue;
    }

    ExplainOpenGroup("Trigger", NULL, true, es);

    relname = RelationGetRelationName(rInfo->ri_RelationDesc);
    if (OidIsValid(trig->tgconstraint))
    {
      conname = get_constraint_name(trig->tgconstraint);
    }

       
                                                                       
                                                                         
                                 
       
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      if (es->verbose || conname == NULL)
      {
        appendStringInfo(es->str, "Trigger %s", trig->tgname);
      }
      else
      {
        appendStringInfoString(es->str, "Trigger");
      }
      if (conname)
      {
        appendStringInfo(es->str, " for constraint %s", conname);
      }
      if (show_relname)
      {
        appendStringInfo(es->str, " on %s", relname);
      }
      if (es->timing)
      {
        appendStringInfo(es->str, ": time=%.3f calls=%.0f\n", 1000.0 * instr->total, instr->ntuples);
      }
      else
      {
        appendStringInfo(es->str, ": calls=%.0f\n", instr->ntuples);
      }
    }
    else
    {
      ExplainPropertyText("Trigger Name", trig->tgname, es);
      if (conname)
      {
        ExplainPropertyText("Constraint Name", conname, es);
      }
      ExplainPropertyText("Relation", relname, es);
      if (es->timing)
      {
        ExplainPropertyFloat("Time", "ms", 1000.0 * instr->total, 3, es);
      }
      ExplainPropertyFloat("Calls", NULL, instr->ntuples, 0, es);
    }

    if (conname)
    {
      pfree(conname);
    }

    ExplainCloseGroup("Trigger", NULL, true, es);
  }
}

                                                           
static double
elapsed_time(instr_time *starttime)
{
  instr_time endtime;

  INSTR_TIME_SET_CURRENT(endtime);
  INSTR_TIME_SUBTRACT(endtime, *starttime);
  return INSTR_TIME_GET_DOUBLE(endtime);
}

   
                        
                                                                      
   
                                                                             
                                                                       
                                                                             
                                                                          
   
static bool
ExplainPreScanNode(PlanState *planstate, Bitmapset **rels_used)
{
  Plan *plan = planstate->plan;

  switch (nodeTag(plan))
  {
  case T_SeqScan:
  case T_SampleScan:
  case T_IndexScan:
  case T_IndexOnlyScan:
  case T_BitmapHeapScan:
  case T_TidScan:
  case T_SubqueryScan:
  case T_FunctionScan:
  case T_TableFuncScan:
  case T_ValuesScan:
  case T_CteScan:
  case T_NamedTuplestoreScan:
  case T_WorkTableScan:
    *rels_used = bms_add_member(*rels_used, ((Scan *)plan)->scanrelid);
    break;
  case T_ForeignScan:
    *rels_used = bms_add_members(*rels_used, ((ForeignScan *)plan)->fs_relids);
    break;
  case T_CustomScan:
    *rels_used = bms_add_members(*rels_used, ((CustomScan *)plan)->custom_relids);
    break;
  case T_ModifyTable:
    *rels_used = bms_add_member(*rels_used, ((ModifyTable *)plan)->nominalRelation);
    if (((ModifyTable *)plan)->exclRelRTI)
    {
      *rels_used = bms_add_member(*rels_used, ((ModifyTable *)plan)->exclRelRTI);
    }
    break;
  default:
    break;
  }

  return planstate_tree_walker(planstate, ExplainPreScanNode, rels_used);
}

   
                 
                                                     
   
                                                                          
                                                                            
                                                                             
   
                                                                             
                                                             
   
                                                                           
                                                                         
                                             
   
                                                                           
                                                                               
                                                                            
                                                        
   
static void
ExplainNode(PlanState *planstate, List *ancestors, const char *relationship, const char *plan_name, ExplainState *es)
{
  Plan *plan = planstate->plan;
  const char *pname;                                     
  const char *sname;                                         
  const char *strategy = NULL;
  const char *partialmode = NULL;
  const char *operation = NULL;
  const char *custom_name = NULL;
  int save_indent = es->indent;
  bool haschildren;

  switch (nodeTag(plan))
  {
  case T_Result:
    pname = sname = "Result";
    break;
  case T_ProjectSet:
    pname = sname = "ProjectSet";
    break;
  case T_ModifyTable:
    sname = "ModifyTable";
    switch (((ModifyTable *)plan)->operation)
    {
    case CMD_INSERT:
      pname = operation = "Insert";
      break;
    case CMD_UPDATE:
      pname = operation = "Update";
      break;
    case CMD_DELETE:
      pname = operation = "Delete";
      break;
    default:
      pname = "???";
      break;
    }
    break;
  case T_Append:
    pname = sname = "Append";
    break;
  case T_MergeAppend:
    pname = sname = "Merge Append";
    break;
  case T_RecursiveUnion:
    pname = sname = "Recursive Union";
    break;
  case T_BitmapAnd:
    pname = sname = "BitmapAnd";
    break;
  case T_BitmapOr:
    pname = sname = "BitmapOr";
    break;
  case T_NestLoop:
    pname = sname = "Nested Loop";
    break;
  case T_MergeJoin:
    pname = "Merge";                                           
    sname = "Merge Join";
    break;
  case T_HashJoin:
    pname = "Hash";                                           
    sname = "Hash Join";
    break;
  case T_SeqScan:
    pname = sname = "Seq Scan";
    break;
  case T_SampleScan:
    pname = sname = "Sample Scan";
    break;
  case T_Gather:
    pname = sname = "Gather";
    break;
  case T_GatherMerge:
    pname = sname = "Gather Merge";
    break;
  case T_IndexScan:
    pname = sname = "Index Scan";
    break;
  case T_IndexOnlyScan:
    pname = sname = "Index Only Scan";
    break;
  case T_BitmapIndexScan:
    pname = sname = "Bitmap Index Scan";
    break;
  case T_BitmapHeapScan:
    pname = sname = "Bitmap Heap Scan";
    break;
  case T_TidScan:
    pname = sname = "Tid Scan";
    break;
  case T_SubqueryScan:
    pname = sname = "Subquery Scan";
    break;
  case T_FunctionScan:
    pname = sname = "Function Scan";
    break;
  case T_TableFuncScan:
    pname = sname = "Table Function Scan";
    break;
  case T_ValuesScan:
    pname = sname = "Values Scan";
    break;
  case T_CteScan:
    pname = sname = "CTE Scan";
    break;
  case T_NamedTuplestoreScan:
    pname = sname = "Named Tuplestore Scan";
    break;
  case T_WorkTableScan:
    pname = sname = "WorkTable Scan";
    break;
  case T_ForeignScan:
    sname = "Foreign Scan";
    switch (((ForeignScan *)plan)->operation)
    {
    case CMD_SELECT:
      pname = "Foreign Scan";
      operation = "Select";
      break;
    case CMD_INSERT:
      pname = "Foreign Insert";
      operation = "Insert";
      break;
    case CMD_UPDATE:
      pname = "Foreign Update";
      operation = "Update";
      break;
    case CMD_DELETE:
      pname = "Foreign Delete";
      operation = "Delete";
      break;
    default:
      pname = "???";
      break;
    }
    break;
  case T_CustomScan:
    sname = "Custom Scan";
    custom_name = ((CustomScan *)plan)->methods->CustomName;
    if (custom_name)
    {
      pname = psprintf("Custom Scan (%s)", custom_name);
    }
    else
    {
      pname = sname;
    }
    break;
  case T_Material:
    pname = sname = "Materialize";
    break;
  case T_Sort:
    pname = sname = "Sort";
    break;
  case T_Group:
    pname = sname = "Group";
    break;
  case T_Agg:
  {
    Agg *agg = (Agg *)plan;

    sname = "Aggregate";
    switch (agg->aggstrategy)
    {
    case AGG_PLAIN:
      pname = "Aggregate";
      strategy = "Plain";
      break;
    case AGG_SORTED:
      pname = "GroupAggregate";
      strategy = "Sorted";
      break;
    case AGG_HASHED:
      pname = "HashAggregate";
      strategy = "Hashed";
      break;
    case AGG_MIXED:
      pname = "MixedAggregate";
      strategy = "Mixed";
      break;
    default:
      pname = "Aggregate ???";
      strategy = "???";
      break;
    }

    if (DO_AGGSPLIT_SKIPFINAL(agg->aggsplit))
    {
      partialmode = "Partial";
      pname = psprintf("%s %s", partialmode, pname);
    }
    else if (DO_AGGSPLIT_COMBINE(agg->aggsplit))
    {
      partialmode = "Finalize";
      pname = psprintf("%s %s", partialmode, pname);
    }
    else
    {
      partialmode = "Simple";
    }
  }
  break;
  case T_WindowAgg:
    pname = sname = "WindowAgg";
    break;
  case T_Unique:
    pname = sname = "Unique";
    break;
  case T_SetOp:
    sname = "SetOp";
    switch (((SetOp *)plan)->strategy)
    {
    case SETOP_SORTED:
      pname = "SetOp";
      strategy = "Sorted";
      break;
    case SETOP_HASHED:
      pname = "HashSetOp";
      strategy = "Hashed";
      break;
    default:
      pname = "SetOp ???";
      strategy = "???";
      break;
    }
    break;
  case T_LockRows:
    pname = sname = "LockRows";
    break;
  case T_Limit:
    pname = sname = "Limit";
    break;
  case T_Hash:
    pname = sname = "Hash";
    break;
  default:
    pname = sname = "???";
    break;
  }

  ExplainOpenGroup("Plan", relationship ? NULL : "Plan", true, es);

  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    if (plan_name)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfo(es->str, "%s\n", plan_name);
      es->indent++;
    }
    if (es->indent)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfoString(es->str, "->  ");
      es->indent += 2;
    }
    if (plan->parallel_aware)
    {
      appendStringInfoString(es->str, "Parallel ");
    }
    appendStringInfoString(es->str, pname);
    es->indent++;
  }
  else
  {
    ExplainPropertyText("Node Type", sname, es);
    if (strategy)
    {
      ExplainPropertyText("Strategy", strategy, es);
    }
    if (partialmode)
    {
      ExplainPropertyText("Partial Mode", partialmode, es);
    }
    if (operation)
    {
      ExplainPropertyText("Operation", operation, es);
    }
    if (relationship)
    {
      ExplainPropertyText("Parent Relationship", relationship, es);
    }
    if (plan_name)
    {
      ExplainPropertyText("Subplan Name", plan_name, es);
    }
    if (custom_name)
    {
      ExplainPropertyText("Custom Plan Provider", custom_name, es);
    }
    ExplainPropertyBool("Parallel Aware", plan->parallel_aware, es);
  }

  switch (nodeTag(plan))
  {
  case T_SeqScan:
  case T_SampleScan:
  case T_BitmapHeapScan:
  case T_TidScan:
  case T_SubqueryScan:
  case T_FunctionScan:
  case T_TableFuncScan:
  case T_ValuesScan:
  case T_CteScan:
  case T_WorkTableScan:
    ExplainScanTarget((Scan *)plan, es);
    break;
  case T_ForeignScan:
  case T_CustomScan:
    if (((Scan *)plan)->scanrelid > 0)
    {
      ExplainScanTarget((Scan *)plan, es);
    }
    break;
  case T_IndexScan:
  {
    IndexScan *indexscan = (IndexScan *)plan;

    ExplainIndexScanDetails(indexscan->indexid, indexscan->indexorderdir, es);
    ExplainScanTarget((Scan *)indexscan, es);
  }
  break;
  case T_IndexOnlyScan:
  {
    IndexOnlyScan *indexonlyscan = (IndexOnlyScan *)plan;

    ExplainIndexScanDetails(indexonlyscan->indexid, indexonlyscan->indexorderdir, es);
    ExplainScanTarget((Scan *)indexonlyscan, es);
  }
  break;
  case T_BitmapIndexScan:
  {
    BitmapIndexScan *bitmapindexscan = (BitmapIndexScan *)plan;
    const char *indexname = explain_get_index_name(bitmapindexscan->indexid);

    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfo(es->str, " on %s", quote_identifier(indexname));
    }
    else
    {
      ExplainPropertyText("Index Name", indexname, es);
    }
  }
  break;
  case T_ModifyTable:
    ExplainModifyTarget((ModifyTable *)plan, es);
    break;
  case T_NestLoop:
  case T_MergeJoin:
  case T_HashJoin:
  {
    const char *jointype;

    switch (((Join *)plan)->jointype)
    {
    case JOIN_INNER:
      jointype = "Inner";
      break;
    case JOIN_LEFT:
      jointype = "Left";
      break;
    case JOIN_FULL:
      jointype = "Full";
      break;
    case JOIN_RIGHT:
      jointype = "Right";
      break;
    case JOIN_SEMI:
      jointype = "Semi";
      break;
    case JOIN_ANTI:
      jointype = "Anti";
      break;
    default:
      jointype = "???";
      break;
    }
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
         
                                                               
                                    
         
      if (((Join *)plan)->jointype != JOIN_INNER)
      {
        appendStringInfo(es->str, " %s Join", jointype);
      }
      else if (!IsA(plan, NestLoop))
      {
        appendStringInfoString(es->str, " Join");
      }
    }
    else
    {
      ExplainPropertyText("Join Type", jointype, es);
    }
  }
  break;
  case T_SetOp:
  {
    const char *setopcmd;

    switch (((SetOp *)plan)->cmd)
    {
    case SETOPCMD_INTERSECT:
      setopcmd = "Intersect";
      break;
    case SETOPCMD_INTERSECT_ALL:
      setopcmd = "Intersect All";
      break;
    case SETOPCMD_EXCEPT:
      setopcmd = "Except";
      break;
    case SETOPCMD_EXCEPT_ALL:
      setopcmd = "Except All";
      break;
    default:
      setopcmd = "???";
      break;
    }
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfo(es->str, " %s", setopcmd);
    }
    else
    {
      ExplainPropertyText("Command", setopcmd, es);
    }
  }
  break;
  default:
    break;
  }

  if (es->costs)
  {
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfo(es->str, "  (cost=%.2f..%.2f rows=%.0f width=%d)", plan->startup_cost, plan->total_cost, plan->plan_rows, plan->plan_width);
    }
    else
    {
      ExplainPropertyFloat("Startup Cost", NULL, plan->startup_cost, 2, es);
      ExplainPropertyFloat("Total Cost", NULL, plan->total_cost, 2, es);
      ExplainPropertyFloat("Plan Rows", NULL, plan->plan_rows, 0, es);
      ExplainPropertyInteger("Plan Width", NULL, plan->plan_width, es);
    }
  }

     
                                                                       
                                                              
     
                                                                         
                                                                         
                                                                     
                                                                          
                                       
     
  if (planstate->instrument)
  {
    InstrEndLoop(planstate->instrument);
  }

  if (es->analyze && planstate->instrument && planstate->instrument->nloops > 0)
  {
    double nloops = planstate->instrument->nloops;
    double startup_ms = 1000.0 * planstate->instrument->startup / nloops;
    double total_ms = 1000.0 * planstate->instrument->total / nloops;
    double rows = planstate->instrument->ntuples / nloops;

    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      if (es->timing)
      {
        appendStringInfo(es->str, " (actual time=%.3f..%.3f rows=%.0f loops=%.0f)", startup_ms, total_ms, rows, nloops);
      }
      else
      {
        appendStringInfo(es->str, " (actual rows=%.0f loops=%.0f)", rows, nloops);
      }
    }
    else
    {
      if (es->timing)
      {
        ExplainPropertyFloat("Actual Startup Time", "s", startup_ms, 3, es);
        ExplainPropertyFloat("Actual Total Time", "s", total_ms, 3, es);
      }
      ExplainPropertyFloat("Actual Rows", NULL, rows, 0, es);
      ExplainPropertyFloat("Actual Loops", NULL, nloops, 0, es);
    }
  }
  else if (es->analyze)
  {
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfoString(es->str, " (never executed)");
    }
    else
    {
      if (es->timing)
      {
        ExplainPropertyFloat("Actual Startup Time", "ms", 0.0, 3, es);
        ExplainPropertyFloat("Actual Total Time", "ms", 0.0, 3, es);
      }
      ExplainPropertyFloat("Actual Rows", NULL, 0.0, 0, es);
      ExplainPropertyFloat("Actual Loops", NULL, 0.0, 0, es);
    }
  }

                                            
  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    appendStringInfoChar(es->str, '\n');
  }

                   
  if (es->verbose)
  {
    show_plan_tlist(planstate, ancestors, es);
  }

                   
  switch (nodeTag(plan))
  {
  case T_NestLoop:
  case T_MergeJoin:
  case T_HashJoin:
                                                          
    if (es->format != EXPLAIN_FORMAT_TEXT || (es->verbose && ((Join *)plan)->inner_unique))
    {
      ExplainPropertyBool("Inner Unique", ((Join *)plan)->inner_unique, es);
    }
    break;
  default:
    break;
  }

                             
  switch (nodeTag(plan))
  {
  case T_IndexScan:
    show_scan_qual(((IndexScan *)plan)->indexqualorig, "Index Cond", planstate, ancestors, es);
    if (((IndexScan *)plan)->indexqualorig)
    {
      show_instrumentation_count("Rows Removed by Index Recheck", 2, planstate, es);
    }
    show_scan_qual(((IndexScan *)plan)->indexorderbyorig, "Order By", planstate, ancestors, es);
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    break;
  case T_IndexOnlyScan:
    show_scan_qual(((IndexOnlyScan *)plan)->indexqual, "Index Cond", planstate, ancestors, es);
    if (((IndexOnlyScan *)plan)->recheckqual)
    {
      show_instrumentation_count("Rows Removed by Index Recheck", 2, planstate, es);
    }
    show_scan_qual(((IndexOnlyScan *)plan)->indexorderby, "Order By", planstate, ancestors, es);
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    if (es->analyze)
    {
      ExplainPropertyFloat("Heap Fetches", NULL, planstate->instrument->ntuples2, 0, es);
    }
    break;
  case T_BitmapIndexScan:
    show_scan_qual(((BitmapIndexScan *)plan)->indexqualorig, "Index Cond", planstate, ancestors, es);
    break;
  case T_BitmapHeapScan:
    show_scan_qual(((BitmapHeapScan *)plan)->bitmapqualorig, "Recheck Cond", planstate, ancestors, es);
    if (((BitmapHeapScan *)plan)->bitmapqualorig)
    {
      show_instrumentation_count("Rows Removed by Index Recheck", 2, planstate, es);
    }
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    if (es->analyze)
    {
      show_tidbitmap_info((BitmapHeapScanState *)planstate, es);
    }
    break;
  case T_SampleScan:
    show_tablesample(((SampleScan *)plan)->tablesample, planstate, ancestors, es);
                                                                     
                     
  case T_SeqScan:
  case T_ValuesScan:
  case T_CteScan:
  case T_NamedTuplestoreScan:
  case T_WorkTableScan:
  case T_SubqueryScan:
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    break;
  case T_Gather:
  {
    Gather *gather = (Gather *)plan;

    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    ExplainPropertyInteger("Workers Planned", NULL, gather->num_workers, es);

                                              
    if (gather->initParam)
    {
      show_eval_params(gather->initParam, es);
    }

    if (es->analyze)
    {
      int nworkers;

      nworkers = ((GatherState *)planstate)->nworkers_launched;
      ExplainPropertyInteger("Workers Launched", NULL, nworkers, es);
    }

       
                                                                 
                                                                   
       
    if (es->costs && es->verbose && outerPlanState(planstate)->worker_jit_instrument)
    {
      PlanState *child = outerPlanState(planstate);
      int n;
      SharedJitInstrumentation *w = child->worker_jit_instrument;

      for (n = 0; n < w->num_workers; ++n)
      {
        ExplainPrintJIT(es, child->state->es_jit_flags, &w->jit_instr[n], n);
      }
    }

    if (gather->single_copy || es->format != EXPLAIN_FORMAT_TEXT)
    {
      ExplainPropertyBool("Single Copy", gather->single_copy, es);
    }
  }
  break;
  case T_GatherMerge:
  {
    GatherMerge *gm = (GatherMerge *)plan;

    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    ExplainPropertyInteger("Workers Planned", NULL, gm->num_workers, es);

                                                    
    if (gm->initParam)
    {
      show_eval_params(gm->initParam, es);
    }

    if (es->analyze)
    {
      int nworkers;

      nworkers = ((GatherMergeState *)planstate)->nworkers_launched;
      ExplainPropertyInteger("Workers Launched", NULL, nworkers, es);
    }
  }
  break;
  case T_FunctionScan:
    if (es->verbose)
    {
      List *fexprs = NIL;
      ListCell *lc;

      foreach (lc, ((FunctionScan *)plan)->functions)
      {
        RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);

        fexprs = lappend(fexprs, rtfunc->funcexpr);
      }
                                                                 
      show_expression((Node *)fexprs, "Function Call", planstate, ancestors, es->verbose, es);
    }
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    break;
  case T_TableFuncScan:
    if (es->verbose)
    {
      TableFunc *tablefunc = ((TableFuncScan *)plan)->tablefunc;

      show_expression((Node *)tablefunc, "Table Function Call", planstate, ancestors, es->verbose, es);
    }
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    break;
  case T_TidScan:
  {
       
                                                                 
                           
       
    List *tidquals = ((TidScan *)plan)->tidquals;

    if (list_length(tidquals) > 1)
    {
      tidquals = list_make1(make_orclause(tidquals));
    }
    show_scan_qual(tidquals, "TID Cond", planstate, ancestors, es);
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
  }
  break;
  case T_ForeignScan:
    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    show_foreignscan_info((ForeignScanState *)planstate, es);
    break;
  case T_CustomScan:
  {
    CustomScanState *css = (CustomScanState *)planstate;

    show_scan_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    if (css->methods->ExplainCustomScan)
    {
      css->methods->ExplainCustomScan(css, ancestors, es);
    }
  }
  break;
  case T_NestLoop:
    show_upper_qual(((NestLoop *)plan)->join.joinqual, "Join Filter", planstate, ancestors, es);
    if (((NestLoop *)plan)->join.joinqual)
    {
      show_instrumentation_count("Rows Removed by Join Filter", 1, planstate, es);
    }
    show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 2, planstate, es);
    }
    break;
  case T_MergeJoin:
    show_upper_qual(((MergeJoin *)plan)->mergeclauses, "Merge Cond", planstate, ancestors, es);
    show_upper_qual(((MergeJoin *)plan)->join.joinqual, "Join Filter", planstate, ancestors, es);
    if (((MergeJoin *)plan)->join.joinqual)
    {
      show_instrumentation_count("Rows Removed by Join Filter", 1, planstate, es);
    }
    show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 2, planstate, es);
    }
    break;
  case T_HashJoin:
    show_upper_qual(((HashJoin *)plan)->hashclauses, "Hash Cond", planstate, ancestors, es);
    show_upper_qual(((HashJoin *)plan)->join.joinqual, "Join Filter", planstate, ancestors, es);
    if (((HashJoin *)plan)->join.joinqual)
    {
      show_instrumentation_count("Rows Removed by Join Filter", 1, planstate, es);
    }
    show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 2, planstate, es);
    }
    break;
  case T_Agg:
    show_agg_keys(castNode(AggState, planstate), ancestors, es);
    show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    break;
  case T_Group:
    show_group_keys(castNode(GroupState, planstate), ancestors, es);
    show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    break;
  case T_Sort:
    show_sort_keys(castNode(SortState, planstate), ancestors, es);
    show_sort_info(castNode(SortState, planstate), es);
    break;
  case T_MergeAppend:
    show_merge_append_keys(castNode(MergeAppendState, planstate), ancestors, es);
    break;
  case T_Result:
    show_upper_qual((List *)((Result *)plan)->resconstantqual, "One-Time Filter", planstate, ancestors, es);
    show_upper_qual(plan->qual, "Filter", planstate, ancestors, es);
    if (plan->qual)
    {
      show_instrumentation_count("Rows Removed by Filter", 1, planstate, es);
    }
    break;
  case T_ModifyTable:
    show_modifytable_info(castNode(ModifyTableState, planstate), ancestors, es);
    break;
  case T_Hash:
    show_hash_info(castNode(HashState, planstate), es);
    break;
  default:
    break;
  }

                         
  if (es->buffers && planstate->instrument)
  {
    show_buffer_usage(es, &planstate->instrument->bufusage);
  }

                          
  if (es->analyze && es->verbose && planstate->worker_instrument)
  {
    WorkerInstrumentation *w = planstate->worker_instrument;
    bool opened_group = false;
    int n;

    for (n = 0; n < w->num_workers; ++n)
    {
      Instrumentation *instrument = &w->instrument[n];
      double nloops = instrument->nloops;
      double startup_ms;
      double total_ms;
      double rows;

      if (nloops <= 0)
      {
        continue;
      }
      startup_ms = 1000.0 * instrument->startup / nloops;
      total_ms = 1000.0 * instrument->total / nloops;
      rows = instrument->ntuples / nloops;

      if (es->format == EXPLAIN_FORMAT_TEXT)
      {
        appendStringInfoSpaces(es->str, es->indent * 2);
        appendStringInfo(es->str, "Worker %d: ", n);
        if (es->timing)
        {
          appendStringInfo(es->str, "actual time=%.3f..%.3f rows=%.0f loops=%.0f\n", startup_ms, total_ms, rows, nloops);
        }
        else
        {
          appendStringInfo(es->str, "actual rows=%.0f loops=%.0f\n", rows, nloops);
        }
        es->indent++;
        if (es->buffers)
        {
          show_buffer_usage(es, &instrument->bufusage);
        }
        es->indent--;
      }
      else
      {
        if (!opened_group)
        {
          ExplainOpenGroup("Workers", "Workers", false, es);
          opened_group = true;
        }
        ExplainOpenGroup("Worker", NULL, true, es);
        ExplainPropertyInteger("Worker Number", NULL, n, es);

        if (es->timing)
        {
          ExplainPropertyFloat("Actual Startup Time", "ms", startup_ms, 3, es);
          ExplainPropertyFloat("Actual Total Time", "ms", total_ms, 3, es);
        }
        ExplainPropertyFloat("Actual Rows", NULL, rows, 0, es);
        ExplainPropertyFloat("Actual Loops", NULL, nloops, 0, es);

        if (es->buffers)
        {
          show_buffer_usage(es, &instrument->bufusage);
        }

        ExplainCloseGroup("Worker", NULL, true, es);
      }
    }

    if (opened_group)
    {
      ExplainCloseGroup("Workers", "Workers", false, es);
    }
  }

     
                                                                       
                                                                            
                                                                          
                                                                        
                                                                            
                                                                             
     
  switch (nodeTag(plan))
  {
  case T_Append:
    ExplainMissingMembers(((AppendState *)planstate)->as_nplans, list_length(((Append *)plan)->appendplans), es);
    break;
  case T_MergeAppend:
    ExplainMissingMembers(((MergeAppendState *)planstate)->ms_nplans, list_length(((MergeAppend *)plan)->mergeplans), es);
    break;
  default:
    break;
  }

                                            
  haschildren = planstate->initPlan || outerPlanState(planstate) || innerPlanState(planstate) || IsA(plan, ModifyTable) || IsA(plan, Append) || IsA(plan, MergeAppend) || IsA(plan, BitmapAnd) || IsA(plan, BitmapOr) || IsA(plan, SubqueryScan) || (IsA(planstate, CustomScanState) && ((CustomScanState *)planstate)->custom_ps != NIL) || planstate->subPlan;
  if (haschildren)
  {
    ExplainOpenGroup("Plans", "Plans", false, es);
                                                                       
    ancestors = lcons(planstate, ancestors);
  }

                  
  if (planstate->initPlan)
  {
    ExplainSubPlans(planstate->initPlan, ancestors, "InitPlan", es);
  }

                
  if (outerPlanState(planstate))
  {
    ExplainNode(outerPlanState(planstate), ancestors, "Outer", NULL, es);
  }

                 
  if (innerPlanState(planstate))
  {
    ExplainNode(innerPlanState(planstate), ancestors, "Inner", NULL, es);
  }

                           
  switch (nodeTag(plan))
  {
  case T_ModifyTable:
    ExplainMemberNodes(((ModifyTableState *)planstate)->mt_plans, ((ModifyTableState *)planstate)->mt_nplans, ancestors, es);
    break;
  case T_Append:
    ExplainMemberNodes(((AppendState *)planstate)->appendplans, ((AppendState *)planstate)->as_nplans, ancestors, es);
    break;
  case T_MergeAppend:
    ExplainMemberNodes(((MergeAppendState *)planstate)->mergeplans, ((MergeAppendState *)planstate)->ms_nplans, ancestors, es);
    break;
  case T_BitmapAnd:
    ExplainMemberNodes(((BitmapAndState *)planstate)->bitmapplans, ((BitmapAndState *)planstate)->nplans, ancestors, es);
    break;
  case T_BitmapOr:
    ExplainMemberNodes(((BitmapOrState *)planstate)->bitmapplans, ((BitmapOrState *)planstate)->nplans, ancestors, es);
    break;
  case T_SubqueryScan:
    ExplainNode(((SubqueryScanState *)planstate)->subplan, ancestors, "Subquery", NULL, es);
    break;
  case T_CustomScan:
    ExplainCustomChildren((CustomScanState *)planstate, ancestors, es);
    break;
  default:
    break;
  }

                 
  if (planstate->subPlan)
  {
    ExplainSubPlans(planstate->subPlan, ancestors, "SubPlan", es);
  }

                          
  if (haschildren)
  {
    ancestors = list_delete_first(ancestors);
    ExplainCloseGroup("Plans", "Plans", false, es);
  }

                                                          
  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    es->indent = save_indent;
  }

  ExplainCloseGroup("Plan", relationship ? NULL : "Plan", true, es);
}

   
                                      
   
static void
show_plan_tlist(PlanState *planstate, List *ancestors, ExplainState *es)
{
  Plan *plan = planstate->plan;
  List *context;
  List *result = NIL;
  bool useprefix;
  ListCell *lc;

                                                                    
  if (plan->targetlist == NIL)
  {
    return;
  }
                                                                 
  if (IsA(plan, Append))
  {
    return;
  }
                                                   
  if (IsA(plan, MergeAppend))
  {
    return;
  }
  if (IsA(plan, RecursiveUnion))
  {
    return;
  }

     
                                                                          
     
                                                                            
                                                                         
                                                                          
                                                                            
                                                                            
                                   
     
  if (IsA(plan, ForeignScan) && ((ForeignScan *)plan)->operation != CMD_SELECT)
  {
    return;
  }

                                
  context = set_deparse_context_planstate(es->deparse_cxt, (Node *)planstate, ancestors);
  useprefix = list_length(es->rtable) > 1;

                                                                
  foreach (lc, plan->targetlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);

    result = lappend(result, deparse_expression((Node *)tle->expr, context, useprefix, false));
  }

                     
  ExplainPropertyList("Output", result, es);
}

   
                             
   
static void
show_expression(Node *node, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es)
{
  List *context;
  char *exprstr;

                                
  context = set_deparse_context_planstate(es->deparse_cxt, (Node *)planstate, ancestors);

                              
  exprstr = deparse_expression(node, context, useprefix, false);

                          
  ExplainPropertyText(qlabel, exprstr, es);
}

   
                                                                             
   
static void
show_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, bool useprefix, ExplainState *es)
{
  Node *node;

                             
  if (qual == NIL)
  {
    return;
  }

                                        
  node = (Node *)make_ands_explicit(qual);

                   
  show_expression(node, qlabel, planstate, ancestors, useprefix, es);
}

   
                                                    
   
static void
show_scan_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es)
{
  bool useprefix;

  useprefix = (IsA(planstate->plan, SubqueryScan) || es->verbose);
  show_qual(qual, qlabel, planstate, ancestors, useprefix, es);
}

   
                                                            
   
static void
show_upper_qual(List *qual, const char *qlabel, PlanState *planstate, List *ancestors, ExplainState *es)
{
  bool useprefix;

  useprefix = (list_length(es->rtable) > 1 || es->verbose);
  show_qual(qual, qlabel, planstate, ancestors, useprefix, es);
}

   
                                       
   
static void
show_sort_keys(SortState *sortstate, List *ancestors, ExplainState *es)
{
  Sort *plan = (Sort *)sortstate->ss.ps.plan;

  show_sort_group_keys((PlanState *)sortstate, "Sort Key", plan->numCols, plan->sortColIdx, plan->sortOperators, plan->collations, plan->nullsFirst, ancestors, es);
}

   
                                     
   
static void
show_merge_append_keys(MergeAppendState *mstate, List *ancestors, ExplainState *es)
{
  MergeAppend *plan = (MergeAppend *)mstate->ps.plan;

  show_sort_group_keys((PlanState *)mstate, "Sort Key", plan->numCols, plan->sortColIdx, plan->sortOperators, plan->collations, plan->nullsFirst, ancestors, es);
}

   
                                           
   
static void
show_agg_keys(AggState *astate, List *ancestors, ExplainState *es)
{
  Agg *plan = (Agg *)astate->ss.ps.plan;

  if (plan->numCols > 0 || plan->groupingSets)
  {
                                                              
    ancestors = lcons(astate, ancestors);

    if (plan->groupingSets)
    {
      show_grouping_sets(outerPlanState(astate), plan, ancestors, es);
    }
    else
    {
      show_sort_group_keys(outerPlanState(astate), "Group Key", plan->numCols, plan->grpColIdx, NULL, NULL, NULL, ancestors, es);
    }

    ancestors = list_delete_first(ancestors);
  }
}

static void
show_grouping_sets(PlanState *planstate, Agg *agg, List *ancestors, ExplainState *es)
{
  List *context;
  bool useprefix;
  ListCell *lc;

                                
  context = set_deparse_context_planstate(es->deparse_cxt, (Node *)planstate, ancestors);
  useprefix = (list_length(es->rtable) > 1 || es->verbose);

  ExplainOpenGroup("Grouping Sets", "Grouping Sets", false, es);

  show_grouping_set_keys(planstate, agg, NULL, context, useprefix, ancestors, es);

  foreach (lc, agg->chain)
  {
    Agg *aggnode = lfirst(lc);
    Sort *sortnode = (Sort *)aggnode->plan.lefttree;

    show_grouping_set_keys(planstate, aggnode, sortnode, context, useprefix, ancestors, es);
  }

  ExplainCloseGroup("Grouping Sets", "Grouping Sets", false, es);
}

static void
show_grouping_set_keys(PlanState *planstate, Agg *aggnode, Sort *sortnode, List *context, bool useprefix, List *ancestors, ExplainState *es)
{
  Plan *plan = planstate->plan;
  char *exprstr;
  ListCell *lc;
  List *gsets = aggnode->groupingSets;
  AttrNumber *keycols = aggnode->grpColIdx;
  const char *keyname;
  const char *keysetname;

  if (aggnode->aggstrategy == AGG_HASHED || aggnode->aggstrategy == AGG_MIXED)
  {
    keyname = "Hash Key";
    keysetname = "Hash Keys";
  }
  else
  {
    keyname = "Group Key";
    keysetname = "Group Keys";
  }

  ExplainOpenGroup("Grouping Set", NULL, true, es);

  if (sortnode)
  {
    show_sort_group_keys(planstate, "Sort Key", sortnode->numCols, sortnode->sortColIdx, sortnode->sortOperators, sortnode->collations, sortnode->nullsFirst, ancestors, es);
    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      es->indent++;
    }
  }

  ExplainOpenGroup(keysetname, keysetname, false, es);

  foreach (lc, gsets)
  {
    List *result = NIL;
    ListCell *lc2;

    foreach (lc2, (List *)lfirst(lc))
    {
      Index i = lfirst_int(lc2);
      AttrNumber keyresno = keycols[i];
      TargetEntry *target = get_tle_by_resno(plan->targetlist, keyresno);

      if (!target)
      {
        elog(ERROR, "no tlist entry for key %d", keyresno);
      }
                                                              
      exprstr = deparse_expression((Node *)target->expr, context, useprefix, true);

      result = lappend(result, exprstr);
    }

    if (!result && es->format == EXPLAIN_FORMAT_TEXT)
    {
      ExplainPropertyText(keyname, "()", es);
    }
    else
    {
      ExplainPropertyListNested(keyname, result, es);
    }
  }

  ExplainCloseGroup(keysetname, keysetname, false, es);

  if (sortnode && es->format == EXPLAIN_FORMAT_TEXT)
  {
    es->indent--;
  }

  ExplainCloseGroup("Grouping Set", NULL, true, es);
}

   
                                            
   
static void
show_group_keys(GroupState *gstate, List *ancestors, ExplainState *es)
{
  Group *plan = (Group *)gstate->ss.ps.plan;

                                                            
  ancestors = lcons(gstate, ancestors);
  show_sort_group_keys(outerPlanState(gstate), "Group Key", plan->numCols, plan->grpColIdx, NULL, NULL, NULL, ancestors, es);
  ancestors = list_delete_first(ancestors);
}

   
                                                                            
                                                                            
                                                               
   
static void
show_sort_group_keys(PlanState *planstate, const char *qlabel, int nkeys, AttrNumber *keycols, Oid *sortOperators, Oid *collations, bool *nullsFirst, List *ancestors, ExplainState *es)
{
  Plan *plan = planstate->plan;
  List *context;
  List *result = NIL;
  StringInfoData sortkeybuf;
  bool useprefix;
  int keyno;

  if (nkeys <= 0)
  {
    return;
  }

  initStringInfo(&sortkeybuf);

                                
  context = set_deparse_context_planstate(es->deparse_cxt, (Node *)planstate, ancestors);
  useprefix = (list_length(es->rtable) > 1 || es->verbose);

  for (keyno = 0; keyno < nkeys; keyno++)
  {
                                      
    AttrNumber keyresno = keycols[keyno];
    TargetEntry *target = get_tle_by_resno(plan->targetlist, keyresno);
    char *exprstr;

    if (!target)
    {
      elog(ERROR, "no tlist entry for key %d", keyresno);
    }
                                                            
    exprstr = deparse_expression((Node *)target->expr, context, useprefix, true);
    resetStringInfo(&sortkeybuf);
    appendStringInfoString(&sortkeybuf, exprstr);
                                                    
    if (sortOperators != NULL)
    {
      show_sortorder_options(&sortkeybuf, (Node *)target->expr, sortOperators[keyno], collations[keyno], nullsFirst[keyno]);
    }
                                                  
    result = lappend(result, pstrdup(sortkeybuf.data));
  }

  ExplainPropertyList(qlabel, result, es);
}

   
                                                                             
                                            
   
static void
show_sortorder_options(StringInfo buf, Node *sortexpr, Oid sortOperator, Oid collation, bool nullsFirst)
{
  Oid sortcoltype = exprType(sortexpr);
  bool reverse = false;
  TypeCacheEntry *typentry;

  typentry = lookup_type_cache(sortcoltype, TYPECACHE_LT_OPR | TYPECACHE_GT_OPR);

     
                                                                         
                                                                            
                                                                             
                                                                           
                     
     
  if (OidIsValid(collation) && collation != get_typcollation(sortcoltype))
  {
    char *collname = get_collation_name(collation);

    if (collname == NULL)
    {
      elog(ERROR, "cache lookup failed for collation %u", collation);
    }
    appendStringInfo(buf, " COLLATE %s", quote_identifier(collname));
  }

                                                                         
  if (sortOperator == typentry->gt_opr)
  {
    appendStringInfoString(buf, " DESC");
    reverse = true;
  }
  else if (sortOperator != typentry->lt_opr)
  {
    char *opname = get_opname(sortOperator);

    if (opname == NULL)
    {
      elog(ERROR, "cache lookup failed for operator %u", sortOperator);
    }
    appendStringInfo(buf, " USING %s", opname);
                                                                    
    (void)get_equality_op_for_ordering_op(sortOperator, &reverse);
  }

                                                           
  if (nullsFirst && !reverse)
  {
    appendStringInfoString(buf, " NULLS FIRST");
  }
  else if (!nullsFirst && reverse)
  {
    appendStringInfoString(buf, " NULLS LAST");
  }
}

   
                               
   
static void
show_tablesample(TableSampleClause *tsc, PlanState *planstate, List *ancestors, ExplainState *es)
{
  List *context;
  bool useprefix;
  char *method_name;
  List *params = NIL;
  char *repeatable;
  ListCell *lc;

                                
  context = set_deparse_context_planstate(es->deparse_cxt, (Node *)planstate, ancestors);
  useprefix = list_length(es->rtable) > 1;

                                       
  method_name = get_func_name(tsc->tsmhandler);

                                     
  foreach (lc, tsc->args)
  {
    Node *arg = (Node *)lfirst(lc);

    params = lappend(params, deparse_expression(arg, context, useprefix, false));
  }
  if (tsc->repeatable)
  {
    repeatable = deparse_expression((Node *)tsc->repeatable, context, useprefix, false);
  }
  else
  {
    repeatable = NULL;
  }

                     
  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    bool first = true;

    appendStringInfoSpaces(es->str, es->indent * 2);
    appendStringInfo(es->str, "Sampling: %s (", method_name);
    foreach (lc, params)
    {
      if (!first)
      {
        appendStringInfoString(es->str, ", ");
      }
      appendStringInfoString(es->str, (const char *)lfirst(lc));
      first = false;
    }
    appendStringInfoChar(es->str, ')');
    if (repeatable)
    {
      appendStringInfo(es->str, " REPEATABLE (%s)", repeatable);
    }
    appendStringInfoChar(es->str, '\n');
  }
  else
  {
    ExplainPropertyText("Sampling Method", method_name, es);
    ExplainPropertyList("Sampling Parameters", params, es);
    if (repeatable)
    {
      ExplainPropertyText("Repeatable Seed", repeatable, es);
    }
  }
}

   
                                                                 
   
static void
show_sort_info(SortState *sortstate, ExplainState *es)
{
  if (!es->analyze)
  {
    return;
  }

  if (sortstate->sort_Done && sortstate->tuplesortstate != NULL)
  {
    Tuplesortstate *state = (Tuplesortstate *)sortstate->tuplesortstate;
    TuplesortInstrumentation stats;
    const char *sortMethod;
    const char *spaceType;
    long spaceUsed;

    tuplesort_get_stats(state, &stats);
    sortMethod = tuplesort_method_name(stats.sortMethod);
    spaceType = tuplesort_space_type_name(stats.spaceType);
    spaceUsed = stats.spaceUsed;

    if (es->format == EXPLAIN_FORMAT_TEXT)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfo(es->str, "Sort Method: %s  %s: %ldkB\n", sortMethod, spaceType, spaceUsed);
    }
    else
    {
      ExplainPropertyText("Sort Method", sortMethod, es);
      ExplainPropertyInteger("Sort Space Used", "kB", spaceUsed, es);
      ExplainPropertyText("Sort Space Type", spaceType, es);
    }
  }

  if (sortstate->shared_info != NULL)
  {
    int n;
    bool opened_group = false;

    for (n = 0; n < sortstate->shared_info->num_workers; n++)
    {
      TuplesortInstrumentation *sinstrument;
      const char *sortMethod;
      const char *spaceType;
      long spaceUsed;

      sinstrument = &sortstate->shared_info->sinstrument[n];
      if (sinstrument->sortMethod == SORT_TYPE_STILL_IN_PROGRESS)
      {
        continue;                                
      }
      sortMethod = tuplesort_method_name(sinstrument->sortMethod);
      spaceType = tuplesort_space_type_name(sinstrument->spaceType);
      spaceUsed = sinstrument->spaceUsed;

      if (es->format == EXPLAIN_FORMAT_TEXT)
      {
        appendStringInfoSpaces(es->str, es->indent * 2);
        appendStringInfo(es->str, "Worker %d:  Sort Method: %s  %s: %ldkB\n", n, sortMethod, spaceType, spaceUsed);
      }
      else
      {
        if (!opened_group)
        {
          ExplainOpenGroup("Workers", "Workers", false, es);
          opened_group = true;
        }
        ExplainOpenGroup("Worker", NULL, true, es);
        ExplainPropertyInteger("Worker Number", NULL, n, es);
        ExplainPropertyText("Sort Method", sortMethod, es);
        ExplainPropertyInteger("Sort Space Used", "kB", spaceUsed, es);
        ExplainPropertyText("Sort Space Type", spaceType, es);
        ExplainCloseGroup("Worker", NULL, true, es);
      }
    }
    if (opened_group)
    {
      ExplainCloseGroup("Workers", "Workers", false, es);
    }
  }
}

   
                                             
   
static void
show_hash_info(HashState *hashstate, ExplainState *es)
{
  HashInstrumentation hinstrument = {0};

     
                                                                         
                                                                             
                                                                          
                                                                             
                                                                 
     
  if (hashstate->hashtable)
  {
    ExecHashGetInstrumentation(&hinstrument, hashstate->hashtable);
  }

     
                                                                      
                                                                     
                                                                      
                                                                            
                                                                         
                                                                
     
  if (hashstate->shared_info)
  {
    SharedHashInfo *shared_info = hashstate->shared_info;
    int i;

    for (i = 0; i < shared_info->num_workers; ++i)
    {
      HashInstrumentation *worker_hi = &shared_info->hinstrument[i];

      if (worker_hi->nbatch > 0)
      {
           
                                                                   
                                                                
           
        hinstrument.nbuckets = worker_hi->nbuckets;
        hinstrument.nbuckets_original = worker_hi->nbuckets_original;

           
                                                                    
                                                                     
                                                                       
                                                      
           
        hinstrument.nbatch = Max(hinstrument.nbatch, worker_hi->nbatch);
        hinstrument.nbatch_original = worker_hi->nbatch_original;

           
                                                                
                                                       
           
        hinstrument.space_peak = Max(hinstrument.space_peak, worker_hi->space_peak);
      }
    }
  }

  if (hinstrument.nbatch > 0)
  {
    long spacePeakKb = (hinstrument.space_peak + 1023) / 1024;

    if (es->format != EXPLAIN_FORMAT_TEXT)
    {
      ExplainPropertyInteger("Hash Buckets", NULL, hinstrument.nbuckets, es);
      ExplainPropertyInteger("Original Hash Buckets", NULL, hinstrument.nbuckets_original, es);
      ExplainPropertyInteger("Hash Batches", NULL, hinstrument.nbatch, es);
      ExplainPropertyInteger("Original Hash Batches", NULL, hinstrument.nbatch_original, es);
      ExplainPropertyInteger("Peak Memory Usage", "kB", spacePeakKb, es);
    }
    else if (hinstrument.nbatch_original != hinstrument.nbatch || hinstrument.nbuckets_original != hinstrument.nbuckets)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfo(es->str, "Buckets: %d (originally %d)  Batches: %d (originally %d)  Memory Usage: %ldkB\n", hinstrument.nbuckets, hinstrument.nbuckets_original, hinstrument.nbatch, hinstrument.nbatch_original, spacePeakKb);
    }
    else
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfo(es->str, "Buckets: %d  Batches: %d  Memory Usage: %ldkB\n", hinstrument.nbuckets, hinstrument.nbatch, spacePeakKb);
    }
  }
}

   
                                                                             
   
static void
show_tidbitmap_info(BitmapHeapScanState *planstate, ExplainState *es)
{
  if (es->format != EXPLAIN_FORMAT_TEXT)
  {
    ExplainPropertyInteger("Exact Heap Blocks", NULL, planstate->exact_pages, es);
    ExplainPropertyInteger("Lossy Heap Blocks", NULL, planstate->lossy_pages, es);
  }
  else
  {
    if (planstate->exact_pages > 0 || planstate->lossy_pages > 0)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfoString(es->str, "Heap Blocks:");
      if (planstate->exact_pages > 0)
      {
        appendStringInfo(es->str, " exact=%ld", planstate->exact_pages);
      }
      if (planstate->lossy_pages > 0)
      {
        appendStringInfo(es->str, " lossy=%ld", planstate->lossy_pages);
      }
      appendStringInfoChar(es->str, '\n');
    }
  }
}

   
                                                                             
   
                                                             
   
static void
show_instrumentation_count(const char *qlabel, int which, PlanState *planstate, ExplainState *es)
{
  double nfiltered;
  double nloops;

  if (!es->analyze || !planstate->instrument)
  {
    return;
  }

  if (which == 2)
  {
    nfiltered = planstate->instrument->nfiltered2;
  }
  else
  {
    nfiltered = planstate->instrument->nfiltered1;
  }
  nloops = planstate->instrument->nloops;

                                                                          
  if (nfiltered > 0 || es->format != EXPLAIN_FORMAT_TEXT)
  {
    if (nloops > 0)
    {
      ExplainPropertyFloat(qlabel, NULL, nfiltered / nloops, 0, es);
    }
    else
    {
      ExplainPropertyFloat(qlabel, NULL, 0.0, 0, es);
    }
  }
}

   
                                                  
   
static void
show_foreignscan_info(ForeignScanState *fsstate, ExplainState *es)
{
  FdwRoutine *fdwroutine = fsstate->fdwroutine;

                                                 
  if (((ForeignScan *)fsstate->ss.ps.plan)->operation != CMD_SELECT)
  {
    if (fdwroutine->ExplainDirectModify != NULL)
    {
      fdwroutine->ExplainDirectModify(fsstate, es);
    }
  }
  else
  {
    if (fdwroutine->ExplainForeignScan != NULL)
    {
      fdwroutine->ExplainForeignScan(fsstate, es);
    }
  }
}

   
                                                                  
   
static void
show_eval_params(Bitmapset *bms_params, ExplainState *es)
{
  int paramid = -1;
  List *params = NIL;

  Assert(bms_params);

  while ((paramid = bms_next_member(bms_params, paramid)) >= 0)
  {
    char param[32];

    snprintf(param, sizeof(param), "$%d", paramid);
    params = lappend(params, pstrdup(param));
  }

  if (params)
  {
    ExplainPropertyList("Params Evaluated", params, es);
  }
}

   
                                            
   
                                                                             
                             
   
                                                                          
                                                                             
                                                      
   
static const char *
explain_get_index_name(Oid indexId)
{
  const char *result;

  if (explain_get_index_name_hook)
  {
    result = (*explain_get_index_name_hook)(indexId);
  }
  else
  {
    result = NULL;
  }
  if (result == NULL)
  {
                                                      
    result = get_rel_name(indexId);
    if (result == NULL)
    {
      elog(ERROR, "cache lookup failed for index %u", indexId);
    }
  }
  return result;
}

   
                              
   
static void
show_buffer_usage(ExplainState *es, const BufferUsage *usage)
{
  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    bool has_shared = (usage->shared_blks_hit > 0 || usage->shared_blks_read > 0 || usage->shared_blks_dirtied > 0 || usage->shared_blks_written > 0);
    bool has_local = (usage->local_blks_hit > 0 || usage->local_blks_read > 0 || usage->local_blks_dirtied > 0 || usage->local_blks_written > 0);
    bool has_temp = (usage->temp_blks_read > 0 || usage->temp_blks_written > 0);
    bool has_timing = (!INSTR_TIME_IS_ZERO(usage->blk_read_time) || !INSTR_TIME_IS_ZERO(usage->blk_write_time));

                                            
    if (has_shared || has_local || has_temp)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfoString(es->str, "Buffers:");

      if (has_shared)
      {
        appendStringInfoString(es->str, " shared");
        if (usage->shared_blks_hit > 0)
        {
          appendStringInfo(es->str, " hit=%ld", usage->shared_blks_hit);
        }
        if (usage->shared_blks_read > 0)
        {
          appendStringInfo(es->str, " read=%ld", usage->shared_blks_read);
        }
        if (usage->shared_blks_dirtied > 0)
        {
          appendStringInfo(es->str, " dirtied=%ld", usage->shared_blks_dirtied);
        }
        if (usage->shared_blks_written > 0)
        {
          appendStringInfo(es->str, " written=%ld", usage->shared_blks_written);
        }
        if (has_local || has_temp)
        {
          appendStringInfoChar(es->str, ',');
        }
      }
      if (has_local)
      {
        appendStringInfoString(es->str, " local");
        if (usage->local_blks_hit > 0)
        {
          appendStringInfo(es->str, " hit=%ld", usage->local_blks_hit);
        }
        if (usage->local_blks_read > 0)
        {
          appendStringInfo(es->str, " read=%ld", usage->local_blks_read);
        }
        if (usage->local_blks_dirtied > 0)
        {
          appendStringInfo(es->str, " dirtied=%ld", usage->local_blks_dirtied);
        }
        if (usage->local_blks_written > 0)
        {
          appendStringInfo(es->str, " written=%ld", usage->local_blks_written);
        }
        if (has_temp)
        {
          appendStringInfoChar(es->str, ',');
        }
      }
      if (has_temp)
      {
        appendStringInfoString(es->str, " temp");
        if (usage->temp_blks_read > 0)
        {
          appendStringInfo(es->str, " read=%ld", usage->temp_blks_read);
        }
        if (usage->temp_blks_written > 0)
        {
          appendStringInfo(es->str, " written=%ld", usage->temp_blks_written);
        }
      }
      appendStringInfoChar(es->str, '\n');
    }

                                                      
    if (has_timing)
    {
      appendStringInfoSpaces(es->str, es->indent * 2);
      appendStringInfoString(es->str, "I/O Timings:");
      if (!INSTR_TIME_IS_ZERO(usage->blk_read_time))
      {
        appendStringInfo(es->str, " read=%0.3f", INSTR_TIME_GET_MILLISEC(usage->blk_read_time));
      }
      if (!INSTR_TIME_IS_ZERO(usage->blk_write_time))
      {
        appendStringInfo(es->str, " write=%0.3f", INSTR_TIME_GET_MILLISEC(usage->blk_write_time));
      }
      appendStringInfoChar(es->str, '\n');
    }
  }
  else
  {
    ExplainPropertyInteger("Shared Hit Blocks", NULL, usage->shared_blks_hit, es);
    ExplainPropertyInteger("Shared Read Blocks", NULL, usage->shared_blks_read, es);
    ExplainPropertyInteger("Shared Dirtied Blocks", NULL, usage->shared_blks_dirtied, es);
    ExplainPropertyInteger("Shared Written Blocks", NULL, usage->shared_blks_written, es);
    ExplainPropertyInteger("Local Hit Blocks", NULL, usage->local_blks_hit, es);
    ExplainPropertyInteger("Local Read Blocks", NULL, usage->local_blks_read, es);
    ExplainPropertyInteger("Local Dirtied Blocks", NULL, usage->local_blks_dirtied, es);
    ExplainPropertyInteger("Local Written Blocks", NULL, usage->local_blks_written, es);
    ExplainPropertyInteger("Temp Read Blocks", NULL, usage->temp_blks_read, es);
    ExplainPropertyInteger("Temp Written Blocks", NULL, usage->temp_blks_written, es);
    if (track_io_timing)
    {
      ExplainPropertyFloat("I/O Read Time", "ms", INSTR_TIME_GET_MILLISEC(usage->blk_read_time), 3, es);
      ExplainPropertyFloat("I/O Write Time", "ms", INSTR_TIME_GET_MILLISEC(usage->blk_write_time), 3, es);
    }
  }
}

   
                                                                   
   
static void
ExplainIndexScanDetails(Oid indexid, ScanDirection indexorderdir, ExplainState *es)
{
  const char *indexname = explain_get_index_name(indexid);

  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    if (ScanDirectionIsBackward(indexorderdir))
    {
      appendStringInfoString(es->str, " Backward");
    }
    appendStringInfo(es->str, " using %s", quote_identifier(indexname));
  }
  else
  {
    const char *scandir;

    switch (indexorderdir)
    {
    case BackwardScanDirection:
      scandir = "Backward";
      break;
    case NoMovementScanDirection:
      scandir = "NoMovement";
      break;
    case ForwardScanDirection:
      scandir = "Forward";
      break;
    default:
      scandir = "???";
      break;
    }
    ExplainPropertyText("Scan Direction", scandir, es);
    ExplainPropertyText("Index Name", indexname, es);
  }
}

   
                                  
   
static void
ExplainScanTarget(Scan *plan, ExplainState *es)
{
  ExplainTargetRel((Plan *)plan, plan->scanrelid, es);
}

   
                                         
   
                                                                           
                                                                               
                               
   
static void
ExplainModifyTarget(ModifyTable *plan, ExplainState *es)
{
  ExplainTargetRel((Plan *)plan, plan->nominalRelation, es);
}

   
                                                     
   
static void
ExplainTargetRel(Plan *plan, Index rti, ExplainState *es)
{
  char *objectname = NULL;
  char *namespace = NULL;
  const char *objecttag = NULL;
  RangeTblEntry *rte;
  char *refname;

  rte = rt_fetch(rti, es->rtable);
  refname = (char *)list_nth(es->rtable_names, rti - 1);
  if (refname == NULL)
  {
    refname = rte->eref->aliasname;
  }

  switch (nodeTag(plan))
  {
  case T_SeqScan:
  case T_SampleScan:
  case T_IndexScan:
  case T_IndexOnlyScan:
  case T_BitmapHeapScan:
  case T_TidScan:
  case T_ForeignScan:
  case T_CustomScan:
  case T_ModifyTable:
                                        
    Assert(rte->rtekind == RTE_RELATION);
    objectname = get_rel_name(rte->relid);
    if (es->verbose)
    namespace = get_namespace_name(get_rel_namespace(rte->relid));
    objecttag = "Relation Name";
    break;
  case T_FunctionScan:
  {
    FunctionScan *fscan = (FunctionScan *)plan;

                                        
    Assert(rte->rtekind == RTE_FUNCTION);

       
                                                              
                                                           
                                                                
                                                                 
       
    if (list_length(fscan->functions) == 1)
    {
      RangeTblFunction *rtfunc = (RangeTblFunction *)linitial(fscan->functions);

      if (IsA(rtfunc->funcexpr, FuncExpr))
      {
        FuncExpr *funcexpr = (FuncExpr *)rtfunc->funcexpr;
        Oid funcid = funcexpr->funcid;

        objectname = get_func_name(funcid);
        if (es->verbose)
        namespace = get_namespace_name(get_func_namespace(funcid));
      }
    }
    objecttag = "Function Name";
  }
  break;
  case T_TableFuncScan:
    Assert(rte->rtekind == RTE_TABLEFUNC);
    objectname = "xmltable";
    objecttag = "Table Function Name";
    break;
  case T_ValuesScan:
    Assert(rte->rtekind == RTE_VALUES);
    break;
  case T_CteScan:
                                                 
    Assert(rte->rtekind == RTE_CTE);
    Assert(!rte->self_reference);
    objectname = rte->ctename;
    objecttag = "CTE Name";
    break;
  case T_NamedTuplestoreScan:
    Assert(rte->rtekind == RTE_NAMEDTUPLESTORE);
    objectname = rte->enrname;
    objecttag = "Tuplestore Name";
    break;
  case T_WorkTableScan:
                                             
    Assert(rte->rtekind == RTE_CTE);
    Assert(rte->self_reference);
    objectname = rte->ctename;
    objecttag = "CTE Name";
    break;
  default:
    break;
  }

  if (es->format == EXPLAIN_FORMAT_TEXT)
  {
    appendStringInfoString(es->str, " on");
    if (namespace != NULL)
    {
      appendStringInfo(es->str, " %s.%s", quote_identifier(namespace), quote_identifier(objectname));
    }
    else if (objectname != NULL)
    {
      appendStringInfo(es->str, " %s", quote_identifier(objectname));
    }
    if (objectname == NULL || strcmp(refname, objectname) != 0)
    {
      appendStringInfo(es->str, " %s", quote_identifier(refname));
    }
  }
  else
  {
    if (objecttag != NULL && objectname != NULL)
    {
      ExplainPropertyText(objecttag, objectname, es);
    }
    if (namespace != NULL)
    {
      ExplainPropertyText("Schema", namespace, es);
    }
    ExplainPropertyText("Alias", refname, es);
  }
}

   
                                                 
   
                                                                          
                                                                        
                                                                              
                                                        
   
static void
show_modifytable_info(ModifyTableState *mtstate, List *ancestors, ExplainState *es)
{
  ModifyTable *node = (ModifyTable *)mtstate->ps.plan;
  const char *operation;
  const char *foperation;
  bool labeltargets;
  int j;
  List *idxNames = NIL;
  ListCell *lst;

  switch (node->operation)
  {
  case CMD_INSERT:
    operation = "Insert";
    foperation = "Foreign Insert";
    break;
  case CMD_UPDATE:
    operation = "Update";
    foperation = "Foreign Update";
    break;
  case CMD_DELETE:
    operation = "Delete";
    foperation = "Foreign Delete";
    break;
  default:
    operation = "???";
    foperation = "Foreign ???";
    break;
  }

                                                    
  labeltargets = (mtstate->mt_nplans > 1 || (mtstate->mt_nplans == 1 && mtstate->resultRelInfo[0].ri_RangeTableIndex != node->nominalRelation));

  if (labeltargets)
  {
    ExplainOpenGroup("Target Tables", "Target Tables", false, es);
  }

  for (j = 0; j < mtstate->mt_nplans; j++)
  {
    ResultRelInfo *resultRelInfo = mtstate->resultRelInfo + j;
    FdwRoutine *fdwroutine = resultRelInfo->ri_FdwRoutine;

    if (labeltargets)
    {
                                        
      ExplainOpenGroup("Target Table", NULL, true, es);

         
                                                                         
                                                                  
         
      if (es->format == EXPLAIN_FORMAT_TEXT)
      {
        appendStringInfoSpaces(es->str, es->indent * 2);
        appendStringInfoString(es->str, fdwroutine ? foperation : operation);
      }

                           
      ExplainTargetRel((Plan *)node, resultRelInfo->ri_RangeTableIndex, es);

      if (es->format == EXPLAIN_FORMAT_TEXT)
      {
        appendStringInfoChar(es->str, '\n');
        es->indent++;
      }
    }

                                     
    if (!resultRelInfo->ri_usesFdwDirectModify && fdwroutine != NULL && fdwroutine->ExplainForeignModify != NULL)
    {
      List *fdw_private = (List *)list_nth(node->fdwPrivLists, j);

      fdwroutine->ExplainForeignModify(mtstate, resultRelInfo, fdw_private, j, es);
    }

    if (labeltargets)
    {
                                                        
      if (es->format == EXPLAIN_FORMAT_TEXT)
      {
        es->indent--;
      }

                           
      ExplainCloseGroup("Target Table", NULL, true, es);
    }
  }

                                                   
  foreach (lst, node->arbiterIndexes)
  {
    char *indexname = get_rel_name(lfirst_oid(lst));

    idxNames = lappend(idxNames, indexname);
  }

  if (node->onConflictAction != ONCONFLICT_NONE)
  {
    ExplainPropertyText("Conflict Resolution", node->onConflictAction == ONCONFLICT_NOTHING ? "NOTHING" : "UPDATE", es);

       
                                                                    
                                        
       
    if (idxNames)
    {
      ExplainPropertyList("Conflict Arbiter Indexes", idxNames, es);
    }

                                                                 
    if (node->onConflictWhere)
    {
      show_upper_qual((List *)node->onConflictWhere, "Conflict Filter", &mtstate->ps, ancestors, es);
      show_instrumentation_count("Rows Removed by Conflict Filter", 1, &mtstate->ps, es);
    }

                                                                           
    if (es->analyze && mtstate->ps.instrument)
    {
      double total;
      double insert_path;
      double other_path;

      InstrEndLoop(mtstate->mt_plans[0]->instrument);

                                           
      total = mtstate->mt_plans[0]->instrument->ntuples;
      other_path = mtstate->ps.instrument->ntuples2;
      insert_path = total - other_path;

      ExplainPropertyFloat("Tuples Inserted", NULL, insert_path, 0, es);
      ExplainPropertyFloat("Conflicting Tuples", NULL, other_path, 0, es);
    }
  }

  if (labeltargets)
  {
    ExplainCloseGroup("Target Tables", "Target Tables", false, es);
  }
}

   
                                                                        
                                
   
                                                                           
          
   
static void
ExplainMemberNodes(PlanState **planstates, int nplans, List *ancestors, ExplainState *es)
{
  int j;

  for (j = 0; j < nplans; j++)
  {
    ExplainNode(planstates[j], ancestors, "Member", NULL, es);
  }
}

   
                                                                      
   
                                                 
                                                                    
                                                                    
   
static void
ExplainMissingMembers(int nplans, int nchildren, ExplainState *es)
{
  if (nplans < nchildren || es->format != EXPLAIN_FORMAT_TEXT)
  {
    ExplainPropertyInteger("Subplans Removed", NULL, nchildren - nplans, es);
  }
}

   
                                                                            
   
                                                                           
                  
   
static void
ExplainSubPlans(List *plans, List *ancestors, const char *relationship, ExplainState *es)
{
  ListCell *lst;

  foreach (lst, plans)
  {
    SubPlanState *sps = (SubPlanState *)lfirst(lst);
    SubPlan *sp = sps->subplan;

       
                                                                         
                                                                           
                                                                           
                                                                           
                                                                          
                                                                          
                                                                          
                                   
       
    if (bms_is_member(sp->plan_id, es->printed_subplans))
    {
      continue;
    }
    es->printed_subplans = bms_add_member(es->printed_subplans, sp->plan_id);

    ExplainNode(sps->planstate, ancestors, relationship, sp->plan_name, es);
  }
}

   
                                               
   
static void
ExplainCustomChildren(CustomScanState *css, List *ancestors, ExplainState *es)
{
  ListCell *cell;
  const char *label = (list_length(css->custom_ps) != 1 ? "children" : "child");

  foreach (cell, css->custom_ps)
  {
    ExplainNode((PlanState *)lfirst(cell), ancestors, label, NULL, es);
  }
}

   
                                                                            
                                                              
   
void
ExplainPropertyList(const char *qlabel, List *data, ExplainState *es)
{
  ListCell *lc;
  bool first = true;

  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
    appendStringInfoSpaces(es->str, es->indent * 2);
    appendStringInfo(es->str, "%s: ", qlabel);
    foreach (lc, data)
    {
      if (!first)
      {
        appendStringInfoString(es->str, ", ");
      }
      appendStringInfoString(es->str, (const char *)lfirst(lc));
      first = false;
    }
    appendStringInfoChar(es->str, '\n');
    break;

  case EXPLAIN_FORMAT_XML:
    ExplainXMLTag(qlabel, X_OPENING, es);
    foreach (lc, data)
    {
      char *str;

      appendStringInfoSpaces(es->str, es->indent * 2 + 2);
      appendStringInfoString(es->str, "<Item>");
      str = escape_xml((const char *)lfirst(lc));
      appendStringInfoString(es->str, str);
      pfree(str);
      appendStringInfoString(es->str, "</Item>\n");
    }
    ExplainXMLTag(qlabel, X_CLOSING, es);
    break;

  case EXPLAIN_FORMAT_JSON:
    ExplainJSONLineEnding(es);
    appendStringInfoSpaces(es->str, es->indent * 2);
    escape_json(es->str, qlabel);
    appendStringInfoString(es->str, ": [");
    foreach (lc, data)
    {
      if (!first)
      {
        appendStringInfoString(es->str, ", ");
      }
      escape_json(es->str, (const char *)lfirst(lc));
      first = false;
    }
    appendStringInfoChar(es->str, ']');
    break;

  case EXPLAIN_FORMAT_YAML:
    ExplainYAMLLineStarting(es);
    appendStringInfo(es->str, "%s: ", qlabel);
    foreach (lc, data)
    {
      appendStringInfoChar(es->str, '\n');
      appendStringInfoSpaces(es->str, es->indent * 2 + 2);
      appendStringInfoString(es->str, "- ");
      escape_yaml(es->str, (const char *)lfirst(lc));
    }
    break;
  }
}

   
                                                                              
                                                 
   
void
ExplainPropertyListNested(const char *qlabel, List *data, ExplainState *es)
{
  ListCell *lc;
  bool first = true;

  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
  case EXPLAIN_FORMAT_XML:
    ExplainPropertyList(qlabel, data, es);
    return;

  case EXPLAIN_FORMAT_JSON:
    ExplainJSONLineEnding(es);
    appendStringInfoSpaces(es->str, es->indent * 2);
    appendStringInfoChar(es->str, '[');
    foreach (lc, data)
    {
      if (!first)
      {
        appendStringInfoString(es->str, ", ");
      }
      escape_json(es->str, (const char *)lfirst(lc));
      first = false;
    }
    appendStringInfoChar(es->str, ']');
    break;

  case EXPLAIN_FORMAT_YAML:
    ExplainYAMLLineStarting(es);
    appendStringInfoString(es->str, "- [");
    foreach (lc, data)
    {
      if (!first)
      {
        appendStringInfoString(es->str, ", ");
      }
      escape_yaml(es->str, (const char *)lfirst(lc));
      first = false;
    }
    appendStringInfoChar(es->str, ']');
    break;
  }
}

   
                              
   
                                                                    
                                  
   
                                                                        
   
                                                                            
                                                                       
   
static void
ExplainProperty(const char *qlabel, const char *unit, const char *value, bool numeric, ExplainState *es)
{
  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
    appendStringInfoSpaces(es->str, es->indent * 2);
    if (unit)
    {
      appendStringInfo(es->str, "%s: %s %s\n", qlabel, value, unit);
    }
    else
    {
      appendStringInfo(es->str, "%s: %s\n", qlabel, value);
    }
    break;

  case EXPLAIN_FORMAT_XML:
  {
    char *str;

    appendStringInfoSpaces(es->str, es->indent * 2);
    ExplainXMLTag(qlabel, X_OPENING | X_NOWHITESPACE, es);
    str = escape_xml(value);
    appendStringInfoString(es->str, str);
    pfree(str);
    ExplainXMLTag(qlabel, X_CLOSING | X_NOWHITESPACE, es);
    appendStringInfoChar(es->str, '\n');
  }
  break;

  case EXPLAIN_FORMAT_JSON:
    ExplainJSONLineEnding(es);
    appendStringInfoSpaces(es->str, es->indent * 2);
    escape_json(es->str, qlabel);
    appendStringInfoString(es->str, ": ");
    if (numeric)
    {
      appendStringInfoString(es->str, value);
    }
    else
    {
      escape_json(es->str, value);
    }
    break;

  case EXPLAIN_FORMAT_YAML:
    ExplainYAMLLineStarting(es);
    appendStringInfo(es->str, "%s: ", qlabel);
    if (numeric)
    {
      appendStringInfoString(es->str, value);
    }
    else
    {
      escape_yaml(es->str, value);
    }
    break;
  }
}

   
                                     
   
void
ExplainPropertyText(const char *qlabel, const char *value, ExplainState *es)
{
  ExplainProperty(qlabel, NULL, value, false, es);
}

   
                                       
   
void
ExplainPropertyInteger(const char *qlabel, const char *unit, int64 value, ExplainState *es)
{
  char buf[32];

  snprintf(buf, sizeof(buf), INT64_FORMAT, value);
  ExplainProperty(qlabel, unit, buf, true, es);
}

   
                                                                  
                      
   
void
ExplainPropertyFloat(const char *qlabel, const char *unit, double value, int ndigits, ExplainState *es)
{
  char *buf;

  buf = psprintf("%.*f", ndigits, value);
  ExplainProperty(qlabel, unit, buf, true, es);
  pfree(buf);
}

   
                                   
   
void
ExplainPropertyBool(const char *qlabel, bool value, ExplainState *es)
{
  ExplainProperty(qlabel, NULL, value ? "true" : "false", true, es);
}

   
                                    
   
                                                                          
                                 
   
                                                                     
                                                      
   
void
ExplainOpenGroup(const char *objtype, const char *labelname, bool labeled, ExplainState *es)
{
  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
                       
    break;

  case EXPLAIN_FORMAT_XML:
    ExplainXMLTag(objtype, X_OPENING, es);
    es->indent++;
    break;

  case EXPLAIN_FORMAT_JSON:
    ExplainJSONLineEnding(es);
    appendStringInfoSpaces(es->str, 2 * es->indent);
    if (labelname)
    {
      escape_json(es->str, labelname);
      appendStringInfoString(es->str, ": ");
    }
    appendStringInfoChar(es->str, labeled ? '{' : '[');

       
                                                                       
                                                                   
                                                                   
                                
       
    es->grouping_stack = lcons_int(0, es->grouping_stack);
    es->indent++;
    break;

  case EXPLAIN_FORMAT_YAML:

       
                                                                       
                                                                      
                                                              
                                  
       
    ExplainYAMLLineStarting(es);
    if (labelname)
    {
      appendStringInfo(es->str, "%s: ", labelname);
      es->grouping_stack = lcons_int(1, es->grouping_stack);
    }
    else
    {
      appendStringInfoString(es->str, "- ");
      es->grouping_stack = lcons_int(0, es->grouping_stack);
    }
    es->indent++;
    break;
  }
}

   
                                     
                                                                  
   
void
ExplainCloseGroup(const char *objtype, const char *labelname, bool labeled, ExplainState *es)
{
  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
                       
    break;

  case EXPLAIN_FORMAT_XML:
    es->indent--;
    ExplainXMLTag(objtype, X_CLOSING, es);
    break;

  case EXPLAIN_FORMAT_JSON:
    es->indent--;
    appendStringInfoChar(es->str, '\n');
    appendStringInfoSpaces(es->str, 2 * es->indent);
    appendStringInfoChar(es->str, labeled ? '}' : ']');
    es->grouping_stack = list_delete_first(es->grouping_stack);
    break;

  case EXPLAIN_FORMAT_YAML:
    es->indent--;
    es->grouping_stack = list_delete_first(es->grouping_stack);
    break;
  }
}

   
                                                    
   
                                                                          
                                 
   
static void
ExplainDummyGroup(const char *objtype, const char *labelname, ExplainState *es)
{
  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
                       
    break;

  case EXPLAIN_FORMAT_XML:
    ExplainXMLTag(objtype, X_CLOSE_IMMEDIATE, es);
    break;

  case EXPLAIN_FORMAT_JSON:
    ExplainJSONLineEnding(es);
    appendStringInfoSpaces(es->str, 2 * es->indent);
    if (labelname)
    {
      escape_json(es->str, labelname);
      appendStringInfoString(es->str, ": ");
    }
    escape_json(es->str, objtype);
    break;

  case EXPLAIN_FORMAT_YAML:
    ExplainYAMLLineStarting(es);
    if (labelname)
    {
      escape_yaml(es->str, labelname);
      appendStringInfoString(es->str, ": ");
    }
    else
    {
      appendStringInfoString(es->str, "- ");
    }
    escape_yaml(es->str, objtype);
    break;
  }
}

   
                                         
   
                                                                         
                                   
   
void
ExplainBeginOutput(ExplainState *es)
{
  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
                       
    break;

  case EXPLAIN_FORMAT_XML:
    appendStringInfoString(es->str, "<explain xmlns=\"http://www.postgresql.org/2009/explain\">\n");
    es->indent++;
    break;

  case EXPLAIN_FORMAT_JSON:
                                                  
    appendStringInfoChar(es->str, '[');
    es->grouping_stack = lcons_int(0, es->grouping_stack);
    es->indent++;
    break;

  case EXPLAIN_FORMAT_YAML:
    es->grouping_stack = lcons_int(0, es->grouping_stack);
    break;
  }
}

   
                                       
   
void
ExplainEndOutput(ExplainState *es)
{
  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
                       
    break;

  case EXPLAIN_FORMAT_XML:
    es->indent--;
    appendStringInfoString(es->str, "</explain>");
    break;

  case EXPLAIN_FORMAT_JSON:
    es->indent--;
    appendStringInfoString(es->str, "\n]");
    es->grouping_stack = list_delete_first(es->grouping_stack);
    break;

  case EXPLAIN_FORMAT_YAML:
    es->grouping_stack = list_delete_first(es->grouping_stack);
    break;
  }
}

   
                                                       
   
void
ExplainSeparatePlans(ExplainState *es)
{
  switch (es->format)
  {
  case EXPLAIN_FORMAT_TEXT:
                          
    appendStringInfoChar(es->str, '\n');
    break;

  case EXPLAIN_FORMAT_XML:
  case EXPLAIN_FORMAT_JSON:
  case EXPLAIN_FORMAT_YAML:
                       
    break;
  }
}

   
                                    
   
                                                                    
                                                                             
        
   
                                                                             
                                                                            
                                                                
   
static void
ExplainXMLTag(const char *tagname, int flags, ExplainState *es)
{
  const char *s;
  const char *valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";

  if ((flags & X_NOWHITESPACE) == 0)
  {
    appendStringInfoSpaces(es->str, 2 * es->indent);
  }
  appendStringInfoCharMacro(es->str, '<');
  if ((flags & X_CLOSING) != 0)
  {
    appendStringInfoCharMacro(es->str, '/');
  }
  for (s = tagname; *s; s++)
  {
    appendStringInfoChar(es->str, strchr(valid, *s) ? *s : '-');
  }
  if ((flags & X_CLOSE_IMMEDIATE) != 0)
  {
    appendStringInfoString(es->str, " /");
  }
  appendStringInfoCharMacro(es->str, '>');
  if ((flags & X_NOWHITESPACE) == 0)
  {
    appendStringInfoCharMacro(es->str, '\n');
  }
}

   
                            
   
                                                                                
                                                                               
                                                    
   
static void
ExplainJSONLineEnding(ExplainState *es)
{
  Assert(es->format == EXPLAIN_FORMAT_JSON);
  if (linitial_int(es->grouping_stack) != 0)
  {
    appendStringInfoChar(es->str, ',');
  }
  else
  {
    linitial_int(es->grouping_stack) = 1;
  }
  appendStringInfoChar(es->str, '\n');
}

   
                       
   
                                                                           
                                                                         
                                                                               
                                                                              
                                                                       
   
static void
ExplainYAMLLineStarting(ExplainState *es)
{
  Assert(es->format == EXPLAIN_FORMAT_YAML);
  if (linitial_int(es->grouping_stack) == 0)
  {
    linitial_int(es->grouping_stack) = 1;
  }
  else
  {
    appendStringInfoChar(es->str, '\n');
    appendStringInfoSpaces(es->str, es->indent * 2);
  }
}

   
                                                                         
                                                                          
                                                                               
                                                                           
                                                                              
                                                                         
                                                                               
                                  
   
static void
escape_yaml(StringInfo buf, const char *str)
{
  escape_json(buf, str);
}
