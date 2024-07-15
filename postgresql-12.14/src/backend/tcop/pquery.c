                                                                            
   
            
                                         
   
                                                                         
                                                                        
   
   
                  
                               
   
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "access/xact.h"
#include "commands/prepare.h"
#include "executor/tstoreReceiver.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

   
                                                                            
                          
   
Portal ActivePortal = NULL;

static void
ProcessQuery(PlannedStmt *plan, const char *sourceText, ParamListInfo params, QueryEnvironment *queryEnv, DestReceiver *dest, char *completionTag);
static void
FillPortalStore(Portal portal, bool isTopLevel);
static uint64
RunFromStore(Portal portal, ScanDirection direction, uint64 count, DestReceiver *dest);
static uint64
PortalRunSelect(Portal portal, bool forward, long count, DestReceiver *dest);
static void
PortalRunUtility(Portal portal, PlannedStmt *pstmt, bool isTopLevel, bool setHoldSnapshot, DestReceiver *dest, char *completionTag);
static void
PortalRunMulti(Portal portal, bool isTopLevel, bool setHoldSnapshot, DestReceiver *dest, DestReceiver *altdest, char *completionTag);
static uint64
DoPortalRunFetch(Portal portal, FetchDirection fdirection, long count, DestReceiver *dest);
static void
DoPortalRewind(Portal portal);

   
                   
   
QueryDesc *
CreateQueryDesc(PlannedStmt *plannedstmt, const char *sourceText, Snapshot snapshot, Snapshot crosscheck_snapshot, DestReceiver *dest, ParamListInfo params, QueryEnvironment *queryEnv, int instrument_options)
{
  QueryDesc *qd = (QueryDesc *)palloc(sizeof(QueryDesc));

  qd->operation = plannedstmt->commandType;                 
  qd->plannedstmt = plannedstmt;                       
  qd->sourceText = sourceText;                               
  qd->snapshot = RegisterSnapshot(snapshot);               
                         
  qd->crosscheck_snapshot = RegisterSnapshot(crosscheck_snapshot);
  qd->dest = dest;                      
  qd->params = params;                                         
  qd->queryEnv = queryEnv;
  qd->instrument_options = instrument_options;                              

                                                    
  qd->tupDesc = NULL;
  qd->estate = NULL;
  qd->planstate = NULL;
  qd->totaltime = NULL;

                        
  qd->already_executed = false;

  return qd;
}

   
                 
   
void
FreeQueryDesc(QueryDesc *qdesc)
{
                             
  Assert(qdesc->estate == NULL);

                            
  UnregisterSnapshot(qdesc->snapshot);
  UnregisterSnapshot(qdesc->crosscheck_snapshot);

                                               
  pfree(qdesc);
}

   
                
                                                                  
                                                        
   
                                     
                                            
                                 
                               
                                                                    
                                                          
   
                                                                     
   
                                                                       
                                                                
   
static void
ProcessQuery(PlannedStmt *plan, const char *sourceText, ParamListInfo params, QueryEnvironment *queryEnv, DestReceiver *dest, char *completionTag)
{
  QueryDesc *queryDesc;

     
                                 
     
  queryDesc = CreateQueryDesc(plan, sourceText, GetActiveSnapshot(), InvalidSnapshot, dest, params, queryEnv, 0);

     
                                                          
     
  ExecutorStart(queryDesc, 0);

     
                                 
     
  ExecutorRun(queryDesc, ForwardScanDirection, 0L, true);

     
                                                                  
     
  if (completionTag)
  {
    Oid lastOid;

    switch (queryDesc->operation)
    {
    case CMD_SELECT:
      snprintf(completionTag, COMPLETION_TAG_BUFSIZE, "SELECT " UINT64_FORMAT, queryDesc->estate->es_processed);
      break;
    case CMD_INSERT:
                                         
      lastOid = InvalidOid;
      snprintf(completionTag, COMPLETION_TAG_BUFSIZE, "INSERT %u " UINT64_FORMAT, lastOid, queryDesc->estate->es_processed);
      break;
    case CMD_UPDATE:
      snprintf(completionTag, COMPLETION_TAG_BUFSIZE, "UPDATE " UINT64_FORMAT, queryDesc->estate->es_processed);
      break;
    case CMD_DELETE:
      snprintf(completionTag, COMPLETION_TAG_BUFSIZE, "DELETE " UINT64_FORMAT, queryDesc->estate->es_processed);
      break;
    default:
      strcpy(completionTag, "???");
      break;
    }
  }

     
                                                                    
     
  ExecutorFinish(queryDesc);
  ExecutorEnd(queryDesc);

  FreeQueryDesc(queryDesc);
}

   
                        
                                                                        
   
                                                    
                                                                         
   
                                 
   
PortalStrategy
ChoosePortalStrategy(List *stmts)
{
  int nSetTag;
  ListCell *lc;

     
                                                                     
                                                                          
                                                                             
                                                   
     
  if (list_length(stmts) == 1)
  {
    Node *stmt = (Node *)linitial(stmts);

    if (IsA(stmt, Query))
    {
      Query *query = (Query *)stmt;

      if (query->canSetTag)
      {
        if (query->commandType == CMD_SELECT)
        {
          if (query->hasModifyingCTE)
          {
            return PORTAL_ONE_MOD_WITH;
          }
          else
          {
            return PORTAL_ONE_SELECT;
          }
        }
        if (query->commandType == CMD_UTILITY)
        {
          if (UtilityReturnsTuples(query->utilityStmt))
          {
            return PORTAL_UTIL_SELECT;
          }
                                                     
          return PORTAL_MULTI_QUERY;
        }
      }
    }
    else if (IsA(stmt, PlannedStmt))
    {
      PlannedStmt *pstmt = (PlannedStmt *)stmt;

      if (pstmt->canSetTag)
      {
        if (pstmt->commandType == CMD_SELECT)
        {
          if (pstmt->hasModifyingCTE)
          {
            return PORTAL_ONE_MOD_WITH;
          }
          else
          {
            return PORTAL_ONE_SELECT;
          }
        }
        if (pstmt->commandType == CMD_UTILITY)
        {
          if (UtilityReturnsTuples(pstmt->utilityStmt))
          {
            return PORTAL_UTIL_SELECT;
          }
                                                     
          return PORTAL_MULTI_QUERY;
        }
      }
    }
    else
    {
      elog(ERROR, "unrecognized node type: %d", (int)nodeTag(stmt));
    }
  }

     
                                                                           
                                                                             
                              
     
  nSetTag = 0;
  foreach (lc, stmts)
  {
    Node *stmt = (Node *)lfirst(lc);

    if (IsA(stmt, Query))
    {
      Query *query = (Query *)stmt;

      if (query->canSetTag)
      {
        if (++nSetTag > 1)
        {
          return PORTAL_MULTI_QUERY;                              
        }
        if (query->commandType == CMD_UTILITY || query->returningList == NIL)
        {
          return PORTAL_MULTI_QUERY;                              
        }
      }
    }
    else if (IsA(stmt, PlannedStmt))
    {
      PlannedStmt *pstmt = (PlannedStmt *)stmt;

      if (pstmt->canSetTag)
      {
        if (++nSetTag > 1)
        {
          return PORTAL_MULTI_QUERY;                              
        }
        if (pstmt->commandType == CMD_UTILITY || !pstmt->hasReturning)
        {
          return PORTAL_MULTI_QUERY;                              
        }
      }
    }
    else
    {
      elog(ERROR, "unrecognized node type: %d", (int)nodeTag(stmt));
    }
  }
  if (nSetTag == 1)
  {
    return PORTAL_ONE_RETURNING;
  }

                                      
  return PORTAL_MULTI_QUERY;
}

   
                         
                                                                      
                                                                      
   
                                   
   
List *
FetchPortalTargetList(Portal portal)
{
                                                                     
  if (portal->strategy == PORTAL_MULTI_QUERY)
  {
    return NIL;
  }
                                                              
  return FetchStatementTargetList((Node *)PortalGetPrimaryStmt(portal));
}

   
                            
                                                                         
                                                                         
   
                                                    
                                                                         
   
                                   
   
                                                                  
   
List *
FetchStatementTargetList(Node *stmt)
{
  if (stmt == NULL)
  {
    return NIL;
  }
  if (IsA(stmt, Query))
  {
    Query *query = (Query *)stmt;

    if (query->commandType == CMD_UTILITY)
    {
                                                   
      stmt = query->utilityStmt;
    }
    else
    {
      if (query->commandType == CMD_SELECT)
      {
        return query->targetList;
      }
      if (query->returningList)
      {
        return query->returningList;
      }
      return NIL;
    }
  }
  if (IsA(stmt, PlannedStmt))
  {
    PlannedStmt *pstmt = (PlannedStmt *)stmt;

    if (pstmt->commandType == CMD_UTILITY)
    {
                                                   
      stmt = pstmt->utilityStmt;
    }
    else
    {
      if (pstmt->commandType == CMD_SELECT)
      {
        return pstmt->planTree->targetlist;
      }
      if (pstmt->hasReturning)
      {
        return pstmt->planTree->targetlist;
      }
      return NIL;
    }
  }
  if (IsA(stmt, FetchStmt))
  {
    FetchStmt *fstmt = (FetchStmt *)stmt;
    Portal subportal;

    Assert(!fstmt->ismove);
    subportal = GetPortalByName(fstmt->portalname);
    Assert(PortalIsValid(subportal));
    return FetchPortalTargetList(subportal);
  }
  if (IsA(stmt, ExecuteStmt))
  {
    ExecuteStmt *estmt = (ExecuteStmt *)stmt;
    PreparedStatement *entry;

    entry = FetchPreparedStatement(estmt->name, true);
    return FetchPreparedStatementTargetList(entry);
  }
  return NIL;
}

   
               
                                    
   
                                                                          
                                          
   
                                                                          
                                                                 
   
                                                                          
                                                                          
                                                                        
                            
   
                                                                              
                                                                         
                                                                           
                            
   
                                                                          
                              
   
void
PortalStart(Portal portal, ParamListInfo params, int eflags, Snapshot snapshot)
{
  Portal saveActivePortal;
  ResourceOwner saveResourceOwner;
  MemoryContext savePortalContext;
  MemoryContext oldContext;
  QueryDesc *queryDesc;
  int myeflags;

  AssertArg(PortalIsValid(portal));
  AssertState(portal->status == PORTAL_DEFINED);

     
                                            
     
  saveActivePortal = ActivePortal;
  saveResourceOwner = CurrentResourceOwner;
  savePortalContext = PortalContext;
  PG_TRY();
  {
    ActivePortal = portal;
    if (portal->resowner)
    {
      CurrentResourceOwner = portal->resowner;
    }
    PortalContext = portal->portalContext;

    oldContext = MemoryContextSwitchTo(PortalContext);

                                                 
    portal->portalParams = params;

       
                                               
       
    portal->strategy = ChoosePortalStrategy(portal->stmts);

       
                                             
       
    switch (portal->strategy)
    {
    case PORTAL_ONE_SELECT:

                                                       
      if (snapshot)
      {
        PushActiveSnapshot(snapshot);
      }
      else
      {
        PushActiveSnapshot(GetTransactionSnapshot());
      }

         
                                                                   
                                                                 
                                                                     
                                                                     
                                                           
                                                     
         

         
                                                                   
                                      
         
      queryDesc = CreateQueryDesc(linitial_node(PlannedStmt, portal->stmts), portal->sourceText, GetActiveSnapshot(), InvalidSnapshot, None_Receiver, params, portal->queryEnv, 0);

         
                                                                
                                                                   
                             
         
      if (portal->cursorOptions & CURSOR_OPT_SCROLL)
      {
        myeflags = eflags | EXEC_FLAG_REWIND | EXEC_FLAG_BACKWARD;
      }
      else
      {
        myeflags = eflags;
      }

         
                                                              
         
      ExecutorStart(queryDesc, myeflags);

         
                                                            
         
      portal->queryDesc = queryDesc;

         
                                                               
         
      portal->tupDesc = queryDesc->tupDesc;

         
                                                        
         
      portal->atStart = true;
      portal->atEnd = false;                    
      portal->portalPos = 0;

      PopActiveSnapshot();
      break;

    case PORTAL_ONE_RETURNING:
    case PORTAL_ONE_MOD_WITH:

         
                                                                  
                                                           
         
      {
        PlannedStmt *pstmt;

        pstmt = PortalGetPrimaryStmt(portal);
        portal->tupDesc = ExecCleanTypeFromTL(pstmt->planTree->targetlist);
      }

         
                                                        
         
      portal->atStart = true;
      portal->atEnd = false;                    
      portal->portalPos = 0;
      break;

    case PORTAL_UTIL_SELECT:

         
                                                                   
                                    
         
      {
        PlannedStmt *pstmt = PortalGetPrimaryStmt(portal);

        Assert(pstmt->commandType == CMD_UTILITY);
        portal->tupDesc = UtilityTupleDescriptor(pstmt->utilityStmt);
      }

         
                                                        
         
      portal->atStart = true;
      portal->atEnd = false;                    
      portal->portalPos = 0;
      break;

    case PORTAL_MULTI_QUERY:
                               
      portal->tupDesc = NULL;
      break;
    }
  }
  PG_CATCH();
  {
                                                             
    MarkPortalFailed(portal);

                                                 
    ActivePortal = saveActivePortal;
    CurrentResourceOwner = saveResourceOwner;
    PortalContext = savePortalContext;

    PG_RE_THROW();
  }
  PG_END_TRY();

  MemoryContextSwitchTo(oldContext);

  ActivePortal = saveActivePortal;
  CurrentResourceOwner = saveResourceOwner;
  PortalContext = savePortalContext;

  portal->status = PORTAL_READY;
}

   
                         
                                                   
   
                                                                        
                                                                              
                                
   
                                                                            
   
void
PortalSetResultFormat(Portal portal, int nFormats, int16 *formats)
{
  int natts;
  int i;

                                                
  if (portal->tupDesc == NULL)
  {
    return;
  }
  natts = portal->tupDesc->natts;
  portal->formats = (int16 *)MemoryContextAlloc(portal->portalContext, natts * sizeof(int16));
  if (nFormats > 1)
  {
                                          
    if (nFormats != natts)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("bind message has %d result formats but query has %d columns", nFormats, natts)));
    }
    memcpy(portal->formats, formats, natts * sizeof(int16));
  }
  else if (nFormats > 0)
  {
                                                      
    int16 format1 = formats[0];

    for (i = 0; i < natts; i++)
    {
      portal->formats[i] = format1;
    }
  }
  else
  {
                                            
    for (i = 0; i < natts; i++)
    {
      portal->formats[i] = 0;
    }
  }
}

   
             
                                     
   
                                                                         
                                                                         
                                                                         
                                                             
   
                                                                      
                                                     
   
                                                           
   
                                                        
   
                                                                    
                                                          
                                                        
   
                                                                       
                                                       
   
bool
PortalRun(Portal portal, long count, bool isTopLevel, bool run_once, DestReceiver *dest, DestReceiver *altdest, char *completionTag)
{
  bool result;
  uint64 nprocessed;
  ResourceOwner saveTopTransactionResourceOwner;
  MemoryContext saveTopTransactionContext;
  Portal saveActivePortal;
  ResourceOwner saveResourceOwner;
  MemoryContext savePortalContext;
  MemoryContext saveMemoryContext;

  AssertArg(PortalIsValid(portal));

  TRACE_POSTGRESQL_QUERY_EXECUTE_START();

                                                 
  if (completionTag)
  {
    completionTag[0] = '\0';
  }

  if (log_executor_stats && portal->strategy != PORTAL_MULTI_QUERY)
  {
    elog(DEBUG3, "PortalRun");
                                                         
    ResetUsage();
  }

     
                                                            
     
  MarkPortalActive(portal);

                                                                 
  Assert(!portal->run_once || run_once);
  portal->run_once = run_once;

     
                                            
     
                                                                          
                                                                         
                                                                             
                                                                      
                                                                     
                                                                          
                                                                             
                                                              
                                                                           
                                                                 
     
  saveTopTransactionResourceOwner = TopTransactionResourceOwner;
  saveTopTransactionContext = TopTransactionContext;
  saveActivePortal = ActivePortal;
  saveResourceOwner = CurrentResourceOwner;
  savePortalContext = PortalContext;
  saveMemoryContext = CurrentMemoryContext;
  PG_TRY();
  {
    ActivePortal = portal;
    if (portal->resowner)
    {
      CurrentResourceOwner = portal->resowner;
    }
    PortalContext = portal->portalContext;

    MemoryContextSwitchTo(PortalContext);

    switch (portal->strategy)
    {
    case PORTAL_ONE_SELECT:
    case PORTAL_ONE_RETURNING:
    case PORTAL_ONE_MOD_WITH:
    case PORTAL_UTIL_SELECT:

         
                                                                
                                                                   
                                         
         
      if (portal->strategy != PORTAL_ONE_SELECT && !portal->holdStore)
      {
        FillPortalStore(portal, isTopLevel);
      }

         
                                               
         
      nprocessed = PortalRunSelect(portal, true, count, dest);

         
                                                                    
                                                                    
                                           
         
      if (completionTag && portal->commandTag)
      {
        if (strcmp(portal->commandTag, "SELECT") == 0)
        {
          snprintf(completionTag, COMPLETION_TAG_BUFSIZE, "SELECT " UINT64_FORMAT, nprocessed);
        }
        else
        {
          strcpy(completionTag, portal->commandTag);
        }
      }

                                  
      portal->status = PORTAL_READY;

         
                                                                     
         
      result = portal->atEnd;
      break;

    case PORTAL_MULTI_QUERY:
      PortalRunMulti(portal, isTopLevel, false, dest, altdest, completionTag);

                                                            
      MarkPortalDone(portal);

                                              
      result = true;
      break;

    default:
      elog(ERROR, "unrecognized portal strategy: %d", (int)portal->strategy);
      result = false;                          
      break;
    }
  }
  PG_CATCH();
  {
                                                             
    MarkPortalFailed(portal);

                                                 
    if (saveMemoryContext == saveTopTransactionContext)
    {
      MemoryContextSwitchTo(TopTransactionContext);
    }
    else
    {
      MemoryContextSwitchTo(saveMemoryContext);
    }
    ActivePortal = saveActivePortal;
    if (saveResourceOwner == saveTopTransactionResourceOwner)
    {
      CurrentResourceOwner = TopTransactionResourceOwner;
    }
    else
    {
      CurrentResourceOwner = saveResourceOwner;
    }
    PortalContext = savePortalContext;

    PG_RE_THROW();
  }
  PG_END_TRY();

  if (saveMemoryContext == saveTopTransactionContext)
  {
    MemoryContextSwitchTo(TopTransactionContext);
  }
  else
  {
    MemoryContextSwitchTo(saveMemoryContext);
  }
  ActivePortal = saveActivePortal;
  if (saveResourceOwner == saveTopTransactionResourceOwner)
  {
    CurrentResourceOwner = TopTransactionResourceOwner;
  }
  else
  {
    CurrentResourceOwner = saveResourceOwner;
  }
  PortalContext = savePortalContext;

  if (log_executor_stats && portal->strategy != PORTAL_MULTI_QUERY)
  {
    ShowUsage("EXECUTOR STATISTICS");
  }

  TRACE_POSTGRESQL_QUERY_EXECUTE_DONE();

  return result;
}

   
                   
                                                                 
                                                                      
                                                       
   
                                                                           
                                                         
   
                                                                         
                                                                         
                                                      
   
                                                                      
                          
   
                                                                     
   
static uint64
PortalRunSelect(Portal portal, bool forward, long count, DestReceiver *dest)
{
  QueryDesc *queryDesc;
  ScanDirection direction;
  uint64 nprocessed;

     
                                                                           
                                                         
     
  queryDesc = portal->queryDesc;

                                                                        
  Assert(queryDesc || portal->holdStore);

     
                                                                        
                                                                             
                                                                        
                                      
     
  if (queryDesc)
  {
    queryDesc->dest = dest;
  }

     
                                                                           
                                                                           
                                                                         
                                                                         
                                                                         
                                                                             
                                                                           
                                                                           
                
     
  if (forward)
  {
    if (portal->atEnd || count <= 0)
    {
      direction = NoMovementScanDirection;
      count = 0;                                            
    }
    else
    {
      direction = ForwardScanDirection;
    }

                                                        
    if (count == FETCH_ALL)
    {
      count = 0;
    }

    if (portal->holdStore)
    {
      nprocessed = RunFromStore(portal, direction, (uint64)count, dest);
    }
    else
    {
      PushActiveSnapshot(queryDesc->snapshot);
      ExecutorRun(queryDesc, direction, (uint64)count, portal->run_once);
      nprocessed = queryDesc->estate->es_processed;
      PopActiveSnapshot();
    }

    if (!ScanDirectionIsNoMovement(direction))
    {
      if (nprocessed > 0)
      {
        portal->atStart = false;                            
      }
      if (count == 0 || nprocessed < (uint64)count)
      {
        portal->atEnd = true;                           
      }
      portal->portalPos += nprocessed;
    }
  }
  else
  {
    if (portal->cursorOptions & CURSOR_OPT_NO_SCROLL)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cursor can only scan forward"), errhint("Declare it with SCROLL option to enable backward scan.")));
    }

    if (portal->atStart || count <= 0)
    {
      direction = NoMovementScanDirection;
      count = 0;                                            
    }
    else
    {
      direction = BackwardScanDirection;
    }

                                                        
    if (count == FETCH_ALL)
    {
      count = 0;
    }

    if (portal->holdStore)
    {
      nprocessed = RunFromStore(portal, direction, (uint64)count, dest);
    }
    else
    {
      PushActiveSnapshot(queryDesc->snapshot);
      ExecutorRun(queryDesc, direction, (uint64)count, portal->run_once);
      nprocessed = queryDesc->estate->es_processed;
      PopActiveSnapshot();
    }

    if (!ScanDirectionIsNoMovement(direction))
    {
      if (nprocessed > 0 && portal->atEnd)
      {
        portal->atEnd = false;                           
        portal->portalPos++;                                 
      }
      if (count == 0 || nprocessed < (uint64)count)
      {
        portal->atStart = true;                           
        portal->portalPos = 0;
      }
      else
      {
        portal->portalPos -= nprocessed;
      }
    }
  }

  return nprocessed;
}

   
                   
                                                                        
   
                                                                   
                                  
   
static void
FillPortalStore(Portal portal, bool isTopLevel)
{
  DestReceiver *treceiver;
  char completionTag[COMPLETION_TAG_BUFSIZE];

  PortalCreateHoldStore(portal);
  treceiver = CreateDestReceiver(DestTuplestore);
  SetTuplestoreDestReceiverParams(treceiver, portal->holdStore, portal->holdContext, false);

  completionTag[0] = '\0';

  switch (portal->strategy)
  {
  case PORTAL_ONE_RETURNING:
  case PORTAL_ONE_MOD_WITH:

       
                                                            
                                                                    
                                                                    
                                                                     
       
    PortalRunMulti(portal, isTopLevel, true, treceiver, None_Receiver, completionTag);
    break;

  case PORTAL_UTIL_SELECT:
    PortalRunUtility(portal, linitial_node(PlannedStmt, portal->stmts), isTopLevel, true, treceiver, completionTag);
    break;

  default:
    elog(ERROR, "unsupported portal strategy: %d", (int)portal->strategy);
    break;
  }

                                                                  
  if (completionTag[0] != '\0')
  {
    portal->commandTag = pstrdup(completionTag);
  }

  treceiver->rDestroy(treceiver);
}

   
                
                                                
   
                                                                  
                                                                           
                                                                          
   
                                                                              
                                                                            
                         
   
static uint64
RunFromStore(Portal portal, ScanDirection direction, uint64 count, DestReceiver *dest)
{
  uint64 current_tuple_count = 0;
  TupleTableSlot *slot;

  slot = MakeSingleTupleTableSlot(portal->tupDesc, &TTSOpsMinimalTuple);

  dest->rStartup(dest, CMD_SELECT, portal->tupDesc);

  if (ScanDirectionIsNoMovement(direction))
  {
                                                      
  }
  else
  {
    bool forward = ScanDirectionIsForward(direction);

    for (;;)
    {
      MemoryContext oldcontext;
      bool ok;

      oldcontext = MemoryContextSwitchTo(portal->holdContext);

      ok = tuplestore_gettupleslot(portal->holdStore, forward, false, slot);

      MemoryContextSwitchTo(oldcontext);

      if (!ok)
      {
        break;
      }

         
                                                                         
                                                                        
                       
         
      if (!dest->receiveSlot(slot, dest))
      {
        break;
      }

      ExecClearTuple(slot);

         
                                                                      
                                                                        
                         
         
      current_tuple_count++;
      if (count && count == current_tuple_count)
      {
        break;
      }
    }
  }

  dest->rShutdown(dest);

  ExecDropSingleTupleTableSlot(slot);

  return current_tuple_count;
}

   
                    
                                                 
   
static void
PortalRunUtility(Portal portal, PlannedStmt *pstmt, bool isTopLevel, bool setHoldSnapshot, DestReceiver *dest, char *completionTag)
{
     
                                             
     
  if (PlannedStmtRequiresSnapshot(pstmt))
  {
    Snapshot snapshot = GetTransactionSnapshot();

                                                                          
    if (setHoldSnapshot)
    {
      snapshot = RegisterSnapshot(snapshot);
      portal->holdSnapshot = snapshot;
    }

       
                                                                        
                                                                    
                                                                       
                                                                       
       
    PushActiveSnapshotWithLevel(snapshot, portal->createLevel);
                                                                    
    portal->portalSnapshot = GetActiveSnapshot();
  }
  else
  {
    portal->portalSnapshot = NULL;
  }

  ProcessUtility(pstmt, portal->sourceText, isTopLevel ? PROCESS_UTILITY_TOPLEVEL : PROCESS_UTILITY_QUERY, portal->portalParams, portal->queryEnv, dest, completionTag);

                                                        
  MemoryContextSwitchTo(portal->portalContext);

     
                                                                            
                                                                             
                                                                         
                                                                          
     
  if (portal->portalSnapshot != NULL && ActiveSnapshotSet())
  {
    Assert(portal->portalSnapshot == GetActiveSnapshot());
    PopActiveSnapshot();
  }
  portal->portalSnapshot = NULL;
}

   
                  
                                                                  
                                
   
static void
PortalRunMulti(Portal portal, bool isTopLevel, bool setHoldSnapshot, DestReceiver *dest, DestReceiver *altdest, char *completionTag)
{
  bool active_snapshot_set = false;
  ListCell *stmtlist_item;

     
                                                                       
                                                                             
                                                                            
                                                                             
                                                                          
                                                                          
                                                                     
               
     
  if (dest->mydest == DestRemoteExecute)
  {
    dest = None_Receiver;
  }
  if (altdest->mydest == DestRemoteExecute)
  {
    altdest = None_Receiver;
  }

     
                                                                             
                              
     
  foreach (stmtlist_item, portal->stmts)
  {
    PlannedStmt *pstmt = lfirst_node(PlannedStmt, stmtlist_item);

       
                                                        
       
    CHECK_FOR_INTERRUPTS();

    if (pstmt->utilityStmt == NULL)
    {
         
                                    
         
      TRACE_POSTGRESQL_QUERY_EXECUTE_START();

      if (log_executor_stats)
      {
        ResetUsage();
      }

         
                                                                        
                                                                     
                                                                     
                  
         
      if (!active_snapshot_set)
      {
        Snapshot snapshot = GetTransactionSnapshot();

                                                                  
        if (setHoldSnapshot)
        {
          snapshot = RegisterSnapshot(snapshot);
          portal->holdSnapshot = snapshot;
        }

           
                                                                  
                                                                     
                                                                   
                                                                      
                                                                      
                                                                     
                                        
           
        PushCopiedSnapshot(snapshot);

           
                                                              
                                                              
           

        active_snapshot_set = true;
      }
      else
      {
        UpdateActiveSnapshotCommandId();
      }

      if (pstmt->canSetTag)
      {
                                          
        ProcessQuery(pstmt, portal->sourceText, portal->portalParams, portal->queryEnv, dest, completionTag);
      }
      else
      {
                                                  
        ProcessQuery(pstmt, portal->sourceText, portal->portalParams, portal->queryEnv, altdest, NULL);
      }

      if (log_executor_stats)
      {
        ShowUsage("EXECUTOR STATISTICS");
      }

      TRACE_POSTGRESQL_QUERY_EXECUTE_DONE();
    }
    else
    {
         
                                                            
         
                                                                         
                                                                        
                                                                        
                                                                      
                                                                     
                                                                        
                                        
         
      if (pstmt->canSetTag)
      {
        Assert(!active_snapshot_set);
                                          
        PortalRunUtility(portal, pstmt, isTopLevel, false, dest, completionTag);
      }
      else
      {
        Assert(IsA(pstmt->utilityStmt, NotifyStmt));
                                                  
        PortalRunUtility(portal, pstmt, isTopLevel, false, altdest, NULL);
      }
    }

       
                                                              
       
    Assert(portal->portalContext == CurrentMemoryContext);

    MemoryContextDeleteChildren(portal->portalContext);

       
                                                                      
                                                                    
                                                                          
                                                                           
                                                                          
                
       
    if (portal->stmts == NIL)
    {
      break;
    }

       
                                                                         
            
       
    if (lnext(stmtlist_item) != NULL)
    {
      CommandCounterIncrement();
    }
  }

                                          
  if (active_snapshot_set)
  {
    PopActiveSnapshot();
  }

     
                                                                          
                                                        
     
                                                                            
                                                                           
                                                                            
                                                                             
                                                                           
                                                                           
                                                                    
     
  if (completionTag && completionTag[0] == '\0')
  {
    if (portal->commandTag)
    {
      strcpy(completionTag, portal->commandTag);
    }
    if (strcmp(completionTag, "SELECT") == 0)
    {
      sprintf(completionTag, "SELECT 0 0");
    }
    else if (strcmp(completionTag, "INSERT") == 0)
    {
      strcpy(completionTag, "INSERT 0 0");
    }
    else if (strcmp(completionTag, "UPDATE") == 0)
    {
      strcpy(completionTag, "UPDATE 0");
    }
    else if (strcmp(completionTag, "DELETE") == 0)
    {
      strcpy(completionTag, "DELETE 0");
    }
  }
}

   
                  
                                                                  
   
                                                                             
   
                                                                         
                                                                         
                                                      
   
                                                                     
   
uint64
PortalRunFetch(Portal portal, FetchDirection fdirection, long count, DestReceiver *dest)
{
  uint64 result;
  Portal saveActivePortal;
  ResourceOwner saveResourceOwner;
  MemoryContext savePortalContext;
  MemoryContext oldContext;

  AssertArg(PortalIsValid(portal));

     
                                                            
     
  MarkPortalActive(portal);

                                                      
  Assert(!portal->run_once);

     
                                            
     
  saveActivePortal = ActivePortal;
  saveResourceOwner = CurrentResourceOwner;
  savePortalContext = PortalContext;
  PG_TRY();
  {
    ActivePortal = portal;
    if (portal->resowner)
    {
      CurrentResourceOwner = portal->resowner;
    }
    PortalContext = portal->portalContext;

    oldContext = MemoryContextSwitchTo(PortalContext);

    switch (portal->strategy)
    {
    case PORTAL_ONE_SELECT:
      result = DoPortalRunFetch(portal, fdirection, count, dest);
      break;

    case PORTAL_ONE_RETURNING:
    case PORTAL_ONE_MOD_WITH:
    case PORTAL_UTIL_SELECT:

         
                                                                
                                             
         
      if (!portal->holdStore)
      {
        FillPortalStore(portal, false                 );
      }

         
                                               
         
      result = DoPortalRunFetch(portal, fdirection, count, dest);
      break;

    default:
      elog(ERROR, "unsupported portal strategy");
      result = 0;                          
      break;
    }
  }
  PG_CATCH();
  {
                                                             
    MarkPortalFailed(portal);

                                                 
    ActivePortal = saveActivePortal;
    CurrentResourceOwner = saveResourceOwner;
    PortalContext = savePortalContext;

    PG_RE_THROW();
  }
  PG_END_TRY();

  MemoryContextSwitchTo(oldContext);

                              
  portal->status = PORTAL_READY;

  ActivePortal = saveActivePortal;
  CurrentResourceOwner = saveResourceOwner;
  PortalContext = savePortalContext;

  return result;
}

   
                    
                                                                    
   
                                                                               
                                                         
   
                                                                     
   
static uint64
DoPortalRunFetch(Portal portal, FetchDirection fdirection, long count, DestReceiver *dest)
{
  bool forward;

  Assert(portal->strategy == PORTAL_ONE_SELECT || portal->strategy == PORTAL_ONE_RETURNING || portal->strategy == PORTAL_ONE_MOD_WITH || portal->strategy == PORTAL_UTIL_SELECT);

     
                                                                           
                                                                            
                                                                             
                                                                           
                                                                       
                                                                        
                                  
     
  switch (fdirection)
  {
  case FETCH_FORWARD:
    if (count < 0)
    {
      fdirection = FETCH_BACKWARD;
      count = -count;
    }
                                                              
    break;
  case FETCH_BACKWARD:
    if (count < 0)
    {
      fdirection = FETCH_FORWARD;
      count = -count;
    }
                                                             
    break;
  case FETCH_ABSOLUTE:
    if (count > 0)
    {
         
                                                                   
                            
         
                                                                   
                                                       
         
                                                                    
                                                                    
                                                               
                                                                
         
                                                               
                   
         
      if ((uint64)(count - 1) <= portal->portalPos / 2 || portal->portalPos >= (uint64)LONG_MAX)
      {
        DoPortalRewind(portal);
        if (count > 1)
        {
          PortalRunSelect(portal, true, count - 1, None_Receiver);
        }
      }
      else
      {
        long pos = (long)portal->portalPos;

        if (portal->atEnd)
        {
          pos++;                                      
        }
        if (count <= pos)
        {
          PortalRunSelect(portal, false, pos - count + 1, None_Receiver);
        }
        else if (count > pos + 1)
        {
          PortalRunSelect(portal, true, count - pos - 1, None_Receiver);
        }
      }
      return PortalRunSelect(portal, true, 1L, dest);
    }
    else if (count < 0)
    {
         
                                                                
                                                                  
                                                                    
                                                                     
                                                            
         
      PortalRunSelect(portal, true, FETCH_ALL, None_Receiver);
      if (count < -1)
      {
        PortalRunSelect(portal, false, -count - 1, None_Receiver);
      }
      return PortalRunSelect(portal, false, 1L, dest);
    }
    else
    {
                      
                                             
      DoPortalRewind(portal);
      return PortalRunSelect(portal, true, 0L, dest);
    }
    break;
  case FETCH_RELATIVE:
    if (count > 0)
    {
         
                                                                     
         
      if (count > 1)
      {
        PortalRunSelect(portal, true, count - 1, None_Receiver);
      }
      return PortalRunSelect(portal, true, 1L, dest);
    }
    else if (count < 0)
    {
         
                                                                     
               
         
      if (count < -1)
      {
        PortalRunSelect(portal, false, -count - 1, None_Receiver);
      }
      return PortalRunSelect(portal, false, 1L, dest);
    }
    else
    {
                      
                                                          
      fdirection = FETCH_FORWARD;
    }
    break;
  default:
    elog(ERROR, "bogus direction");
    break;
  }

     
                                                                            
           
     
  forward = (fdirection == FETCH_FORWARD);

     
                                                                    
     
  if (count == 0)
  {
    bool on_row;

                                  
    on_row = (!portal->atStart && !portal->atEnd);

    if (dest->mydest == DestNone)
    {
                                                                     
      return on_row ? 1 : 0;
    }
    else
    {
         
                                                                        
                                                                       
                                                                       
                                                                      
                                                   
         
      if (on_row)
      {
        PortalRunSelect(portal, false, 1L, None_Receiver);
                                             
        count = 1;
        forward = true;
      }
    }
  }

     
                                               
     
  if (!forward && count == FETCH_ALL && dest->mydest == DestNone)
  {
    uint64 result = portal->portalPos;

    if (result > 0 && !portal->atEnd)
    {
      result--;
    }
    DoPortalRewind(portal);
    return result;
  }

  return PortalRunSelect(portal, forward, count, dest);
}

   
                                                      
   
static void
DoPortalRewind(Portal portal)
{
  QueryDesc *queryDesc;

     
                                                                          
                                                                         
     
  if (portal->atStart && !portal->atEnd)
  {
    return;
  }

     
                                                                             
                                                                            
                                                                           
                                                                       
     
  if ((portal->cursorOptions & CURSOR_OPT_NO_SCROLL) && portal->holdStore)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cursor can only scan forward"), errhint("Declare it with SCROLL option to enable backward scan.")));
  }

                                        
  if (portal->holdStore)
  {
    MemoryContext oldcontext;

    oldcontext = MemoryContextSwitchTo(portal->holdContext);
    tuplestore_rescan(portal->holdStore);
    MemoryContextSwitchTo(oldcontext);
  }

                                  
  queryDesc = portal->queryDesc;
  if (queryDesc)
  {
    PushActiveSnapshot(queryDesc->snapshot);
    ExecutorRewind(queryDesc);
    PopActiveSnapshot();
  }

  portal->atStart = true;
  portal->atEnd = false;
  portal->portalPos = 0;
}

   
                                                         
   
bool
PlannedStmtRequiresSnapshot(PlannedStmt *pstmt)
{
  Node *utilityStmt = pstmt->utilityStmt;

                                                                       
  if (utilityStmt == NULL)
  {
    return true;
  }

     
                                                                          
                                                                             
                      
     
                                                                         
                                                                            
                                                                          
                                                                        
                                                                             
                                                                 
                                                                 
     
  if (IsA(utilityStmt, TransactionStmt) || IsA(utilityStmt, LockStmt) || IsA(utilityStmt, VariableSetStmt) || IsA(utilityStmt, VariableShowStmt) || IsA(utilityStmt, ConstraintsSetStmt) ||
                                           
      IsA(utilityStmt, FetchStmt) || IsA(utilityStmt, ListenStmt) || IsA(utilityStmt, NotifyStmt) || IsA(utilityStmt, UnlistenStmt) || IsA(utilityStmt, CheckPointStmt))
  {
    return false;
  }

  return true;
}

   
                                                                          
   
                                                                        
                                                                    
                                                                         
                                                                          
                                                                      
                                                            
   
void
EnsurePortalSnapshotExists(void)
{
  Portal portal;

     
                                                                        
                                                                         
                                                                    
     
  if (ActiveSnapshotSet())
  {
    return;
  }

                                                    
  portal = ActivePortal;
  if (unlikely(portal == NULL))
  {
    elog(ERROR, "cannot execute SQL without an outer snapshot or portal");
  }
  Assert(portal->portalSnapshot == NULL);

     
                                                                       
                                                                            
                                                                          
                                                      
     
  PushActiveSnapshotWithLevel(GetTransactionSnapshot(), portal->createLevel);
                                                                  
  portal->portalSnapshot = GetActiveSnapshot();
}
