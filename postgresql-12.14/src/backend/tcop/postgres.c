                                                                            
   
              
                                  
   
                                                                         
                                                                        
   
   
                  
                                 
   
         
                                                           
                                                 
   
                                                                            
   

#include "postgres.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifndef HAVE_GETRUSAGE
#include "rusagestub.h"
#endif

#include "access/parallel.h"
#include "access/printtup.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "commands/async.h"
#include "commands/prepare.h"
#include "jit/jit.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "nodes/print.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "pg_trace.h"
#include "parser/analyze.h"
#include "parser/parser.h"
#include "pg_getopt.h"
#include "postmaster/autovacuum.h"
#include "postmaster/postmaster.h"
#include "replication/logicallauncher.h"
#include "replication/logicalworker.h"
#include "replication/slot.h"
#include "replication/walsender.h"
#include "rewrite/rewriteHandler.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/procsignal.h"
#include "storage/sinval.h"
#include "tcop/fastpath.h"
#include "tcop/pquery.h"
#include "tcop/tcopprot.h"
#include "tcop/utility.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/snapmgr.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"
#include "mb/pg_wchar.h"

                    
                     
                    
   
const char *debug_query_string;                                   

                                                                              
CommandDest whereToSendOutput = DestDebug;

                                     
bool Log_disconnections = false;

int log_statement = LOGSTMT_NONE;

                                                                  
int max_stack_depth = 100;

                                                    
int PostAuthDelay = 0;

                    
                      
                    
   

                                                              
static long max_stack_depth_bytes = 100 * 1024L;

   
                                                                        
                                                                           
                                                                         
                                         
   
char *stack_base_ptr = NULL;

   
                                                             
   
#if defined(__ia64__) || defined(__ia64)
char *register_stack_base_ptr = NULL;
#endif

   
                                                                
                                                                          
   
static bool xact_started = false;

   
                                                                         
                                                                      
                                  
   
static bool DoingCommandRead = false;

   
                                                                          
                                
   
static bool doing_extended_query_message = false;
static bool ignore_till_sync = false;

   
                                                                    
   
static bool stmt_timeout_active = false;

   
                                                              
                                                                     
                                                        
   
static CachedPlanSource *unnamed_stmt_psrc = NULL;

                                    
static const char *userDoption = NULL;                    
static bool EchoQuery = false;                            
static bool UseSemiNewlineNewline = false;                

                                                                         
static bool RecoveryConflictPending = false;
static bool RecoveryConflictRetryable = true;
static ProcSignalReason RecoveryConflictReason;

                                                          
static MemoryContext row_description_context = NULL;
static StringInfoData row_description_buf;

                                                                    
                                              
                                                                    
   
static int
InteractiveBackend(StringInfo inBuf);
static int
interactive_getc(void);
static int
SocketBackend(StringInfo inBuf);
static int
ReadCommand(StringInfo inBuf);
static void
forbidden_in_wal_sender(char firstchar);
static List *
pg_rewrite_query(Query *query);
static bool
check_log_statement(List *stmt_list);
static int
errdetail_execute(List *raw_parsetree_list);
static int
errdetail_params(ParamListInfo params);
static int
errdetail_abort(void);
static int
errdetail_recovery_conflict(void);
static void
start_xact_command(void);
static void
finish_xact_command(void);
static bool
IsTransactionExitStmt(Node *parsetree);
static bool
IsTransactionExitStmtList(List *pstmts);
static bool
IsTransactionStmtList(List *pstmts);
static void
drop_unnamed_stmt(void);
static void
log_disconnections(int code, Datum arg);
static void
enable_statement_timeout(void);
static void
disable_statement_timeout(void);

                                                                    
                                  
                                                                    
   

                    
                                                                   
   
                                                                    
                                             
   
                                                                    
                    
   

static int
InteractiveBackend(StringInfo inBuf)
{
  int c;                                 

     
                                                     
     
  printf("backend> ");
  fflush(stdout);

  resetStringInfo(inBuf);

     
                                                                     
     
  while ((c = interactive_getc()) != EOF)
  {
    if (c == '\n')
    {
      if (UseSemiNewlineNewline)
      {
           
                                                                   
                                                                  
           
        if (inBuf->len > 1 && inBuf->data[inBuf->len - 1] == '\n' && inBuf->data[inBuf->len - 2] == ';')
        {
                                                     
          break;
        }
      }
      else
      {
           
                                                                      
                      
           
        if (inBuf->len > 0 && inBuf->data[inBuf->len - 1] == '\\')
        {
                                            
          inBuf->data[--inBuf->len] = '\0';
                                   
          continue;
        }
        else
        {
                                                               
          appendStringInfoChar(inBuf, '\n');
          break;
        }
      }
    }

                                                              
    appendStringInfoChar(inBuf, (char)c);
  }

                                                      
  if (c == EOF && inBuf->len == 0)
  {
    return EOF;
  }

     
                                                   
     

                                                          
  appendStringInfoChar(inBuf, (char)'\0');

     
                                                         
     
  if (EchoQuery)
  {
    printf("statement: %s\n", inBuf->data);
  }
  fflush(stdout);

  return 'Q';
}

   
                                                        
   
                                                                            
                                                     
   
static int
interactive_getc(void)
{
  int c;

     
                                                                     
                                                                          
                                                                     
                                                    
     
  CHECK_FOR_INTERRUPTS();

  c = getc(stdin);

  ProcessClientReadInterrupt(false);

  return c;
}

                    
                                                               
   
                                                                          
   
                                              
                    
   
static int
SocketBackend(StringInfo inBuf)
{
  int qtype;

     
                                              
     
  HOLD_CANCEL_INTERRUPTS();
  pq_startmsgread();
  qtype = pq_getbyte();

  if (qtype == EOF)                            
  {
    if (IsTransactionState())
    {
      ereport(COMMERROR, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("unexpected EOF on client connection with an open transaction")));
    }
    else
    {
         
                                                                      
                                                                  
                            
         
      whereToSendOutput = DestNone;
      ereport(DEBUG1, (errcode(ERRCODE_CONNECTION_DOES_NOT_EXIST), errmsg("unexpected EOF on client connection")));
    }
    return qtype;
  }

     
                                                                            
                                                                             
                                       
     
                                                                             
                          
     
  switch (qtype)
  {
  case 'Q':                   
    doing_extended_query_message = false;
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
    {
                                                  
      if (pq_getstring(inBuf))
      {
        if (IsTransactionState())
        {
          ereport(COMMERROR, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("unexpected EOF on client connection with an open transaction")));
        }
        else
        {
             
                                                             
                                                             
                                                      
             
          whereToSendOutput = DestNone;
          ereport(DEBUG1, (errcode(ERRCODE_CONNECTION_DOES_NOT_EXIST), errmsg("unexpected EOF on client connection")));
        }
        return EOF;
      }
    }
    break;

  case 'F':                             
    doing_extended_query_message = false;
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
    {
      if (GetOldFunctionMessage(inBuf))
      {
        if (IsTransactionState())
        {
          ereport(COMMERROR, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("unexpected EOF on client connection with an open transaction")));
        }
        else
        {
             
                                                             
                                                             
                                                      
             
          whereToSendOutput = DestNone;
          ereport(DEBUG1, (errcode(ERRCODE_CONNECTION_DOES_NOT_EXIST), errmsg("unexpected EOF on client connection")));
        }
        return EOF;
      }
    }
    break;

  case 'X':                
    doing_extended_query_message = false;
    ignore_till_sync = false;
    break;

  case 'B':           
  case 'C':            
  case 'D':               
  case 'E':              
  case 'H':            
  case 'P':            
    doing_extended_query_message = true;
                                            
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
    {
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid frontend message type %d", qtype)));
    }
    break;

  case 'S':           
                                        
    ignore_till_sync = false;
                                                                   
    doing_extended_query_message = false;
                                  
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
    {
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid frontend message type %d", qtype)));
    }
    break;

  case 'd':                
  case 'c':                
  case 'f':                
    doing_extended_query_message = false;
                                            
    if (PG_PROTOCOL_MAJOR(FrontendProtocol) < 3)
    {
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid frontend message type %d", qtype)));
    }
    break;

  default:

       
                                                                     
                                                                      
                                       
       
    ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid frontend message type %d", qtype)));
    break;
  }

     
                                                                          
                                                                            
               
     
  if (PG_PROTOCOL_MAJOR(FrontendProtocol) >= 3)
  {
    if (pq_getmessage(inBuf, 0))
    {
      return EOF;                                      
    }
  }
  else
  {
    pq_endmsgread();
  }
  RESUME_CANCEL_INTERRUPTS();

  return qtype;
}

                    
                                                            
                                                        
                                                   
                                    
                    
   
static int
ReadCommand(StringInfo inBuf)
{
  int result;

  if (whereToSendOutput == DestRemote)
  {
    result = SocketBackend(inBuf);
  }
  else
  {
    result = InteractiveBackend(inBuf);
  }
  return result;
}

   
                                                                              
   
                                                         
                                                                            
                                           
   
                        
   
void
ProcessClientReadInterrupt(bool blocked)
{
  int save_errno = errno;

  if (DoingCommandRead)
  {
                                                                        
    CHECK_FOR_INTERRUPTS();

                                                   
    if (catchupInterruptPending)
    {
      ProcessCatchupInterrupt();
    }

                                           
    if (notifyInterruptPending)
    {
      ProcessNotifyInterrupt();
    }
  }
  else if (ProcDiePending)
  {
       
                                                                           
                                                                        
                                                                       
                                                                       
                                                                      
                                 
       
    if (blocked)
    {
      CHECK_FOR_INTERRUPTS();
    }
    else
    {
      SetLatch(MyLatch);
    }
  }

  errno = save_errno;
}

   
                                                                                
   
                                                          
                                                                       
                                            
   
                        
   
void
ProcessClientWriteInterrupt(bool blocked)
{
  int save_errno = errno;

  if (ProcDiePending)
  {
       
                                                                          
                                                                          
                                                                       
                                                                       
                                                                       
                                                                      
                                 
       
    if (blocked)
    {
         
                                                                         
                                 
         
      if (InterruptHoldoffCount == 0 && CritSectionCount == 0)
      {
           
                                                                     
                                                                   
                                                                     
                                            
           
        if (whereToSendOutput == DestRemote)
        {
          whereToSendOutput = DestNone;
        }

        CHECK_FOR_INTERRUPTS();
      }
    }
    else
    {
      SetLatch(MyLatch);
    }
  }

  errno = save_errno;
}

   
                          
   
                                                                          
                                          
   
                                                                       
                                                                        
                                                                          
                                                                        
                                                                           
                                                                    
   
List *
pg_parse_query(const char *query_string)
{
  List *raw_parsetree_list;

  TRACE_POSTGRESQL_QUERY_PARSE_START(query_string);

  if (log_parser_stats)
  {
    ResetUsage();
  }

  raw_parsetree_list = raw_parser(query_string);

  if (log_parser_stats)
  {
    ShowUsage("PARSER STATISTICS");
  }

#ifdef COPY_PARSE_PLAN_TREES
                                                                          
  {
    List *new_list = copyObject(raw_parsetree_list);

                                                                   
    if (!equal(new_list, raw_parsetree_list))
    {
      elog(WARNING, "copyObject() failed to produce an equal raw parse tree");
    }
    else
    {
      raw_parsetree_list = new_list;
    }
  }
#endif

     
                                                                         
                                                                          
           
     

  TRACE_POSTGRESQL_QUERY_PARSE_DONE(query_string);

  return raw_parsetree_list;
}

   
                                                                           
                                                                               
   
                                                                       
                                               
   
                                                                              
   
List *
pg_analyze_and_rewrite(RawStmt *parsetree, const char *query_string, Oid *paramTypes, int numParams, QueryEnvironment *queryEnv)
{
  Query *query;
  List *querytree_list;

  TRACE_POSTGRESQL_QUERY_REWRITE_START(query_string);

     
                                 
     
  if (log_parser_stats)
  {
    ResetUsage();
  }

  query = parse_analyze(parsetree, query_string, paramTypes, numParams, queryEnv);

  if (log_parser_stats)
  {
    ShowUsage("PARSE ANALYSIS STATISTICS");
  }

     
                                           
     
  querytree_list = pg_rewrite_query(query);

  TRACE_POSTGRESQL_QUERY_REWRITE_DONE(query_string);

  return querytree_list;
}

   
                                                                                
                                                                              
                                                         
   
List *
pg_analyze_and_rewrite_params(RawStmt *parsetree, const char *query_string, ParserSetupHook parserSetup, void *parserSetupArg, QueryEnvironment *queryEnv)
{
  ParseState *pstate;
  Query *query;
  List *querytree_list;

  Assert(query_string != NULL);                         

  TRACE_POSTGRESQL_QUERY_REWRITE_START(query_string);

     
                                 
     
  if (log_parser_stats)
  {
    ResetUsage();
  }

  pstate = make_parsestate(NULL);
  pstate->p_sourcetext = query_string;
  pstate->p_queryEnv = queryEnv;
  (*parserSetup)(pstate, parserSetupArg);

  query = transformTopLevelStmt(pstate, parsetree);

  if (post_parse_analyze_hook)
  {
    (*post_parse_analyze_hook)(pstate, query);
  }

  free_parsestate(pstate);

  if (log_parser_stats)
  {
    ShowUsage("PARSE ANALYSIS STATISTICS");
  }

     
                                           
     
  querytree_list = pg_rewrite_query(query);

  TRACE_POSTGRESQL_QUERY_REWRITE_DONE(query_string);

  return querytree_list;
}

   
                                                            
   
                                                                         
                                
   
static List *
pg_rewrite_query(Query *query)
{
  List *querytree_list;

  if (Debug_print_parse)
  {
    elog_node_display(LOG, "parse tree", query, Debug_pretty_print);
  }

  if (log_parser_stats)
  {
    ResetUsage();
  }

  if (query->commandType == CMD_UTILITY)
  {
                                                                 
    querytree_list = list_make1(query);
  }
  else
  {
                                 
    querytree_list = QueryRewrite(query);
  }

  if (log_parser_stats)
  {
    ShowUsage("REWRITER STATISTICS");
  }

#ifdef COPY_PARSE_PLAN_TREES
                                                                     
  {
    List *new_list;

    new_list = copyObject(querytree_list);
                                                                   
    if (!equal(new_list, querytree_list))
    {
      elog(WARNING, "copyObject() failed to produce equal parse tree");
    }
    else
    {
      querytree_list = new_list;
    }
  }
#endif

#ifdef WRITE_READ_PARSE_PLAN_TREES
                                                                           
  {
    List *new_list = NIL;
    ListCell *lc;

       
                                                                     
                                                                           
       
    foreach (lc, querytree_list)
    {
      Query *query = castNode(Query, lfirst(lc));

      if (query->commandType != CMD_UTILITY)
      {
        char *str = nodeToString(query);
        Query *new_query = stringToNodeWithLocations(str);

           
                                                                      
                                                         
           
        new_query->queryId = query->queryId;

        new_list = lappend(new_list, new_query);
        pfree(str);
      }
      else
      {
        new_list = lappend(new_list, query);
      }
    }

                                                                         
    if (!equal(new_list, querytree_list))
    {
      elog(WARNING, "outfuncs/readfuncs failed to produce equal parse tree");
    }
    else
    {
      querytree_list = new_list;
    }
  }
#endif

  if (Debug_print_rewritten)
  {
    elog_node_display(LOG, "rewritten parse tree", querytree_list, Debug_pretty_print);
  }

  return querytree_list;
}

   
                                                         
                                                                          
   
PlannedStmt *
pg_plan_query(Query *querytree, int cursorOptions, ParamListInfo boundParams)
{
  PlannedStmt *plan;

                                       
  if (querytree->commandType == CMD_UTILITY)
  {
    return NULL;
  }

                                                                             
  Assert(ActiveSnapshotSet());

  TRACE_POSTGRESQL_QUERY_PLAN_START();

  if (log_planner_stats)
  {
    ResetUsage();
  }

                          
  plan = planner(querytree, cursorOptions, boundParams);

  if (log_planner_stats)
  {
    ShowUsage("PLANNER STATISTICS");
  }

#ifdef COPY_PARSE_PLAN_TREES
                                                                     
  {
    PlannedStmt *new_plan = copyObject(plan);

       
                                                                          
                                                              
       
#ifdef NOT_USED
                                                                   
    if (!equal(new_plan, plan))
    {
      elog(WARNING, "copyObject() failed to produce an equal plan tree");
    }
    else
#endif
      plan = new_plan;
  }
#endif

#ifdef WRITE_READ_PARSE_PLAN_TREES
                                                                           
  {
    char *str;
    PlannedStmt *new_plan;

    str = nodeToString(plan);
    new_plan = stringToNodeWithLocations(str);
    pfree(str);

       
                                                                          
                                                              
       
#ifdef NOT_USED
                                                                         
    if (!equal(new_plan, plan))
    {
      elog(WARNING, "outfuncs/readfuncs failed to produce an equal plan tree");
    }
    else
#endif
      plan = new_plan;
  }
#endif

     
                              
     
  if (Debug_print_plan)
  {
    elog_node_display(LOG, "plan", plan, Debug_pretty_print);
  }

  TRACE_POSTGRESQL_QUERY_PLAN_DONE();

  return plan;
}

   
                                                           
   
                                                                       
                                                     
   
                                              
   
List *
pg_plan_queries(List *querytrees, int cursorOptions, ParamListInfo boundParams)
{
  List *stmt_list = NIL;
  ListCell *query_list;

  foreach (query_list, querytrees)
  {
    Query *query = lfirst_node(Query, query_list);
    PlannedStmt *stmt;

    if (query->commandType == CMD_UTILITY)
    {
                                                 
      stmt = makeNode(PlannedStmt);
      stmt->commandType = CMD_UTILITY;
      stmt->canSetTag = query->canSetTag;
      stmt->utilityStmt = query->utilityStmt;
      stmt->stmt_location = query->stmt_location;
      stmt->stmt_len = query->stmt_len;
    }
    else
    {
      stmt = pg_plan_query(query, cursorOptions, boundParams);
    }

    stmt_list = lappend(stmt_list, stmt);
  }

  return stmt_list;
}

   
                     
   
                                              
   
static void
exec_simple_query(const char *query_string)
{
  CommandDest dest = whereToSendOutput;
  MemoryContext oldcontext;
  List *parsetree_list;
  ListCell *parsetree_item;
  bool save_log_statement_stats = log_statement_stats;
  bool was_logged = false;
  bool use_implicit_block;
  char msec_str[32];

     
                                                    
     
  debug_query_string = query_string;

  pgstat_report_activity(STATE_RUNNING, query_string);

  TRACE_POSTGRESQL_QUERY_START(query_string);

     
                                                                           
                                               
     
  if (save_log_statement_stats)
  {
    ResetUsage();
  }

     
                                                                   
                                                                         
                                                                             
                                                                          
                                                   
     
  start_xact_command();

     
                                                                             
                                                                         
                                                                             
                          
     
  drop_unnamed_stmt();

     
                                                                
     
  oldcontext = MemoryContextSwitchTo(MessageContext);

     
                                                                           
                                           
     
  parsetree_list = pg_parse_query(query_string);

                                                    
  if (check_log_statement(parsetree_list))
  {
    ereport(LOG, (errmsg("statement: %s", query_string), errhidestmt(true), errdetail_execute(parsetree_list)));
    was_logged = true;
  }

     
                                                           
     
  MemoryContextSwitchTo(oldcontext);

     
                                                                       
                                                                             
                                                                       
                                                                       
                                                                          
                        
     
  use_implicit_block = (list_length(parsetree_list) > 1);

     
                                                            
     
  foreach (parsetree_item, parsetree_list)
  {
    RawStmt *parsetree = lfirst_node(RawStmt, parsetree_item);
    bool snapshot_set = false;
    const char *commandTag;
    char completionTag[COMPLETION_TAG_BUFSIZE];
    List *querytree_list, *plantree_list;
    Portal portal;
    DestReceiver *receiver;
    int16 format;

       
                                                                           
                                                                          
                                                                    
                    
       
    commandTag = CreateCommandTag(parsetree->stmt);

    set_ps_display(commandTag, false);

    BeginCommand(commandTag, dest);

       
                                                                       
                                                                         
                                                                          
                                                                       
                                                                       
                               
       
    if (IsAbortedTransactionBlockState() && !IsTransactionExitStmt(parsetree->stmt))
    {
      ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION),
                         errmsg("current transaction is aborted, "
                                "commands ignored until end of transaction block"),
                         errdetail_abort()));
    }

                                                   
    start_xact_command();

       
                                                                          
                                                                          
                                                                         
                                                                       
                                                             
       
    if (use_implicit_block)
    {
      BeginImplicitTransactionBlock();
    }

                                                                     
    CHECK_FOR_INTERRUPTS();

       
                                                                   
       
    if (analyze_requires_snapshot(parsetree))
    {
      PushActiveSnapshot(GetTransactionSnapshot());
      snapshot_set = true;
    }

       
                                                    
       
                                                                         
                                                  
       
    oldcontext = MemoryContextSwitchTo(MessageContext);

    querytree_list = pg_analyze_and_rewrite(parsetree, query_string, NULL, 0, NULL);

    plantree_list = pg_plan_queries(querytree_list, CURSOR_OPT_PARALLEL_OK, NULL);

                                                          
    if (snapshot_set)
    {
      PopActiveSnapshot();
    }

                                                                 
    CHECK_FOR_INTERRUPTS();

       
                                                                      
                                         
       
    portal = CreatePortal("", true, true);
                                                
    portal->visible = false;

       
                                                                          
                                                                        
                      
       
    PortalDefineQuery(portal, NULL, query_string, commandTag, plantree_list, NULL);

       
                                              
       
    PortalStart(portal, NULL, 0, InvalidSnapshot);

       
                                                                        
                                                                           
                                                                      
                                  
       
    format = 0;                      
    if (IsA(parsetree->stmt, FetchStmt))
    {
      FetchStmt *stmt = (FetchStmt *)parsetree->stmt;

      if (!stmt->ismove)
      {
        Portal fportal = GetPortalByName(stmt->portalname);

        if (PortalIsValid(fportal) && (fportal->cursorOptions & CURSOR_OPT_BINARY))
        {
          format = 1;             
        }
      }
    }
    PortalSetResultFormat(portal, 1, &format);

       
                                                          
       
    receiver = CreateDestReceiver(dest);
    if (dest == DestRemote)
    {
      SetRemoteDestReceiverParams(receiver, portal);
    }

       
                                                         
       
    MemoryContextSwitchTo(oldcontext);

       
                                                                          
       
    (void)PortalRun(portal, FETCH_ALL, true,                       
        true, receiver, receiver, completionTag);

    receiver->rDestroy(receiver);

    PortalDrop(portal, false);

    if (lnext(parsetree_item) == NULL)
    {
         
                                                                       
                                                                        
                                                                      
                                                                    
                                                                         
                                                                     
                                                                   
         
      if (use_implicit_block)
      {
        EndImplicitTransactionBlock();
      }
      finish_xact_command();
    }
    else if (IsA(parsetree->stmt, TransactionStmt))
    {
         
                                                                         
                                                        
         
      finish_xact_command();
    }
    else
    {
         
                                                                     
                                                                 
                                                                       
         
      Assert(!(MyXactFlags & XACT_FLAGS_NEEDIMMEDIATECOMMIT));

         
                                                                     
                                                      
         
      CommandCounterIncrement();
    }

       
                                                                          
                                                                           
                                                                        
                                                                    
       
    EndCommand(completionTag, dest);
  }                               

     
                                                                           
                                                                        
                                
     
  finish_xact_command();

     
                                                                     
     
  if (!parsetree_list)
  {
    NullCommand(dest);
  }

     
                                           
     
  switch (check_log_duration(msec_str, was_logged))
  {
  case 1:
    ereport(LOG, (errmsg("duration: %s ms", msec_str), errhidestmt(true)));
    break;
  case 2:
    ereport(LOG, (errmsg("duration: %s ms  statement: %s", msec_str, query_string), errhidestmt(true), errdetail_execute(parsetree_list)));
    break;
  }

  if (save_log_statement_stats)
  {
    ShowUsage("QUERY STATISTICS");
  }

  TRACE_POSTGRESQL_QUERY_DONE(query_string);

  debug_query_string = NULL;
}

   
                      
   
                                       
   
static void
exec_parse_message(const char *query_string,                        
    const char *stmt_name,                                               
    Oid *paramTypes,                                              
    int numParams)                                                     
{
  MemoryContext unnamed_stmt_context = NULL;
  MemoryContext oldcontext;
  List *parsetree_list;
  RawStmt *raw_parse_tree;
  const char *commandTag;
  List *querytree_list;
  CachedPlanSource *psrc;
  bool is_named;
  bool save_log_statement_stats = log_statement_stats;
  char msec_str[32];

     
                                                    
     
  debug_query_string = query_string;

  pgstat_report_activity(STATE_RUNNING, query_string);

  set_ps_display("PARSE", false);

  if (save_log_statement_stats)
  {
    ResetUsage();
  }

  ereport(DEBUG2, (errmsg("parse %s: %s", *stmt_name ? stmt_name : "<unnamed>", query_string)));

     
                                                                            
                                                                             
                                                                        
                
     
  start_xact_command();

     
                                                                
     
                                                                           
                                                                     
                                                                  
                                                                            
                                                                            
                                                                            
                                                                          
                                                                            
                                                              
     
  is_named = (stmt_name[0] != '\0');
  if (is_named)
  {
                                                              
    oldcontext = MemoryContextSwitchTo(MessageContext);
  }
  else
  {
                                                                       
    drop_unnamed_stmt();
                                    
    unnamed_stmt_context = AllocSetContextCreate(MessageContext, "unnamed prepared statement", ALLOCSET_DEFAULT_SIZES);
    oldcontext = MemoryContextSwitchTo(unnamed_stmt_context);
  }

     
                                                                           
                                           
     
  parsetree_list = pg_parse_query(query_string);

     
                                                                            
                                                                         
                                                          
     
  if (list_length(parsetree_list) > 1)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot insert multiple commands into a prepared statement")));
  }

  if (parsetree_list != NIL)
  {
    Query *query;
    bool snapshot_set = false;

    raw_parse_tree = linitial_node(RawStmt, parsetree_list);

       
                                                                
       
    commandTag = CreateCommandTag(raw_parse_tree->stmt);

       
                                                                       
                                                                        
                                                                       
                                                                          
                                                                           
                               
       
    if (IsAbortedTransactionBlockState() && !IsTransactionExitStmt(raw_parse_tree->stmt))
    {
      ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION),
                         errmsg("current transaction is aborted, "
                                "commands ignored until end of transaction block"),
                         errdetail_abort()));
    }

       
                                                                         
                                                   
       
    psrc = CreateCachedPlan(raw_parse_tree, query_string, commandTag);

       
                                                          
       
    if (analyze_requires_snapshot(raw_parse_tree))
    {
      PushActiveSnapshot(GetTransactionSnapshot());
      snapshot_set = true;
    }

       
                                                                          
                                                                       
                                  
       
    if (log_parser_stats)
    {
      ResetUsage();
    }

    query = parse_analyze_varparams(raw_parse_tree, query_string, &paramTypes, &numParams);

       
                                                 
       
    for (int i = 0; i < numParams; i++)
    {
      Oid ptype = paramTypes[i];

      if (ptype == InvalidOid || ptype == UNKNOWNOID)
      {
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_DATATYPE), errmsg("could not determine data type of parameter $%d", i + 1)));
      }
    }

    if (log_parser_stats)
    {
      ShowUsage("PARSE ANALYSIS STATISTICS");
    }

    querytree_list = pg_rewrite_query(query);

                                                 
    if (snapshot_set)
    {
      PopActiveSnapshot();
    }
  }
  else
  {
                                             
    raw_parse_tree = NULL;
    commandTag = NULL;
    psrc = CreateCachedPlan(raw_parse_tree, query_string, commandTag);
    querytree_list = NIL;
  }

     
                                                                         
                                                                         
                                                                             
            
     
  if (unnamed_stmt_context)
  {
    MemoryContextSetParent(psrc->context, MessageContext);
  }

                                              
  CompleteCachedPlan(psrc, querytree_list, unnamed_stmt_context, paramTypes, numParams, NULL, NULL, CURSOR_OPT_PARALLEL_OK,                          
      true);                                                                                                                                  

                                                       
  CHECK_FOR_INTERRUPTS();

  if (is_named)
  {
       
                                                
       
    StorePreparedStatement(stmt_name, psrc, false);
  }
  else
  {
       
                                                                 
       
    SaveCachedPlan(psrc);
    unnamed_stmt_psrc = psrc;
  }

  MemoryContextSwitchTo(oldcontext);

     
                                                                          
                                                                           
                                                   
     
  CommandCounterIncrement();

     
                         
     
  if (whereToSendOutput == DestRemote)
  {
    pq_putemptymessage('1');
  }

     
                                           
     
  switch (check_log_duration(msec_str, false))
  {
  case 1:
    ereport(LOG, (errmsg("duration: %s ms", msec_str), errhidestmt(true)));
    break;
  case 2:
    ereport(LOG, (errmsg("duration: %s ms  parse %s: %s", msec_str, *stmt_name ? stmt_name : "<unnamed>", query_string), errhidestmt(true)));
    break;
  }

  if (save_log_statement_stats)
  {
    ShowUsage("PARSE MESSAGE STATISTICS");
  }

  debug_query_string = NULL;
}

   
                     
   
                                                                         
   
static void
exec_bind_message(StringInfo input_message)
{
  const char *portal_name;
  const char *stmt_name;
  int numPFormats;
  int16 *pformats = NULL;
  int numParams;
  int numRFormats;
  int16 *rformats = NULL;
  CachedPlanSource *psrc;
  CachedPlan *cplan;
  Portal portal;
  char *query_string;
  char *saved_stmt_name;
  ParamListInfo params;
  MemoryContext oldContext;
  bool save_log_statement_stats = log_statement_stats;
  bool snapshot_set = false;
  char msec_str[32];

                                         
  portal_name = pq_getmsgstring(input_message);
  stmt_name = pq_getmsgstring(input_message);

  ereport(DEBUG2, (errmsg("bind %s to %s", *portal_name ? portal_name : "<unnamed>", *stmt_name ? stmt_name : "<unnamed>")));

                               
  if (stmt_name[0] != '\0')
  {
    PreparedStatement *pstmt;

    pstmt = FetchPreparedStatement(stmt_name, true);
    psrc = pstmt->plansource;
  }
  else
  {
                                            
    psrc = unnamed_stmt_psrc;
    if (!psrc)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PSTATEMENT), errmsg("unnamed prepared statement does not exist")));
    }
  }

     
                                                    
     
  debug_query_string = psrc->query_string;

  pgstat_report_activity(STATE_RUNNING, psrc->query_string);

  set_ps_display("BIND", false);

  if (save_log_statement_stats)
  {
    ResetUsage();
  }

     
                                                                             
                                                                           
                                                                     
                
     
  start_xact_command();

                                      
  MemoryContextSwitchTo(MessageContext);

                                      
  numPFormats = pq_getmsgint(input_message, 2);
  if (numPFormats > 0)
  {
    pformats = (int16 *)palloc(numPFormats * sizeof(int16));
    for (int i = 0; i < numPFormats; i++)
    {
      pformats[i] = pq_getmsgint(input_message, 2);
    }
  }

                                     
  numParams = pq_getmsgint(input_message, 2);

  if (numPFormats > 1 && numPFormats != numParams)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("bind message has %d parameter formats but %d parameters", numPFormats, numParams)));
  }

  if (numParams != psrc->num_params)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("bind message supplies %d parameters, but prepared statement \"%s\" requires %d", numParams, stmt_name, psrc->num_params)));
  }

     
                                                                     
                                                                       
                                                                          
                                                                       
                                                                          
                
     
  if (IsAbortedTransactionBlockState() && (!(psrc->raw_parse_tree && IsTransactionExitStmt(psrc->raw_parse_tree->stmt)) || numParams != 0))
  {
    ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION),
                       errmsg("current transaction is aborted, "
                              "commands ignored until end of transaction block"),
                       errdetail_abort()));
  }

     
                                                                             
                                         
     
  if (portal_name[0] == '\0')
  {
    portal = CreatePortal(portal_name, true, true);
  }
  else
  {
    portal = CreatePortal(portal_name, false, false);
  }

     
                                                                             
                                                                          
                                                             
                                                                             
     
  oldContext = MemoryContextSwitchTo(portal->portalContext);

                                                    
  query_string = pstrdup(psrc->query_string);

                                                                       
  if (stmt_name[0])
  {
    saved_stmt_name = pstrdup(stmt_name);
  }
  else
  {
    saved_stmt_name = NULL;
  }

     
                                                                    
                                                                        
                                                                            
                                                                          
                    
     
  if (numParams > 0 || (psrc->raw_parse_tree && analyze_requires_snapshot(psrc->raw_parse_tree)))
  {
    PushActiveSnapshot(GetTransactionSnapshot());
    snapshot_set = true;
  }

     
                                                                         
     
  if (numParams > 0)
  {
    params = makeParamList(numParams);

    for (int paramno = 0; paramno < numParams; paramno++)
    {
      Oid ptype = psrc->param_types[paramno];
      int32 plength;
      Datum pval;
      bool isNull;
      StringInfoData pbuf;
      char csave;
      int16 pformat;

      plength = pq_getmsgint(input_message, 4);
      isNull = (plength == -1);

      if (!isNull)
      {
        const char *pvalue = pq_getmsgbytes(input_message, plength);

           
                                                                   
                                                                     
                                                                       
                                                                 
                                                                
                                                      
           
        pbuf.data = unconstify(char *, pvalue);
        pbuf.maxlen = plength + 1;
        pbuf.len = plength;
        pbuf.cursor = 0;

        csave = pbuf.data[plength];
        pbuf.data[plength] = '\0';
      }
      else
      {
        pbuf.data = NULL;                          
        csave = 0;
      }

      if (numPFormats > 1)
      {
        pformat = pformats[paramno];
      }
      else if (numPFormats > 0)
      {
        pformat = pformats[0];
      }
      else
      {
        pformat = 0;                     
      }

      if (pformat == 0)                
      {
        Oid typinput;
        Oid typioparam;
        char *pstring;

        getTypeInputInfo(ptype, &typinput, &typioparam);

           
                                                                
                             
           
        if (isNull)
        {
          pstring = NULL;
        }
        else
        {
          pstring = pg_client_to_server(pbuf.data, plength);
        }

        pval = OidInputFunctionCall(typinput, pstring, typioparam, -1);

                                                        
        if (pstring && pstring != pbuf.data)
        {
          pfree(pstring);
        }
      }
      else if (pformat == 1)                  
      {
        Oid typreceive;
        Oid typioparam;
        StringInfo bufptr;

           
                                                            
           
        getTypeBinaryInputInfo(ptype, &typreceive, &typioparam);

        if (isNull)
        {
          bufptr = NULL;
        }
        else
        {
          bufptr = &pbuf;
        }

        pval = OidReceiveFunctionCall(typreceive, bufptr, typioparam, -1);

                                                       
        if (!isNull && pbuf.cursor != pbuf.len)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("incorrect binary data format in bind parameter %d", paramno + 1)));
        }
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unsupported format code: %d", pformat)));
        pval = 0;                          
      }

                                           
      if (!isNull)
      {
        pbuf.data[plength] = csave;
      }

      params->params[paramno].value = pval;
      params->params[paramno].isnull = isNull;

         
                                                                         
                                                 
         
      params->params[paramno].pflags = PARAM_FLAG_CONST;
      params->params[paramno].ptype = ptype;
    }
  }
  else
  {
    params = NULL;
  }

                                              
  MemoryContextSwitchTo(oldContext);

                                   
  numRFormats = pq_getmsgint(input_message, 2);
  if (numRFormats > 0)
  {
    rformats = (int16 *)palloc(numRFormats * sizeof(int16));
    for (int i = 0; i < numRFormats; i++)
    {
      rformats[i] = pq_getmsgint(input_message, 2);
    }
  }

  pq_getmsgend(input_message);

     
                                                                           
                                                                     
                                                                           
     
  cplan = GetCachedPlan(psrc, params, false, NULL);

     
                                   
     
                                                                        
                                        
     
  PortalDefineQuery(portal, saved_stmt_name, query_string, psrc->commandTag, cplan->stmt_list, cplan);

                                                                          
  if (snapshot_set)
  {
    PopActiveSnapshot();
  }

     
                                                
     
  PortalStart(portal, params, 0, InvalidSnapshot);

     
                                                     
     
  PortalSetResultFormat(portal, numRFormats, rformats);

     
                        
     
  if (whereToSendOutput == DestRemote)
  {
    pq_putemptymessage('2');
  }

     
                                           
     
  switch (check_log_duration(msec_str, false))
  {
  case 1:
    ereport(LOG, (errmsg("duration: %s ms", msec_str), errhidestmt(true)));
    break;
  case 2:
    ereport(LOG, (errmsg("duration: %s ms  bind %s%s%s: %s", msec_str, *stmt_name ? stmt_name : "<unnamed>", *portal_name ? "/" : "", *portal_name ? portal_name : "", psrc->query_string), errhidestmt(true), errdetail_params(params)));
    break;
  }

  if (save_log_statement_stats)
  {
    ShowUsage("BIND MESSAGE STATISTICS");
  }

  debug_query_string = NULL;
}

   
                        
   
                                             
   
static void
exec_execute_message(const char *portal_name, long max_rows)
{
  CommandDest dest;
  DestReceiver *receiver;
  Portal portal;
  bool completed;
  char completionTag[COMPLETION_TAG_BUFSIZE];
  const char *sourceText;
  const char *prepStmtName;
  ParamListInfo portalParams;
  bool save_log_statement_stats = log_statement_stats;
  bool is_xact_command;
  bool execute_is_fetch;
  bool was_logged = false;
  char msec_str[32];

                                                        
  dest = whereToSendOutput;
  if (dest == DestRemote)
  {
    dest = DestRemoteExecute;
  }

  portal = GetPortalByName(portal_name);
  if (!PortalIsValid(portal))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR), errmsg("portal \"%s\" does not exist", portal_name)));
  }

     
                                                          
                         
     
  if (portal->commandTag == NULL)
  {
    Assert(portal->stmts == NIL);
    NullCommand(dest);
    return;
  }

                                                      
  is_xact_command = IsTransactionStmtList(portal->stmts);

     
                                                                         
                                                                         
                                                                          
                        
     
  sourceText = pstrdup(portal->sourceText);
  if (portal->prepStmtName)
  {
    prepStmtName = pstrdup(portal->prepStmtName);
  }
  else
  {
    prepStmtName = "<unnamed>";
  }
  portalParams = portal->portalParams;

     
                                                    
     
  debug_query_string = sourceText;

  pgstat_report_activity(STATE_RUNNING, sourceText);

  set_ps_display(portal->commandTag, false);

  if (save_log_statement_stats)
  {
    ResetUsage();
  }

  BeginCommand(portal->commandTag, dest);

     
                                                                             
                                                                       
     
  receiver = CreateDestReceiver(dest);
  if (dest == DestRemoteExecute)
  {
    SetRemoteDestReceiverParams(receiver, portal);
  }

     
                                                                         
                                      
     
  start_xact_command();

     
                                                                            
                                                                             
                                                                             
                                 
     
  execute_is_fetch = !portal->atStart;

                                                    
  if (check_log_statement(portal->stmts))
  {
    ereport(LOG, (errmsg("%s %s%s%s: %s", execute_is_fetch ? _("execute fetch from") : _("execute"), prepStmtName, *portal_name ? "/" : "", *portal_name ? portal_name : "", sourceText), errhidestmt(true), errdetail_params(portalParams)));
    was_logged = true;
  }

     
                                                                     
                                                                    
     
  if (IsAbortedTransactionBlockState() && !IsTransactionExitStmtList(portal->stmts))
  {
    ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION),
                       errmsg("current transaction is aborted, "
                              "commands ignored until end of transaction block"),
                       errdetail_abort()));
  }

                                                         
  CHECK_FOR_INTERRUPTS();

     
                             
     
  if (max_rows <= 0)
  {
    max_rows = FETCH_ALL;
  }

  completed = PortalRun(portal, max_rows, true,                       
      !execute_is_fetch && max_rows == FETCH_ALL, receiver, receiver, completionTag);

  receiver->rDestroy(receiver);

  if (completed)
  {
    if (is_xact_command || (MyXactFlags & XACT_FLAGS_NEEDIMMEDIATECOMMIT))
    {
         
                                                                     
                                                                      
                                                                       
                                                                
                                                                 
                                               
         
      finish_xact_command();

         
                                                                         
                                                                     
                                                                       
         
      portalParams = NULL;
    }
    else
    {
         
                                                                     
                                                      
         
      CommandCounterIncrement();

         
                                                                   
                                                                 
         
      MyXactFlags |= XACT_FLAGS_PIPELINING;

                                                         
      disable_statement_timeout();
    }

                                                    
    EndCommand(completionTag, dest);
  }
  else
  {
                                                          
    if (whereToSendOutput == DestRemote)
    {
      pq_putemptymessage('s');
    }

       
                                                                         
            
       
    MyXactFlags |= XACT_FLAGS_PIPELINING;
  }

     
                                           
     
  switch (check_log_duration(msec_str, was_logged))
  {
  case 1:
    ereport(LOG, (errmsg("duration: %s ms", msec_str), errhidestmt(true)));
    break;
  case 2:
    ereport(LOG, (errmsg("duration: %s ms  %s %s%s%s: %s", msec_str, execute_is_fetch ? _("execute fetch from") : _("execute"), prepStmtName, *portal_name ? "/" : "", *portal_name ? portal_name : "", sourceText), errhidestmt(true), errdetail_params(portalParams)));
    break;
  }

  if (save_log_statement_stats)
  {
    ShowUsage("EXECUTE MESSAGE STATISTICS");
  }

  debug_query_string = NULL;
}

   
                       
                                                                        
   
                                                                   
              
   
static bool
check_log_statement(List *stmt_list)
{
  ListCell *stmt_item;

  if (log_statement == LOGSTMT_NONE)
  {
    return false;
  }
  if (log_statement == LOGSTMT_ALL)
  {
    return true;
  }

                                                                      
  foreach (stmt_item, stmt_list)
  {
    Node *stmt = (Node *)lfirst(stmt_item);

    if (GetCommandLogLevel(stmt) <= log_statement)
    {
      return true;
    }
  }

  return false;
}

   
                      
                                                                  
                                                                       
                                  
   
            
                              
                                            
                                                     
   
                                                                            
                                   
   
                                                                          
                                                
   
int
check_log_duration(char *msec_str, bool was_logged)
{
  if (log_duration || log_min_duration_statement >= 0 || xact_is_sampled)
  {
    long secs;
    int usecs;
    int msecs;
    bool exceeded;

    TimestampDifference(GetCurrentStatementStartTimestamp(), GetCurrentTimestamp(), &secs, &usecs);
    msecs = usecs / 1000;

       
                                                                           
                                                                       
                                                                          
       
    exceeded = (log_min_duration_statement == 0 || (log_min_duration_statement > 0 && (secs > log_min_duration_statement / 1000 || secs * 1000 + msecs >= log_min_duration_statement)));

    if (exceeded || log_duration || xact_is_sampled)
    {
      snprintf(msec_str, 32, "%ld.%03d", secs * 1000 + msecs, usecs % 1000);
      if ((exceeded || xact_is_sampled) && !was_logged)
      {
        return 2;
      }
      else
      {
        return 1;
      }
    }
  }

  return 0;
}

   
                     
   
                                                                               
                                           
   
static int
errdetail_execute(List *raw_parsetree_list)
{
  ListCell *parsetree_item;

  foreach (parsetree_item, raw_parsetree_list)
  {
    RawStmt *parsetree = lfirst_node(RawStmt, parsetree_item);

    if (IsA(parsetree->stmt, ExecuteStmt))
    {
      ExecuteStmt *stmt = (ExecuteStmt *)parsetree->stmt;
      PreparedStatement *pstmt;

      pstmt = FetchPreparedStatement(stmt->name, false);
      if (pstmt)
      {
        errdetail("prepare: %s", pstmt->plansource->query_string);
        return 0;
      }
    }
  }

  return 0;
}

   
                    
   
                                                                      
   
static int
errdetail_params(ParamListInfo params)
{
                                                                          
  if (params && params->numParams > 0 && !IsAbortedTransactionBlockState())
  {
    StringInfoData param_str;
    MemoryContext oldcontext;

                                                       
    Assert(params->paramFetch == NULL);

                                                            
    oldcontext = MemoryContextSwitchTo(MessageContext);

    initStringInfo(&param_str);

    for (int paramno = 0; paramno < params->numParams; paramno++)
    {
      ParamExternData *prm = &params->params[paramno];
      Oid typoutput;
      bool typisvarlena;
      char *pstring;
      char *p;

      appendStringInfo(&param_str, "%s$%d = ", paramno > 0 ? ", " : "", paramno + 1);

      if (prm->isnull || !OidIsValid(prm->ptype))
      {
        appendStringInfoString(&param_str, "NULL");
        continue;
      }

      getTypeOutputInfo(prm->ptype, &typoutput, &typisvarlena);

      pstring = OidOutputFunctionCall(typoutput, prm->value);

      appendStringInfoCharMacro(&param_str, '\'');
      for (p = pstring; *p; p++)
      {
        if (*p == '\'')                           
        {
          appendStringInfoCharMacro(&param_str, *p);
        }
        appendStringInfoCharMacro(&param_str, *p);
      }
      appendStringInfoCharMacro(&param_str, '\'');

      pfree(pstring);
    }

    errdetail("parameters: %s", param_str.data);

    pfree(param_str.data);

    MemoryContextSwitchTo(oldcontext);
  }

  return 0;
}

   
                   
   
                                                         
   
static int
errdetail_abort(void)
{
  if (MyProc->recoveryConflictPending)
  {
    errdetail("abort reason: recovery conflict");
  }

  return 0;
}

   
                               
   
                                                    
   
static int
errdetail_recovery_conflict(void)
{
  switch (RecoveryConflictReason)
  {
  case PROCSIG_RECOVERY_CONFLICT_BUFFERPIN:
    errdetail("User was holding shared buffer pin for too long.");
    break;
  case PROCSIG_RECOVERY_CONFLICT_LOCK:
    errdetail("User was holding a relation lock for too long.");
    break;
  case PROCSIG_RECOVERY_CONFLICT_TABLESPACE:
    errdetail("User was or might have been using tablespace that must be dropped.");
    break;
  case PROCSIG_RECOVERY_CONFLICT_SNAPSHOT:
    errdetail("User query might have needed to see row versions that must be removed.");
    break;
  case PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK:
    errdetail("User transaction caused buffer deadlock with recovery.");
    break;
  case PROCSIG_RECOVERY_CONFLICT_DATABASE:
    errdetail("User was connected to a database that must be dropped.");
    break;
  default:
    break;
                      
  }

  return 0;
}

   
                                   
   
                                                         
   
static void
exec_describe_statement_message(const char *stmt_name)
{
  CachedPlanSource *psrc;

     
                                                                          
                                                                        
     
  start_xact_command();

                                      
  MemoryContextSwitchTo(MessageContext);

                               
  if (stmt_name[0] != '\0')
  {
    PreparedStatement *pstmt;

    pstmt = FetchPreparedStatement(stmt_name, true);
    psrc = pstmt->plansource;
  }
  else
  {
                                            
    psrc = unnamed_stmt_psrc;
    if (!psrc)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_PSTATEMENT), errmsg("unnamed prepared statement does not exist")));
    }
  }

                                                                  
  Assert(psrc->fixed_result);

     
                                                          
                                                                       
                                                                           
                                                                           
                                                                         
                                                                      
                                                                  
     
  if (IsAbortedTransactionBlockState() && psrc->resultDesc)
  {
    ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION),
                       errmsg("current transaction is aborted, "
                              "commands ignored until end of transaction block"),
                       errdetail_abort()));
  }

  if (whereToSendOutput != DestRemote)
  {
    return;                                    
  }

     
                                      
     
  pq_beginmessage_reuse(&row_description_buf, 't');                          
                                                                      
  pq_sendint16(&row_description_buf, psrc->num_params);

  for (int i = 0; i < psrc->num_params; i++)
  {
    Oid ptype = psrc->param_types[i];

    pq_sendint32(&row_description_buf, (int)ptype);
  }
  pq_endmessage_reuse(&row_description_buf);

     
                                                                  
     
  if (psrc->resultDesc)
  {
    List *tlist;

                                           
    tlist = CachedPlanGetTargetList(psrc, NULL);

    SendRowDescriptionMessage(&row_description_buf, psrc->resultDesc, tlist, NULL);
  }
  else
  {
    pq_putemptymessage('n');             
  }
}

   
                                
   
                                             
   
static void
exec_describe_portal_message(const char *portal_name)
{
  Portal portal;

     
                                                                          
                                                                        
     
  start_xact_command();

                                      
  MemoryContextSwitchTo(MessageContext);

  portal = GetPortalByName(portal_name);
  if (!PortalIsValid(portal))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR), errmsg("portal \"%s\" does not exist", portal_name)));
  }

     
                                                          
                                                                       
                                                                             
                                                                      
                                                                         
                                          
     
  if (IsAbortedTransactionBlockState() && portal->tupDesc)
  {
    ereport(ERROR, (errcode(ERRCODE_IN_FAILED_SQL_TRANSACTION),
                       errmsg("current transaction is aborted, "
                              "commands ignored until end of transaction block"),
                       errdetail_abort()));
  }

  if (whereToSendOutput != DestRemote)
  {
    return;                                    
  }

  if (portal->tupDesc)
  {
    SendRowDescriptionMessage(&row_description_buf, portal->tupDesc, FetchPortalTargetList(portal), portal->formats);
  }
  else
  {
    pq_putemptymessage('n');             
  }
}

   
                                                                  
   
static void
start_xact_command(void)
{
  if (!xact_started)
  {
    StartTransactionCommand();

    xact_started = true;
  }

     
                                                                            
                                                                            
                                                                          
                                                                             
                                                             
     
  enable_statement_timeout();
}

static void
finish_xact_command(void)
{
                                                          
  disable_statement_timeout();

  if (xact_started)
  {
    CommitTransactionCommand();

#ifdef MEMORY_CONTEXT_CHECKING
                                                                    
                                                              
    MemoryContextCheck(TopMemoryContext);
#endif

#ifdef SHOW_MEMORY_STATS
                                                             
    MemoryContextStats(TopMemoryContext);
#endif

    xact_started = false;
  }
}

   
                                                                       
                                                    
   

                           
static bool
IsTransactionExitStmt(Node *parsetree)
{
  if (parsetree && IsA(parsetree, TransactionStmt))
  {
    TransactionStmt *stmt = (TransactionStmt *)parsetree;

    if (stmt->kind == TRANS_STMT_COMMIT || stmt->kind == TRANS_STMT_PREPARE || stmt->kind == TRANS_STMT_ROLLBACK || stmt->kind == TRANS_STMT_ROLLBACK_TO)
    {
      return true;
    }
  }
  return false;
}

                                                 
static bool
IsTransactionExitStmtList(List *pstmts)
{
  if (list_length(pstmts) == 1)
  {
    PlannedStmt *pstmt = linitial_node(PlannedStmt, pstmts);

    if (pstmt->commandType == CMD_UTILITY && IsTransactionExitStmt(pstmt->utilityStmt))
    {
      return true;
    }
  }
  return false;
}

                                                 
static bool
IsTransactionStmtList(List *pstmts)
{
  if (list_length(pstmts) == 1)
  {
    PlannedStmt *pstmt = linitial_node(PlannedStmt, pstmts);

    if (pstmt->commandType == CMD_UTILITY && IsA(pstmt->utilityStmt, TransactionStmt))
    {
      return true;
    }
  }
  return false;
}

                                                     
static void
drop_unnamed_stmt(void)
{
                                                             
  if (unnamed_stmt_psrc)
  {
    CachedPlanSource *psrc = unnamed_stmt_psrc;

    unnamed_stmt_psrc = NULL;
    DropCachedPlan(psrc);
  }
}

                                    
                                                   
                                    
   

   
                                                               
   
                                     
                                                 
   
void
quickdie(SIGNAL_ARGS)
{
  sigaddset(&BlockSig, SIGQUIT);                           
  PG_SETMASK(&BlockSig);

     
                                                                           
                                                                             
                                                   
     
  HOLD_INTERRUPTS();

     
                                                                     
                                                                         
                                                                      
                             
     
  if (ClientAuthInProgress && whereToSendOutput == DestRemote)
  {
    whereToSendOutput = DestNone;
  }

     
                                                                        
     
                                                                            
                                                                            
                                                                           
                                                                          
                                                                         
                                                                           
                                                                       
     
                                                                          
             
     
  ereport(WARNING, (errcode(ERRCODE_CRASH_SHUTDOWN), errmsg("terminating connection because of crash of another server process"),
                       errdetail("The postmaster has commanded this server process to roll back"
                                 " the current transaction and exit, because another"
                                 " server process exited abnormally and possibly corrupted"
                                 " shared memory."),
                       errhint("In a moment you should be able to reconnect to the"
                               " database and repeat your command.")));

     
                                                                           
                                                                        
                                                                          
                                                                         
             
     
                                                                             
                                                                        
                                                                         
                                                                          
                                                                            
                         
     
  _exit(2);
}

   
                                                               
                              
   
void
die(SIGNAL_ARGS)
{
  int save_errno = errno;

                                           
  if (!proc_exit_inprogress)
  {
    InterruptPending = true;
    ProcDiePending = true;
  }

                                                                        
  SetLatch(MyLatch);

     
                                                                          
                                                                        
                                                                         
                                                      
     
  if (DoingCommandRead && whereToSendOutput != DestRemote)
  {
    ProcessInterrupts();
  }

  errno = save_errno;
}

   
                                                                  
                              
   
void
StatementCancelHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

     
                                         
     
  if (!proc_exit_inprogress)
  {
    InterruptPending = true;
    QueryCancelPending = true;
  }

                                                                        
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                 
void
FloatExceptionHandler(SIGNAL_ARGS)
{
                                                     
  ereport(ERROR, (errcode(ERRCODE_FLOATING_POINT_EXCEPTION), errmsg("floating-point exception"),
                     errdetail("An invalid floating-point operation was signaled. "
                               "This probably means an out-of-range result or an "
                               "invalid operation, such as division by zero.")));
}

   
                                                                    
   
                                                                            
                                                                          
                 
   
void
PostgresSigHupHandler(SIGNAL_ARGS)
{
  int save_errno = errno;

  ConfigReloadPending = true;
  SetLatch(MyLatch);

  errno = save_errno;
}

   
                                                                       
                                                                          
                                                                      
                                              
   
void
RecoveryConflictInterrupt(ProcSignalReason reason)
{
  int save_errno = errno;

     
                                         
     
  if (!proc_exit_inprogress)
  {
    RecoveryConflictReason = reason;
    switch (reason)
    {
    case PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK:

         
                                                                
         
      if (!IsWaitingForLock())
      {
        return;
      }

                                                          
                       

    case PROCSIG_RECOVERY_CONFLICT_BUFFERPIN:

         
                                                                    
                                                                   
                
         
                                                            
                                                               
                                                                   
                                                                    
                        
         
      if (!HoldingBufferPinThatDelaysRecovery())
      {
        if (reason == PROCSIG_RECOVERY_CONFLICT_STARTUP_DEADLOCK && GetStartupBufferPinWaitBufId() < 0)
        {
          CheckDeadLockAlert();
        }
        return;
      }

      MyProc->recoveryConflictPending = true;

                                                      
                       

    case PROCSIG_RECOVERY_CONFLICT_LOCK:
    case PROCSIG_RECOVERY_CONFLICT_TABLESPACE:
    case PROCSIG_RECOVERY_CONFLICT_SNAPSHOT:

         
                                                               
         
      if (!IsTransactionOrTransactionBlock())
      {
        return;
      }

         
                                                                     
                                                                 
                                         
         
                                                                  
                                                                
                             
         
                                                                     
                                                           
                                   
         
                                                                  
                                             
         
      if (!IsSubTransaction())
      {
           
                                                                   
                                                                  
                                                               
           
        if (IsAbortedTransactionBlockState())
        {
          return;
        }

        RecoveryConflictPending = true;
        QueryCancelPending = true;
        InterruptPending = true;
        break;
      }

                                                      
                       

    case PROCSIG_RECOVERY_CONFLICT_DATABASE:
      RecoveryConflictPending = true;
      ProcDiePending = true;
      InterruptPending = true;
      break;

    default:
      elog(FATAL, "unrecognized conflict mode: %d", (int)reason);
    }

    Assert(RecoveryConflictPending && (QueryCancelPending || ProcDiePending));

       
                                                                        
                                                                        
                                                                         
                                            
       
    if (reason == PROCSIG_RECOVERY_CONFLICT_DATABASE)
    {
      RecoveryConflictRetryable = false;
    }
  }

     
                                                                      
                                                                           
                                     
     
  SetLatch(MyLatch);

  errno = save_errno;
}

   
                                                                          
   
                                                                      
                                                                   
                             
   
                                                                          
                                                                      
                                                                       
                                                                      
                                                  
   
void
ProcessInterrupts(void)
{
                                        
  if (InterruptHoldoffCount != 0 || CritSectionCount != 0)
  {
    return;
  }
  InterruptPending = false;

  if (ProcDiePending)
  {
    ProcDiePending = false;
    QueryCancelPending = false;                                 
    LockErrorCleanup();
                                                                  
    if (ClientAuthInProgress && whereToSendOutput == DestRemote)
    {
      whereToSendOutput = DestNone;
    }
    if (ClientAuthInProgress)
    {
      ereport(FATAL, (errcode(ERRCODE_QUERY_CANCELED), errmsg("canceling authentication due to timeout")));
    }
    else if (IsAutoVacuumWorkerProcess())
    {
      ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("terminating autovacuum process due to administrator command")));
    }
    else if (IsLogicalWorker())
    {
      ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("terminating logical replication worker due to administrator command")));
    }
    else if (IsLogicalLauncher())
    {
      ereport(DEBUG1, (errmsg("logical replication launcher shutting down")));

         
                                                                      
                                                                  
         
      proc_exit(1);
    }
    else if (RecoveryConflictPending && RecoveryConflictRetryable)
    {
      pgstat_report_recovery_conflict(RecoveryConflictReason);
      ereport(FATAL, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("terminating connection due to conflict with recovery"), errdetail_recovery_conflict()));
    }
    else if (RecoveryConflictPending)
    {
                                                                       
      Assert(RecoveryConflictReason == PROCSIG_RECOVERY_CONFLICT_DATABASE);
      pgstat_report_recovery_conflict(RecoveryConflictReason);
      ereport(FATAL, (errcode(ERRCODE_DATABASE_DROPPED), errmsg("terminating connection due to conflict with recovery"), errdetail_recovery_conflict()));
    }
    else
    {
      ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("terminating connection due to administrator command")));
    }
  }
  if (ClientConnectionLost)
  {
    QueryCancelPending = false;                                         
    LockErrorCleanup();
                                                                          
    whereToSendOutput = DestNone;
    ereport(FATAL, (errcode(ERRCODE_CONNECTION_FAILURE), errmsg("connection to client lost")));
  }

     
                                                                            
                                                                          
                                                                            
                  
     
  if (RecoveryConflictPending && DoingCommandRead)
  {
    QueryCancelPending = false;                              
    RecoveryConflictPending = false;
    LockErrorCleanup();
    pgstat_report_recovery_conflict(RecoveryConflictReason);
    ereport(FATAL, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("terminating connection due to conflict with recovery"), errdetail_recovery_conflict(),
                       errhint("In a moment you should be able to reconnect to the"
                               " database and repeat your command.")));
  }

     
                                                                      
                                                                     
                                                                            
                           
     
  if (QueryCancelPending && QueryCancelHoldoffCount != 0)
  {
       
                                                                        
                                                                       
                                                                           
                                                                           
                                                                           
                
       
    InterruptPending = true;
  }
  else if (QueryCancelPending)
  {
    bool lock_timeout_occurred;
    bool stmt_timeout_occurred;

    QueryCancelPending = false;

       
                                                                         
                                                 
       
    lock_timeout_occurred = get_timeout_indicator(LOCK_TIMEOUT, true);
    stmt_timeout_occurred = get_timeout_indicator(STATEMENT_TIMEOUT, true);

       
                                                                       
                                                                        
                                                                          
                                                                   
       
    if (lock_timeout_occurred && stmt_timeout_occurred && get_timeout_finish_time(STATEMENT_TIMEOUT) < get_timeout_finish_time(LOCK_TIMEOUT))
    {
      lock_timeout_occurred = false;                          
    }

    if (lock_timeout_occurred)
    {
      LockErrorCleanup();
      ereport(ERROR, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("canceling statement due to lock timeout")));
    }
    if (stmt_timeout_occurred)
    {
      LockErrorCleanup();
      ereport(ERROR, (errcode(ERRCODE_QUERY_CANCELED), errmsg("canceling statement due to statement timeout")));
    }
    if (IsAutoVacuumWorkerProcess())
    {
      LockErrorCleanup();
      ereport(ERROR, (errcode(ERRCODE_QUERY_CANCELED), errmsg("canceling autovacuum task")));
    }
    if (RecoveryConflictPending)
    {
      RecoveryConflictPending = false;
      LockErrorCleanup();
      pgstat_report_recovery_conflict(RecoveryConflictReason);
      ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("canceling statement due to conflict with recovery"), errdetail_recovery_conflict()));
    }

       
                                                                           
                                                                   
                                                           
       
    if (!DoingCommandRead)
    {
      LockErrorCleanup();
      ereport(ERROR, (errcode(ERRCODE_QUERY_CANCELED), errmsg("canceling statement due to user request")));
    }
  }

  if (IdleInTransactionSessionTimeoutPending)
  {
                                                               
    if (IdleInTransactionSessionTimeout > 0)
    {
      ereport(FATAL, (errcode(ERRCODE_IDLE_IN_TRANSACTION_SESSION_TIMEOUT), errmsg("terminating connection due to idle-in-transaction timeout")));
    }
    else
    {
      IdleInTransactionSessionTimeoutPending = false;
    }
  }

  if (ParallelMessagePending)
  {
    HandleParallelMessages();
  }
}

   
                                                                           
   
                                                                    
   
                                                                           
                                                                           
                                   
   
#if defined(__ia64__) || defined(__ia64)

#if defined(__hpux) && !defined(__GNUC__) && !defined(__INTEL_COMPILER)
                                       
#include <ia64/sys/inline.h>
#define ia64_get_bsp() ((char *)(_Asm_mov_from_ar(_AREG_BSP, _NO_FENCE)))
#elif defined(__INTEL_COMPILER)
         
#include <asm/ia64regs.h>
#define ia64_get_bsp() ((char *)__getReg(_IA64_REG_AR_BSP))
#else
         
static __inline__ char *
ia64_get_bsp(void)
{
  char *ret;

                                                                    
  __asm__ __volatile__(";;\n"
                       "	mov	%0=ar.bsp	\n"
                       : "=r"(ret));

  return ret;
}
#endif
#endif           

   
                                                                   
   
                                            
   
pg_stack_base_t
set_stack_base(void)
{
#ifndef HAVE__BUILTIN_FRAME_ADDRESS
  char stack_base;
#endif
  pg_stack_base_t old;

#if defined(__ia64__) || defined(__ia64)
  old.stack_base_ptr = stack_base_ptr;
  old.register_stack_base_ptr = register_stack_base_ptr;
#else
  old = stack_base_ptr;
#endif

     
                                                                            
                                                                        
                                                  
     
#ifdef HAVE__BUILTIN_FRAME_ADDRESS
  stack_base_ptr = __builtin_frame_address(0);
#else
  stack_base_ptr = &stack_base;
#endif
#if defined(__ia64__) || defined(__ia64)
  register_stack_base_ptr = ia64_get_bsp();
#endif

  return old;
}

   
                                                                        
   
                                                                          
                                                                            
                                                                             
                                                                             
                           
   
void
restore_stack_base(pg_stack_base_t base)
{
#if defined(__ia64__) || defined(__ia64)
  stack_base_ptr = base.stack_base_ptr;
  register_stack_base_ptr = base.register_stack_base_ptr;
#else
  stack_base_ptr = base;
#endif
}

   
                                                                             
   
                                                                                
                                                                       
                                                                           
                                      
   
                                                                            
                                                                        
   
void
check_stack_depth(void)
{
  if (stack_is_too_deep())
  {
    ereport(ERROR, (errcode(ERRCODE_STATEMENT_TOO_COMPLEX), errmsg("stack depth limit exceeded"),
                       errhint("Increase the configuration parameter \"max_stack_depth\" (currently %dkB), "
                               "after ensuring the platform's stack depth limit is adequate.",
                           max_stack_depth)));
  }
}

bool
stack_is_too_deep(void)
{
  char stack_top_loc;
  long stack_depth;

     
                                                                 
     
  stack_depth = (long)(stack_base_ptr - &stack_top_loc);

     
                                                                           
     
  if (stack_depth < 0)
  {
    stack_depth = -stack_depth;
  }

     
              
     
                                                                        
                                                                            
                                                                            
            
     
  if (stack_depth > max_stack_depth_bytes && stack_base_ptr != NULL)
  {
    return true;
  }

     
                                                                        
                                                                        
                                                                       
                                                         
     
                                                                          
     
#if defined(__ia64__) || defined(__ia64)
  stack_depth = (long)(ia64_get_bsp() - register_stack_base_ptr);

  if (stack_depth > max_stack_depth_bytes && register_stack_base_ptr != NULL)
  {
    return true;
  }
#endif           

  return false;
}

                                        
bool
check_max_stack_depth(int *newval, void **extra, GucSource source)
{
  long newval_bytes = *newval * 1024L;
  long stack_rlimit = get_stack_depth_rlimit();

  if (stack_rlimit > 0 && newval_bytes > stack_rlimit - STACK_DEPTH_SLOP)
  {
    GUC_check_errdetail("\"max_stack_depth\" must not exceed %ldkB.", (stack_rlimit - STACK_DEPTH_SLOP) / 1024L);
    GUC_check_errhint("Increase the platform's stack depth limit via \"ulimit -s\" or local equivalent.");
    return false;
  }
  return true;
}

                                         
void
assign_max_stack_depth(int newval, void *extra)
{
  long newval_bytes = newval * 1024L;

  max_stack_depth_bytes = newval_bytes;
}

   
                                                          
   
                                                                           
                         
   
void
set_debug_options(int debug_flag, GucContext context, GucSource source)
{
  if (debug_flag > 0)
  {
    char debugstr[64];

    sprintf(debugstr, "debug%d", debug_flag);
    SetConfigOption("log_min_messages", debugstr, context, source);
  }
  else
  {
    SetConfigOption("log_min_messages", "notice", context, source);
  }

  if (debug_flag >= 1 && context == PGC_POSTMASTER)
  {
    SetConfigOption("log_connections", "true", context, source);
    SetConfigOption("log_disconnections", "true", context, source);
  }
  if (debug_flag >= 2)
  {
    SetConfigOption("log_statement", "all", context, source);
  }
  if (debug_flag >= 3)
  {
    SetConfigOption("debug_print_parse", "true", context, source);
  }
  if (debug_flag >= 4)
  {
    SetConfigOption("debug_print_plan", "true", context, source);
  }
  if (debug_flag >= 5)
  {
    SetConfigOption("debug_print_rewritten", "true", context, source);
  }
}

bool
set_plan_disabling_options(const char *arg, GucContext context, GucSource source)
{
  const char *tmp = NULL;

  switch (arg[0])
  {
  case 's':              
    tmp = "enable_seqscan";
    break;
  case 'i':                
    tmp = "enable_indexscan";
    break;
  case 'o':                    
    tmp = "enable_indexonlyscan";
    break;
  case 'b':                 
    tmp = "enable_bitmapscan";
    break;
  case 't':              
    tmp = "enable_tidscan";
    break;
  case 'n':               
    tmp = "enable_nestloop";
    break;
  case 'm':                
    tmp = "enable_mergejoin";
    break;
  case 'h':               
    tmp = "enable_hashjoin";
    break;
  }
  if (tmp)
  {
    SetConfigOption(tmp, "false", context, source);
    return true;
  }
  else
  {
    return false;
  }
}

const char *
get_stats_option_name(const char *arg)
{
  switch (arg[0])
  {
  case 'p':
    if (optarg[1] == 'a')               
    {
      return "log_parser_stats";
    }
    else if (optarg[1] == 'l')                
    {
      return "log_planner_stats";
    }
    break;

  case 'e':                 
    return "log_executor_stats";
    break;
  }

  return NULL;
}

                                                                    
                             
                                                    
   
                                                                       
                                                                          
                                                                          
                                          
   
                                                                            
   
                                                                              
                                                                              
                       
   
                                                                     
                                                                              
                                                                    
   
void
process_postgres_switches(int argc, char *argv[], GucContext ctx, const char **dbname)
{
  bool secure = (ctx == PGC_POSTMASTER);
  int errs = 0;
  GucSource gucsource;
  int flag;

  if (secure)
  {
    gucsource = PGC_S_ARGV;                                      

                                                          
    if (argc > 1 && strcmp(argv[1], "--single") == 0)
    {
      argv++;
      argc--;
    }
  }
  else
  {
    gucsource = PGC_S_CLIENT;                                
  }

#ifdef HAVE_INT_OPTERR

     
                                                                         
                                                                            
                                                                   
     
  opterr = 0;
#endif

     
                                                                  
                                                                            
                                                
     
  while ((flag = getopt(argc, argv, "B:bc:C:D:d:EeFf:h:ijk:lN:nOo:Pp:r:S:sTt:v:W:-:")) != -1)
  {
    switch (flag)
    {
    case 'B':
      SetConfigOption("shared_buffers", optarg, ctx, gucsource);
      break;

    case 'b':
                                                      
      if (secure)
      {
        IsBinaryUpgrade = true;
      }
      break;

    case 'C':
                                                       
      break;

    case 'D':
      if (secure)
      {
        userDoption = strdup(optarg);
      }
      break;

    case 'd':
      set_debug_options(atoi(optarg), ctx, gucsource);
      break;

    case 'E':
      if (secure)
      {
        EchoQuery = true;
      }
      break;

    case 'e':
      SetConfigOption("datestyle", "euro", ctx, gucsource);
      break;

    case 'F':
      SetConfigOption("fsync", "false", ctx, gucsource);
      break;

    case 'f':
      if (!set_plan_disabling_options(optarg, ctx, gucsource))
      {
        errs++;
      }
      break;

    case 'h':
      SetConfigOption("listen_addresses", optarg, ctx, gucsource);
      break;

    case 'i':
      SetConfigOption("listen_addresses", "*", ctx, gucsource);
      break;

    case 'j':
      if (secure)
      {
        UseSemiNewlineNewline = true;
      }
      break;

    case 'k':
      SetConfigOption("unix_socket_directories", optarg, ctx, gucsource);
      break;

    case 'l':
      SetConfigOption("ssl", "true", ctx, gucsource);
      break;

    case 'N':
      SetConfigOption("max_connections", optarg, ctx, gucsource);
      break;

    case 'n':
                                                   
      break;

    case 'O':
      SetConfigOption("allow_system_table_mods", "true", ctx, gucsource);
      break;

    case 'o':
      errs++;
      break;

    case 'P':
      SetConfigOption("ignore_system_indexes", "true", ctx, gucsource);
      break;

    case 'p':
      SetConfigOption("port", optarg, ctx, gucsource);
      break;

    case 'r':
                                                             
      if (secure)
      {
        strlcpy(OutputFileName, optarg, MAXPGPATH);
      }
      break;

    case 'S':
      SetConfigOption("work_mem", optarg, ctx, gucsource);
      break;

    case 's':
      SetConfigOption("log_statement_stats", "true", ctx, gucsource);
      break;

    case 'T':
                                                       
      break;

    case 't':
    {
      const char *tmp = get_stats_option_name(optarg);

      if (tmp)
      {
        SetConfigOption(tmp, "true", ctx, gucsource);
      }
      else
      {
        errs++;
      }
      break;
    }

    case 'v':

         
                                                         
                                                                     
                                                                   
                                                                    
                             
         
      if (secure)
      {
        FrontendProtocol = (ProtocolVersion)atoi(optarg);
      }
      break;

    case 'W':
      SetConfigOption("post_auth_delay", optarg, ctx, gucsource);
      break;

    case 'c':
    case '-':
    {
      char *name, *value;

      ParseLongOption(optarg, &name, &value);
      if (!value)
      {
        if (flag == '-')
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("--%s requires a value", optarg)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("-c %s requires a value", optarg)));
        }
      }
      SetConfigOption(name, value, ctx, gucsource);
      free(name);
      if (value)
      {
        free(value);
      }
      break;
    }

    default:
      errs++;
      break;
    }

    if (errs)
    {
      break;
    }
  }

     
                                                                     
     
  if (!errs && dbname && *dbname == NULL && argc - optind >= 1)
  {
    *dbname = strdup(argv[optind++]);
  }

  if (errs || argc != optind)
  {
    if (errs)
    {
      optind--;                                           
    }

                                                                        
    if (IsUnderPostmaster)
    {
      ereport(FATAL, errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid command-line argument for server process: %s", argv[optind]), errhint("Try \"%s --help\" for more information.", progname));
    }
    else
    {
      ereport(FATAL, errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s: invalid command-line argument: %s", progname, argv[optind]), errhint("Try \"%s --help\" for more information.", progname));
    }
  }

     
                                                                            
                                                                       
     
  optind = 1;
#ifdef HAVE_INT_OPTRESET
  optreset = 1;                                 
#endif
}

                                                                    
                
                                                                              
   
                                                                            
                                                                             
                                                                             
                                                                          
                                                                    
                                                                    
   
void
PostgresMain(int argc, char *argv[], const char *dbname, const char *username)
{
  int firstchar;
  StringInfoData input_message;
  sigjmp_buf local_sigjmp_buf;
  volatile bool send_ready_for_query = true;
  bool disable_idle_in_transaction_timeout = false;

                                                            
  if (!IsUnderPostmaster)
  {
    InitStandaloneProcess(argv[0]);
  }

  SetProcessingMode(InitProcessing);

     
                                                  
     
  if (!IsUnderPostmaster)
  {
    InitializeGUCOptions();
  }

     
                                 
     
  process_postgres_switches(argc, argv, PGC_POSTMASTER, &dbname);

                                                                          
  if (dbname == NULL)
  {
    dbname = username;
    if (dbname == NULL)
    {
      ereport(FATAL, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("%s: no database nor user name specified", progname)));
    }
  }

                                                                          
  if (!IsUnderPostmaster)
  {
    if (!SelectConfigFiles(userDoption, progname))
    {
      proc_exit(1);
    }
  }

     
                                       
     
                                                                            
                                                                            
                                 
     
                                                                             
                                                                            
                                                                       
                                                                             
                                                                          
               
     
  if (am_walsender)
  {
    WalSndSignals();
  }
  else
  {
    pqsignal(SIGHUP, PostgresSigHupHandler);                             
                                                        
    pqsignal(SIGINT, StatementCancelHandler);                           
    pqsignal(SIGTERM, die);                                                      

       
                                                                           
                                                                      
                               
       
    if (IsUnderPostmaster)
    {
      pqsignal(SIGQUIT, quickdie);                      
    }
    else
    {
      pqsignal(SIGQUIT, die);                                    
    }
    InitializeTimeouts();                                  

       
                                                                     
                                                                        
                                                                         
                                                          
       
    pqsignal(SIGPIPE, SIG_IGN);
    pqsignal(SIGUSR1, procsignal_sigusr1_handler);
    pqsignal(SIGUSR2, SIG_IGN);
    pqsignal(SIGFPE, FloatExceptionHandler);

       
                                                                     
               
       
    pqsignal(SIGCHLD, SIG_DFL);                                   
                                               
  }

  pqinitmask();

  if (IsUnderPostmaster)
  {
                                                  
    sigdelset(&BlockSig, SIGQUIT);
  }

  PG_SETMASK(&BlockSig);                                      

  if (!IsUnderPostmaster)
  {
       
                                                                          
                                                        
       
    checkDataDir();

                                                                     
    ChangeToDataDir();

       
                                           
       
    CreateDataDirLockFile(false);

                                                                 
    LocalProcessControlFile(false);

                                                                        
    InitializeMaxBackends();
  }

                            
  BaseInit();

     
                                                                        
                                                                            
                                                                             
                                         
     
#ifdef EXEC_BACKEND
  if (!IsUnderPostmaster)
  {
    InitProcess();
  }
#else
  InitProcess();
#endif

                                                                   
  PG_SETMASK(&UnBlockSig);

     
                             
     
                                                                             
                                                                     
                                                         
     
  InitPostgres(dbname, InvalidOid, username, InvalidOid, NULL, false);

     
                                                                           
                                                                             
                                                                          
                                                                            
                                                                         
     
  if (PostmasterContext)
  {
    MemoryContextDelete(PostmasterContext);
    PostmasterContext = NULL;
  }

  SetProcessingMode(NormalProcessing);

     
                                                                    
                  
     
  BeginReportingGUCOptions();

     
                                                                            
                                                  
     
  if (IsUnderPostmaster && Log_disconnections)
  {
    on_proc_exit(log_disconnections, 0);
  }

                                                                
  if (am_walsender)
  {
    InitWalSender();
  }

     
                                                                           
                                                             
     
  process_session_preload_libraries();

     
                                                            
     
  if (whereToSendOutput == DestRemote)
  {
    StringInfoData buf;

    pq_beginmessage(&buf, 'K');
    pq_sendint32(&buf, (int32)MyProcPid);
    pq_sendint32(&buf, (int32)MyCancelKey);
    pq_endmessage(&buf);
                                                        
  }

                                          
  if (whereToSendOutput == DestDebug)
  {
    printf("\nPostgreSQL stand-alone backend %s\n", PG_VERSION);
  }

     
                                                             
     
                                                                           
                                                                       
     
  MessageContext = AllocSetContextCreate(TopMemoryContext, "MessageContext", ALLOCSET_DEFAULT_SIZES);

     
                                                                           
                                                                            
                                                                     
                                            
     
  row_description_context = AllocSetContextCreate(TopMemoryContext, "RowDescriptionContext", ALLOCSET_DEFAULT_SIZES);
  MemoryContextSwitchTo(row_description_context);
  initStringInfo(&row_description_buf);
  MemoryContextSwitchTo(TopMemoryContext);

     
                                               
     
  if (!IsUnderPostmaster)
  {
    PgStartTime = GetCurrentTimestamp();
  }

     
                                               
     
                                                                             
                                              
     
                                                                        
                                                                     
                                                                             
                                                                             
                                                                             
                                                                          
                                                                         
     
                                                                             
                                                                           
                                                                             
                                                                        
                                                                           
                                
     

  if (sigsetjmp(local_sigjmp_buf, 1) != 0)
  {
       
                                                                   
                                                          
                                                                      
                                                                          
                                                                    
       

                                                                
    error_context_stack = NULL;

                                              
    HOLD_INTERRUPTS();

       
                                                                        
                                                                          
                                                                           
                                                                         
                                                                      
                                                                     
                                                                          
                                                                      
                                    
       
    disable_all_timeouts(false);
    QueryCancelPending = false;                                     
    stmt_timeout_active = false;

                                              
    DoingCommandRead = false;

                                            
    pq_comm_reset();

                                                          
    EmitErrorReport();

       
                                                                          
                                 
       
    debug_query_string = NULL;

       
                                                          
       
    AbortCurrentTransaction();

    if (am_walsender)
    {
      WalSndErrorCleanup();
    }

    PortalErrorCleanup();

       
                                                                          
                                                                           
                                                                         
                                                                        
                                                                 
       
    if (MyReplicationSlot != NULL)
    {
      ReplicationSlotRelease();
    }

                                                           
    ReplicationSlotCleanup();

    jit_reset_after_error();

       
                                                                         
                  
       
    MemoryContextSwitchTo(TopMemoryContext);
    FlushErrorState();

       
                                                                        
                                                              
                                          
       
    if (doing_extended_query_message)
    {
      ignore_till_sync = true;
    }

                                                          
    xact_started = false;

       
                                                                     
                                                                    
                                                                  
                                                                          
                                                                        
                           
       
    if (pq_is_reading_msg())
    {
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("terminating connection because protocol synchronization was lost")));
    }

                                           
    RESUME_INTERRUPTS();
  }

                                        
  PG_exception_stack = &local_sigjmp_buf;

  if (!ignore_till_sync)
  {
    send_ready_for_query = true;                                
  }

     
                                  
     

  for (;;)
  {
       
                                                                      
                                                              
       
    doing_extended_query_message = false;

       
                                                                          
                                                         
       
    MemoryContextSwitchTo(MessageContext);
    MemoryContextResetAndDeleteChildren(MessageContext);

    initStringInfo(&input_message);

       
                                                                         
                                                                           
       
    InvalidateCatalogSnapshotConditionally();

       
                                                                          
                    
       
                                                                      
       
                                                                    
                                                                      
                                                                        
                                                                           
                                                                         
                                                                         
       
    if (send_ready_for_query)
    {
      if (IsAbortedTransactionBlockState())
      {
        set_ps_display("idle in transaction (aborted)", false);
        pgstat_report_activity(STATE_IDLEINTRANSACTION_ABORTED, NULL);

                                                 
        if (IdleInTransactionSessionTimeout > 0)
        {
          disable_idle_in_transaction_timeout = true;
          enable_timeout_after(IDLE_IN_TRANSACTION_SESSION_TIMEOUT, IdleInTransactionSessionTimeout);
        }
      }
      else if (IsTransactionOrTransactionBlock())
      {
        set_ps_display("idle in transaction", false);
        pgstat_report_activity(STATE_IDLEINTRANSACTION, NULL);

                                                 
        if (IdleInTransactionSessionTimeout > 0)
        {
          disable_idle_in_transaction_timeout = true;
          enable_timeout_after(IDLE_IN_TRANSACTION_SESSION_TIMEOUT, IdleInTransactionSessionTimeout);
        }
      }
      else
      {
                                                                
        ProcessCompletedNotifies();

           
                                                                      
                                                                 
                                                                     
                                                       
           
        if (notifyInterruptPending)
        {
          ProcessNotifyInterrupt();
        }

        pgstat_report_stat(false);

        set_ps_display("idle", false);
        pgstat_report_activity(STATE_IDLE, NULL);
      }

      ReadyForQuery(whereToSendOutput);
      send_ready_for_query = false;
    }

       
                                                                         
                                                                    
                                                                          
                                    
       
    DoingCommandRead = true;

       
                                             
       
    firstchar = ReadCommand(&input_message);

       
                                                                       
                                                                          
                                
       
    if (disable_idle_in_transaction_timeout)
    {
      disable_timeout(IDLE_IN_TRANSACTION_SESSION_TIMEOUT, false);
      disable_idle_in_transaction_timeout = false;
    }

       
                                                  
       
                                                                        
                                                                       
                                                                          
                                                                         
                                          
       
    CHECK_FOR_INTERRUPTS();
    DoingCommandRead = false;

       
                                                                         
              
       
    if (ConfigReloadPending)
    {
      ConfigReloadPending = false;
      ProcessConfigFile(PGC_SIGHUP);
    }

       
                                                                      
             
       
    if (ignore_till_sync && firstchar != EOF)
    {
      continue;
    }

    switch (firstchar)
    {
    case 'Q':                   
    {
      const char *query_string;

                                     
      SetCurrentStatementStartTimestamp();

      query_string = pq_getmsgstring(&input_message);
      pq_getmsgend(&input_message);

      if (am_walsender)
      {
        if (!exec_replication_command(query_string))
        {
          exec_simple_query(query_string);
        }
      }
      else
      {
        exec_simple_query(query_string);
      }

      send_ready_for_query = true;
    }
    break;

    case 'P':            
    {
      const char *stmt_name;
      const char *query_string;
      int numParams;
      Oid *paramTypes = NULL;

      forbidden_in_wal_sender(firstchar);

                                     
      SetCurrentStatementStartTimestamp();

      stmt_name = pq_getmsgstring(&input_message);
      query_string = pq_getmsgstring(&input_message);
      numParams = pq_getmsgint(&input_message, 2);
      if (numParams > 0)
      {
        paramTypes = (Oid *)palloc(numParams * sizeof(Oid));
        for (int i = 0; i < numParams; i++)
        {
          paramTypes[i] = pq_getmsgint(&input_message, 4);
        }
      }
      pq_getmsgend(&input_message);

      exec_parse_message(query_string, stmt_name, paramTypes, numParams);
    }
    break;

    case 'B':           
      forbidden_in_wal_sender(firstchar);

                                     
      SetCurrentStatementStartTimestamp();

         
                                                                  
                                          
         
      exec_bind_message(&input_message);
      break;

    case 'E':              
    {
      const char *portal_name;
      int max_rows;

      forbidden_in_wal_sender(firstchar);

                                     
      SetCurrentStatementStartTimestamp();

      portal_name = pq_getmsgstring(&input_message);
      max_rows = pq_getmsgint(&input_message, 4);
      pq_getmsgend(&input_message);

      exec_execute_message(portal_name, max_rows);
    }
    break;

    case 'F':                             
      forbidden_in_wal_sender(firstchar);

                                     
      SetCurrentStatementStartTimestamp();

                                                          
      pgstat_report_activity(STATE_FASTPATH, NULL);
      set_ps_display("<FASTPATH>", false);

                                                      
      start_xact_command();

         
                                                         
                                                                 
                                                        
                                                                   
                                                                   
                                 
         

                                          
      MemoryContextSwitchTo(MessageContext);

      HandleFunctionRequest(&input_message);

                                                      
      finish_xact_command();

      send_ready_for_query = true;
      break;

    case 'C':            
    {
      int close_type;
      const char *close_target;

      forbidden_in_wal_sender(firstchar);

      close_type = pq_getmsgbyte(&input_message);
      close_target = pq_getmsgstring(&input_message);
      pq_getmsgend(&input_message);

      switch (close_type)
      {
      case 'S':
        if (close_target[0] != '\0')
        {
          DropPreparedStatement(close_target, false);
        }
        else
        {
                                                  
          drop_unnamed_stmt();
        }
        break;
      case 'P':
      {
        Portal portal;

        portal = GetPortalByName(close_target);
        if (PortalIsValid(portal))
        {
          PortalDrop(portal, false);
        }
      }
      break;
      default:
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid CLOSE message subtype %d", close_type)));
        break;
      }

      if (whereToSendOutput == DestRemote)
      {
        pq_putemptymessage('3');                    
      }
    }
    break;

    case 'D':               
    {
      int describe_type;
      const char *describe_target;

      forbidden_in_wal_sender(firstchar);

                                                       
      SetCurrentStatementStartTimestamp();

      describe_type = pq_getmsgbyte(&input_message);
      describe_target = pq_getmsgstring(&input_message);
      pq_getmsgend(&input_message);

      switch (describe_type)
      {
      case 'S':
        exec_describe_statement_message(describe_target);
        break;
      case 'P':
        exec_describe_portal_message(describe_target);
        break;
      default:
        ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid DESCRIBE message subtype %d", describe_type)));
        break;
      }
    }
    break;

    case 'H':            
      pq_getmsgend(&input_message);
      if (whereToSendOutput == DestRemote)
      {
        pq_flush();
      }
      break;

    case 'S':           
      pq_getmsgend(&input_message);
      finish_xact_command();
      send_ready_for_query = true;
      break;

         
                                                                     
                                                                   
                                  
         
    case 'X':
    case EOF:

         
                                                                    
                                              
         
      if (whereToSendOutput == DestRemote)
      {
        whereToSendOutput = DestNone;
      }

         
                                                                
                                                               
                                                                    
                                                                 
                    
         
      proc_exit(0);

    case 'd':                
    case 'c':                
    case 'f':                

         
                                                                 
                                                                   
                                
         
      break;

    default:
      ereport(FATAL, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid frontend message type %d", firstchar)));
    }
  }                                
}

   
                                                 
   
                                                                            
                                                                            
                                                                     
   
static void
forbidden_in_wal_sender(char firstchar)
{
  if (am_walsender)
  {
    if (firstchar == 'F')
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("fastpath function calls not supported in a replication connection")));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("extended query protocol not supported in a replication connection")));
    }
  }
}

   
                                                
   
                        
   
long
get_stack_depth_rlimit(void)
{
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_STACK)
  static long val = 0;

                                                                  
  if (val == 0)
  {
    struct rlimit rlim;

    if (getrlimit(RLIMIT_STACK, &rlim) < 0)
    {
      val = -1;
    }
    else if (rlim.rlim_cur == RLIM_INFINITY)
    {
      val = LONG_MAX;
    }
                                                                         
    else if (rlim.rlim_cur >= LONG_MAX)
    {
      val = LONG_MAX;
    }
    else
    {
      val = rlim.rlim_cur;
    }
  }
  return val;
#else                   
#if defined(WIN32) || defined(__CYGWIN__)
                                                                        
  return WIN32_STACK_RLIMIT;
#else                              
  return -1;
#endif
#endif
}

static struct rusage Save_r;
static struct timeval Save_t;

void
ResetUsage(void)
{
  getrusage(RUSAGE_SELF, &Save_r);
  gettimeofday(&Save_t, NULL);
}

void
ShowUsage(const char *title)
{
  StringInfoData str;
  struct timeval user, sys;
  struct timeval elapse_t;
  struct rusage r;

  getrusage(RUSAGE_SELF, &r);
  gettimeofday(&elapse_t, NULL);
  memcpy((char *)&user, (char *)&r.ru_utime, sizeof(user));
  memcpy((char *)&sys, (char *)&r.ru_stime, sizeof(sys));
  if (elapse_t.tv_usec < Save_t.tv_usec)
  {
    elapse_t.tv_sec--;
    elapse_t.tv_usec += 1000000;
  }
  if (r.ru_utime.tv_usec < Save_r.ru_utime.tv_usec)
  {
    r.ru_utime.tv_sec--;
    r.ru_utime.tv_usec += 1000000;
  }
  if (r.ru_stime.tv_usec < Save_r.ru_stime.tv_usec)
  {
    r.ru_stime.tv_sec--;
    r.ru_stime.tv_usec += 1000000;
  }

     
                                                                          
                                                                         
     
  initStringInfo(&str);

  appendStringInfoString(&str, "! system usage stats:\n");
  appendStringInfo(&str, "!\t%ld.%06ld s user, %ld.%06ld s system, %ld.%06ld s elapsed\n", (long)(r.ru_utime.tv_sec - Save_r.ru_utime.tv_sec), (long)(r.ru_utime.tv_usec - Save_r.ru_utime.tv_usec), (long)(r.ru_stime.tv_sec - Save_r.ru_stime.tv_sec), (long)(r.ru_stime.tv_usec - Save_r.ru_stime.tv_usec), (long)(elapse_t.tv_sec - Save_t.tv_sec), (long)(elapse_t.tv_usec - Save_t.tv_usec));
  appendStringInfo(&str, "!\t[%ld.%06ld s user, %ld.%06ld s system total]\n", (long)user.tv_sec, (long)user.tv_usec, (long)sys.tv_sec, (long)sys.tv_usec);
#if defined(HAVE_GETRUSAGE)
  appendStringInfo(&str, "!\t%ld kB max resident size\n",
#if defined(__darwin__)
                             
      r.ru_maxrss / 1024
#else
                                                
      r.ru_maxrss
#endif
  );
  appendStringInfo(&str, "!\t%ld/%ld [%ld/%ld] filesystem blocks in/out\n", r.ru_inblock - Save_r.ru_inblock,
                                         
      r.ru_oublock - Save_r.ru_oublock, r.ru_inblock, r.ru_oublock);
  appendStringInfo(&str, "!\t%ld/%ld [%ld/%ld] page faults/reclaims, %ld [%ld] swaps\n", r.ru_majflt - Save_r.ru_majflt, r.ru_minflt - Save_r.ru_minflt, r.ru_majflt, r.ru_minflt, r.ru_nswap - Save_r.ru_nswap, r.ru_nswap);
  appendStringInfo(&str, "!\t%ld [%ld] signals rcvd, %ld/%ld [%ld/%ld] messages rcvd/sent\n", r.ru_nsignals - Save_r.ru_nsignals, r.ru_nsignals, r.ru_msgrcv - Save_r.ru_msgrcv, r.ru_msgsnd - Save_r.ru_msgsnd, r.ru_msgrcv, r.ru_msgsnd);
  appendStringInfo(&str, "!\t%ld/%ld [%ld/%ld] voluntary/involuntary context switches\n", r.ru_nvcsw - Save_r.ru_nvcsw, r.ru_nivcsw - Save_r.ru_nivcsw, r.ru_nvcsw, r.ru_nivcsw);
#endif                     

                               
  if (str.data[str.len - 1] == '\n')
  {
    str.data[--str.len] = '\0';
  }

  ereport(LOG, (errmsg_internal("%s", title), errdetail_internal("%s", str.data)));

  pfree(str.data);
}

   
                                              
   
static void
log_disconnections(int code, Datum arg)
{
  Port *port = MyProcPort;
  long secs;
  int usecs;
  int msecs;
  int hours, minutes, seconds;

  TimestampDifference(MyStartTimestamp, GetCurrentTimestamp(), &secs, &usecs);
  msecs = usecs / 1000;

  hours = secs / SECS_PER_HOUR;
  secs %= SECS_PER_HOUR;
  minutes = secs / SECS_PER_MINUTE;
  seconds = secs % SECS_PER_MINUTE;

  ereport(LOG, (errmsg("disconnection: session time: %d:%02d:%02d.%03d "
                       "user=%s database=%s host=%s%s%s",
                   hours, minutes, seconds, msecs, port->user_name, port->database_name, port->remote_host, port->remote_port[0] ? " port=" : "", port->remote_port)));
}

   
                                              
   
                                                                        
                                                                           
            
   
static void
enable_statement_timeout(void)
{
                              
  Assert(xact_started);

  if (StatementTimeout > 0)
  {
    if (!stmt_timeout_active)
    {
      enable_timeout_after(STATEMENT_TIMEOUT, StatementTimeout);
      stmt_timeout_active = true;
    }
  }
  else
  {
    disable_timeout(STATEMENT_TIMEOUT, false);
  }
}

   
                                         
   
static void
disable_statement_timeout(void)
{
  if (stmt_timeout_active)
  {
    disable_timeout(STATEMENT_TIMEOUT, false);

    stmt_timeout_active = false;
  }
}
