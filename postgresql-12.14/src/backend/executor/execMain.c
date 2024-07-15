                                                                            
   
              
                                           
   
                      
                   
                 
                    
                 
   
                                                                     
                                                                  
   
                                                                     
                                                                  
                                                            
   
                                                                          
                                                                            
                                                                         
                                                                            
                                                
   
                                                                      
                                                                     
                                       
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "catalog/pg_publication.h"
#include "commands/matview.h"
#include "commands/trigger.h"
#include "executor/execdebug.h"
#include "executor/nodeSubplan.h"
#include "foreign/fdwapi.h"
#include "jit/jit.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "tcop/utility.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/rls.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"

                                                                      
ExecutorStart_hook_type ExecutorStart_hook = NULL;
ExecutorRun_hook_type ExecutorRun_hook = NULL;
ExecutorFinish_hook_type ExecutorFinish_hook = NULL;
ExecutorEnd_hook_type ExecutorEnd_hook = NULL;

                                                          
ExecutorCheckPerms_hook_type ExecutorCheckPerms_hook = NULL;

                                                           
static void
InitPlan(QueryDesc *queryDesc, int eflags);
static void
CheckValidRowMarkRel(Relation rel, RowMarkType markType);
static void
ExecPostprocessPlan(EState *estate);
static void
ExecEndPlan(PlanState *planstate, EState *estate);
static void
ExecutePlan(EState *estate, PlanState *planstate, bool use_parallel_mode, CmdType operation, bool sendTuples, uint64 numberTuples, ScanDirection direction, DestReceiver *dest, bool execute_once);
static bool
ExecCheckRTEPerms(RangeTblEntry *rte);
static bool
ExecCheckRTEPermsModified(Oid relOid, Oid userid, Bitmapset *modifiedCols, AclMode requiredPerms);
static void
ExecCheckXactReadOnly(PlannedStmt *plannedstmt);
static char *
ExecBuildSlotValueDescription(Oid reloid, TupleTableSlot *slot, TupleDesc tupdesc, Bitmapset *modifiedCols, int maxfieldlen);
static void
EvalPlanQualStart(EPQState *epqstate, Plan *planTree);

                        

                                                                    
                  
   
                                                                         
               
   
                                                                              
                                                                               
                                                                           
                                                                        
   
                                                         
   
                                                                           
                                                               
   
                                                                  
                                                                  
                                           
   
                                                                    
   
void
ExecutorStart(QueryDesc *queryDesc, int eflags)
{
  if (ExecutorStart_hook)
  {
    (*ExecutorStart_hook)(queryDesc, eflags);
  }
  else
  {
    standard_ExecutorStart(queryDesc, eflags);
  }
}

void
standard_ExecutorStart(QueryDesc *queryDesc, int eflags)
{
  EState *estate;
  MemoryContext oldcontext;

                                                            
  Assert(queryDesc != NULL);
  Assert(queryDesc->estate == NULL);

     
                                                                         
                                                                        
     
                                                                        
                                                                          
                                                                             
                                                                           
                                                                        
             
     
                                                                           
                                                                             
                                       
     
  if ((XactReadOnly || IsInParallelMode()) && !(eflags & EXEC_FLAG_EXPLAIN_ONLY))
  {
    ExecCheckXactReadOnly(queryDesc->plannedstmt);
  }

     
                                                                     
     
  estate = CreateExecutorState();
  queryDesc->estate = estate;

  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

     
                                                                       
                                       
     
  estate->es_param_list_info = queryDesc->params;

  if (queryDesc->plannedstmt->paramExecTypes != NIL)
  {
    int nParamExec;

    nParamExec = list_length(queryDesc->plannedstmt->paramExecTypes);
    estate->es_param_exec_vals = (ParamExecData *)palloc0(nParamExec * sizeof(ParamExecData));
  }

  estate->es_sourceText = queryDesc->sourceText;

     
                                                            
     
  estate->es_queryEnv = queryDesc->queryEnv;

     
                                                                           
     
  switch (queryDesc->operation)
  {
  case CMD_SELECT:

       
                                                                     
              
       
    if (queryDesc->plannedstmt->rowMarks != NIL || queryDesc->plannedstmt->hasModifyingCTE)
    {
      estate->es_output_cid = GetCurrentCommandId(true);
    }

       
                                                                      
                                                                       
                                                                      
                                                       
       
    if (!queryDesc->plannedstmt->hasModifyingCTE)
    {
      eflags |= EXEC_FLAG_SKIP_TRIGGERS;
    }
    break;

  case CMD_INSERT:
  case CMD_DELETE:
  case CMD_UPDATE:
    estate->es_output_cid = GetCurrentCommandId(true);
    break;

  default:
    elog(ERROR, "unrecognized operation code: %d", (int)queryDesc->operation);
    break;
  }

     
                                                      
     
  estate->es_snapshot = RegisterSnapshot(queryDesc->snapshot);
  estate->es_crosscheck_snapshot = RegisterSnapshot(queryDesc->crosscheck_snapshot);
  estate->es_top_eflags = eflags;
  estate->es_instrument = queryDesc->instrument_options;
  estate->es_jit_flags = queryDesc->plannedstmt->jitFlags;

     
                                                                       
                                                                          
     
  if (!(eflags & (EXEC_FLAG_SKIP_TRIGGERS | EXEC_FLAG_EXPLAIN_ONLY)))
  {
    AfterTriggerBeginQuery();
  }

     
                                    
     
  InitPlan(queryDesc, eflags);

  MemoryContextSwitchTo(oldcontext);
}

                                                                    
                
   
                                                                
                                                               
                
   
                                                 
   
                                                                 
                                                              
                                                                 
   
                                                                    
                                                                   
                                                                         
                                
   
                                                                     
                                                                        
                                                         
                          
   
                                                                   
                                                                 
                                          
   
                                                                    
   
void
ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count, bool execute_once)
{
  if (ExecutorRun_hook)
  {
    (*ExecutorRun_hook)(queryDesc, direction, count, execute_once);
  }
  else
  {
    standard_ExecutorRun(queryDesc, direction, count, execute_once);
  }
}

void
standard_ExecutorRun(QueryDesc *queryDesc, ScanDirection direction, uint64 count, bool execute_once)
{
  EState *estate;
  CmdType operation;
  DestReceiver *dest;
  bool sendTuples;
  MemoryContext oldcontext;

                     
  Assert(queryDesc != NULL);

  estate = queryDesc->estate;

  Assert(estate != NULL);
  Assert(!(estate->es_top_eflags & EXEC_FLAG_EXPLAIN_ONLY));

     
                                          
     
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

                                                         
  if (queryDesc->totaltime)
  {
    InstrStartNode(queryDesc->totaltime);
  }

     
                                                                          
     
  operation = queryDesc->operation;
  dest = queryDesc->dest;

     
                                                           
     
  estate->es_processed = 0;

  sendTuples = (operation == CMD_SELECT || queryDesc->plannedstmt->hasReturning);

  if (sendTuples)
  {
    dest->rStartup(dest, operation, queryDesc->tupDesc);
  }

     
              
     
  if (!ScanDirectionIsNoMovement(direction))
  {
    if (execute_once && queryDesc->already_executed)
    {
      elog(ERROR, "can't re-execute query flagged for single execution");
    }
    queryDesc->already_executed = true;

    ExecutePlan(estate, queryDesc->planstate, queryDesc->plannedstmt->parallelModeNeeded, operation, sendTuples, count, direction, dest, execute_once);
  }

     
                                               
     
  if (sendTuples)
  {
    dest->rShutdown(dest);
  }

  if (queryDesc->totaltime)
  {
    InstrStopNode(queryDesc->totaltime, estate->es_processed);
  }

  MemoryContextSwitchTo(oldcontext);
}

                                                                    
                   
   
                                                                 
                                                              
                                                               
                                                
   
                                                                   
                                                                    
                                             
   
                                                                    
   
void
ExecutorFinish(QueryDesc *queryDesc)
{
  if (ExecutorFinish_hook)
  {
    (*ExecutorFinish_hook)(queryDesc);
  }
  else
  {
    standard_ExecutorFinish(queryDesc);
  }
}

void
standard_ExecutorFinish(QueryDesc *queryDesc)
{
  EState *estate;
  MemoryContext oldcontext;

                     
  Assert(queryDesc != NULL);

  estate = queryDesc->estate;

  Assert(estate != NULL);
  Assert(!(estate->es_top_eflags & EXEC_FLAG_EXPLAIN_ONLY));

                                                                   
  Assert(!estate->es_finished);

                                            
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

                                                         
  if (queryDesc->totaltime)
  {
    InstrStartNode(queryDesc->totaltime);
  }

                                           
  ExecPostprocessPlan(estate);

                                                         
  if (!(estate->es_top_eflags & EXEC_FLAG_SKIP_TRIGGERS))
  {
    AfterTriggerEndQuery(estate);
  }

  if (queryDesc->totaltime)
  {
    InstrStopNode(queryDesc->totaltime, 0);
  }

  MemoryContextSwitchTo(oldcontext);

  estate->es_finished = true;
}

                                                                    
                
   
                                                               
               
   
                                                                   
                                                                 
                                          
   
                                                                    
   
void
ExecutorEnd(QueryDesc *queryDesc)
{
  if (ExecutorEnd_hook)
  {
    (*ExecutorEnd_hook)(queryDesc);
  }
  else
  {
    standard_ExecutorEnd(queryDesc);
  }
}

void
standard_ExecutorEnd(QueryDesc *queryDesc)
{
  EState *estate;
  MemoryContext oldcontext;

                     
  Assert(queryDesc != NULL);

  estate = queryDesc->estate;

  Assert(estate != NULL);

     
                                                                             
                                                                           
                              
     
  Assert(estate->es_finished || (estate->es_top_eflags & EXEC_FLAG_EXPLAIN_ONLY));

     
                                                             
     
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

  ExecEndPlan(queryDesc->planstate, estate);

                                  
  UnregisterSnapshot(estate->es_snapshot);
  UnregisterSnapshot(estate->es_crosscheck_snapshot);

     
                                                     
     
  MemoryContextSwitchTo(oldcontext);

     
                                                                       
                                            
     
  FreeExecutorState(estate);

                                                               
  queryDesc->tupDesc = NULL;
  queryDesc->estate = NULL;
  queryDesc->planstate = NULL;
  queryDesc->totaltime = NULL;
}

                                                                    
                   
   
                                                                 
                  
                                                                    
   
void
ExecutorRewind(QueryDesc *queryDesc)
{
  EState *estate;
  MemoryContext oldcontext;

                     
  Assert(queryDesc != NULL);

  estate = queryDesc->estate;

  Assert(estate != NULL);

                                                             
  Assert(queryDesc->operation == CMD_SELECT);

     
                                          
     
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

     
                 
     
  ExecReScan(queryDesc->planstate);

  MemoryContextSwitchTo(oldcontext);
}

   
                    
                                                                        
   
                                                                               
                                                                             
   
                                                                               
                                                                          
                                                                          
   
                              
   
bool
ExecCheckRTPerms(List *rangeTable, bool ereport_on_violation)
{
  ListCell *l;
  bool result = true;

  foreach (l, rangeTable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(l);

    result = ExecCheckRTEPerms(rte);
    if (!result)
    {
      Assert(rte->rtekind == RTE_RELATION);
      if (ereport_on_violation)
      {
        aclcheck_error(ACLCHECK_NO_PRIV, get_relkind_objtype(get_rel_relkind(rte->relid)), get_rel_name(rte->relid));
      }
      return false;
    }
  }

  if (ExecutorCheckPerms_hook)
  {
    result = (*ExecutorCheckPerms_hook)(rangeTable, ereport_on_violation);
  }
  return result;
}

   
                     
                                               
   
static bool
ExecCheckRTEPerms(RangeTblEntry *rte)
{
  AclMode requiredPerms;
  AclMode relPerms;
  AclMode remainingPerms;
  Oid relOid;
  Oid userid;

     
                                                                          
                                                                           
                                      
     
  if (rte->rtekind != RTE_RELATION)
  {
    return true;
  }

     
                                        
     
  requiredPerms = rte->requiredPerms;
  if (requiredPerms == 0)
  {
    return true;
  }

  relOid = rte->relid;

     
                                                                          
     
                                                                        
                                                                             
                                                                           
                                                 
     
  userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();

     
                                                                            
                                                                         
                                                                        
     
  relPerms = pg_class_aclmask(relOid, userid, requiredPerms, ACLMASK_ALL);
  remainingPerms = requiredPerms & ~relPerms;
  if (remainingPerms != 0)
  {
    int col = -1;

       
                                                                           
                                  
       
    if (remainingPerms & ~(ACL_SELECT | ACL_INSERT | ACL_UPDATE))
    {
      return false;
    }

       
                                                                      
       
                                                                         
                                                                         
                          
       
    if (remainingPerms & ACL_SELECT)
    {
         
                                                                      
                                                                     
                                                                
         
      if (bms_is_empty(rte->selectedCols))
      {
        if (pg_attribute_aclcheck_all(relOid, userid, ACL_SELECT, ACLMASK_ANY) != ACLCHECK_OK)
        {
          return false;
        }
      }

      while ((col = bms_next_member(rte->selectedCols, col)) >= 0)
      {
                                                                     
        AttrNumber attno = col + FirstLowInvalidHeapAttributeNumber;

        if (attno == InvalidAttrNumber)
        {
                                                               
          if (pg_attribute_aclcheck_all(relOid, userid, ACL_SELECT, ACLMASK_ALL) != ACLCHECK_OK)
          {
            return false;
          }
        }
        else
        {
          if (pg_attribute_aclcheck(relOid, attno, userid, ACL_SELECT) != ACLCHECK_OK)
          {
            return false;
          }
        }
      }
    }

       
                                                                          
                                                 
       
    if (remainingPerms & ACL_INSERT && !ExecCheckRTEPermsModified(relOid, userid, rte->insertedCols, ACL_INSERT))
    {
      return false;
    }

    if (remainingPerms & ACL_UPDATE && !ExecCheckRTEPermsModified(relOid, userid, rte->updatedCols, ACL_UPDATE))
    {
      return false;
    }
  }
  return true;
}

   
                             
                                                                      
                              
   
static bool
ExecCheckRTEPermsModified(Oid relOid, Oid userid, Bitmapset *modifiedCols, AclMode requiredPerms)
{
  int col = -1;

     
                                                                           
                                                                        
                                                                   
     
  if (bms_is_empty(modifiedCols))
  {
    if (pg_attribute_aclcheck_all(relOid, userid, requiredPerms, ACLMASK_ANY) != ACLCHECK_OK)
    {
      return false;
    }
  }

  while ((col = bms_next_member(modifiedCols, col)) >= 0)
  {
                                                                 
    AttrNumber attno = col + FirstLowInvalidHeapAttributeNumber;

    if (attno == InvalidAttrNumber)
    {
                                                 
      elog(ERROR, "whole-row update is not implemented");
    }
    else
    {
      if (pg_attribute_aclcheck(relOid, attno, userid, requiredPerms) != ACLCHECK_OK)
      {
        return false;
      }
    }
  }
  return true;
}

   
                                                                      
                                                                        
                   
   
                                                                   
                                                                               
                                                                 
   
static void
ExecCheckXactReadOnly(PlannedStmt *plannedstmt)
{
  ListCell *l;

     
                                                                        
                                                                
     
  foreach (l, plannedstmt->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(l);

    if (rte->rtekind != RTE_RELATION)
    {
      continue;
    }

    if ((rte->requiredPerms & (~ACL_SELECT)) == 0)
    {
      continue;
    }

    if (isTempNamespace(get_rel_namespace(rte->relid)))
    {
      continue;
    }

    PreventCommandIfReadOnly(CreateCommandTag((Node *)plannedstmt));
  }

  if (plannedstmt->commandType != CMD_SELECT || plannedstmt->hasModifyingCTE)
  {
    PreventCommandIfParallelMode(CreateCommandTag((Node *)plannedstmt));
  }
}

                                                                    
             
   
                                                             
                                  
                                                                    
   
static void
InitPlan(QueryDesc *queryDesc, int eflags)
{
  CmdType operation = queryDesc->operation;
  PlannedStmt *plannedstmt = queryDesc->plannedstmt;
  Plan *plan = plannedstmt->planTree;
  List *rangeTable = plannedstmt->rtable;
  EState *estate = queryDesc->estate;
  PlanState *planstate;
  TupleDesc tupType;
  ListCell *l;
  int i;

     
                           
     
  ExecCheckRTPerms(rangeTable, true);

     
                                           
     
  ExecInitRangeTable(estate, rangeTable);

  estate->es_plannedstmt = plannedstmt;

     
                                                                         
     
  if (plannedstmt->resultRelations)
  {
    List *resultRelations = plannedstmt->resultRelations;
    int numResultRelations = list_length(resultRelations);
    ResultRelInfo *resultRelInfos;
    ResultRelInfo *resultRelInfo;

    resultRelInfos = (ResultRelInfo *)palloc(numResultRelations * sizeof(ResultRelInfo));
    resultRelInfo = resultRelInfos;
    foreach (l, resultRelations)
    {
      Index resultRelationIndex = lfirst_int(l);
      Relation resultRelation;

      resultRelation = ExecGetRangeTableRelation(estate, resultRelationIndex);
      InitResultRelInfo(resultRelInfo, resultRelation, resultRelationIndex, NULL, estate->es_instrument);
      resultRelInfo++;
    }
    estate->es_result_relations = resultRelInfos;
    estate->es_num_result_relations = numResultRelations;

                                                                        
    estate->es_result_relation_info = NULL;

       
                                                                          
                                                                         
                                              
       
    if (plannedstmt->rootResultRelations)
    {
      int num_roots = list_length(plannedstmt->rootResultRelations);

      resultRelInfos = (ResultRelInfo *)palloc(num_roots * sizeof(ResultRelInfo));
      resultRelInfo = resultRelInfos;
      foreach (l, plannedstmt->rootResultRelations)
      {
        Index resultRelIndex = lfirst_int(l);
        Relation resultRelDesc;

        resultRelDesc = ExecGetRangeTableRelation(estate, resultRelIndex);
        InitResultRelInfo(resultRelInfo, resultRelDesc, resultRelIndex, NULL, estate->es_instrument);
        resultRelInfo++;
      }

      estate->es_root_result_relations = resultRelInfos;
      estate->es_num_root_result_relations = num_roots;
    }
    else
    {
      estate->es_root_result_relations = NULL;
      estate->es_num_root_result_relations = 0;
    }
  }
  else
  {
       
                                                           
       
    estate->es_result_relations = NULL;
    estate->es_num_result_relations = 0;
    estate->es_result_relation_info = NULL;
    estate->es_root_result_relations = NULL;
    estate->es_num_root_result_relations = 0;
  }

     
                                                                        
     
  if (plannedstmt->rowMarks)
  {
    estate->es_rowmarks = (ExecRowMark **)palloc0(estate->es_range_table_size * sizeof(ExecRowMark *));
    foreach (l, plannedstmt->rowMarks)
    {
      PlanRowMark *rc = (PlanRowMark *)lfirst(l);
      Oid relid;
      Relation relation;
      ExecRowMark *erm;

                                                                    
      if (rc->isParent)
      {
        continue;
      }

                                                                    
      relid = exec_rt_fetch(rc->rti, estate)->relid;

                                                                     
      switch (rc->markType)
      {
      case ROW_MARK_EXCLUSIVE:
      case ROW_MARK_NOKEYEXCLUSIVE:
      case ROW_MARK_SHARE:
      case ROW_MARK_KEYSHARE:
      case ROW_MARK_REFERENCE:
        relation = ExecGetRangeTableRelation(estate, rc->rti);
        break;
      case ROW_MARK_COPY:
                                                  
        relation = NULL;
        break;
      default:
        elog(ERROR, "unrecognized markType: %d", rc->markType);
        relation = NULL;                          
        break;
      }

                                                             
      if (relation)
      {
        CheckValidRowMarkRel(relation, rc->markType);
      }

      erm = (ExecRowMark *)palloc(sizeof(ExecRowMark));
      erm->relation = relation;
      erm->relid = relid;
      erm->rti = rc->rti;
      erm->prti = rc->prti;
      erm->rowmarkId = rc->rowmarkId;
      erm->markType = rc->markType;
      erm->strength = rc->strength;
      erm->waitPolicy = rc->waitPolicy;
      erm->ermActive = false;
      ItemPointerSetInvalid(&(erm->curCtid));
      erm->ermExtra = NULL;

      Assert(erm->rti > 0 && erm->rti <= estate->es_range_table_size && estate->es_rowmarks[erm->rti - 1] == NULL);

      estate->es_rowmarks[erm->rti - 1] = erm;
    }
  }

     
                                                     
     
  estate->es_tupleTable = NIL;

                                                   
  estate->es_epq_active = NULL;

     
                                                                             
                                                               
                                                               
     
  Assert(estate->es_subplanstates == NIL);
  i = 1;                                   
  foreach (l, plannedstmt->subplans)
  {
    Plan *subplan = (Plan *)lfirst(l);
    PlanState *subplanstate;
    int sp_eflags;

       
                                                                          
                                                                           
                                                                          
       
    sp_eflags = eflags & (EXEC_FLAG_EXPLAIN_ONLY | EXEC_FLAG_WITH_NO_DATA);
    if (bms_is_member(i, plannedstmt->rewindPlanIDs))
    {
      sp_eflags |= EXEC_FLAG_REWIND;
    }

    subplanstate = ExecInitNode(subplan, estate, sp_eflags);

    estate->es_subplanstates = lappend(estate->es_subplanstates, subplanstate);

    i++;
  }

     
                                                                             
                                                                             
                        
     
  planstate = ExecInitNode(plan, estate, eflags);

     
                                                                       
     
  tupType = ExecGetResultType(planstate);

     
                                                                            
                                                      
     
  if (operation == CMD_SELECT)
  {
    bool junk_filter_needed = false;
    ListCell *tlist;

    foreach (tlist, plan->targetlist)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(tlist);

      if (tle->resjunk)
      {
        junk_filter_needed = true;
        break;
      }
    }

    if (junk_filter_needed)
    {
      JunkFilter *j;
      TupleTableSlot *slot;

      slot = ExecInitExtraTupleSlot(estate, NULL, &TTSOpsVirtual);
      j = ExecInitJunkFilter(planstate->plan->targetlist, slot);
      estate->es_junkFilter = j;

                                                 
      tupType = j->jf_cleanTupType;
    }
  }

  queryDesc->tupDesc = tupType;
  queryDesc->planstate = planstate;
}

   
                                                                             
   
                                                                            
                                 
   
                                                                        
                         
   
void
CheckValidResultRel(ResultRelInfo *resultRelInfo, CmdType operation)
{
  Relation resultRel = resultRelInfo->ri_RelationDesc;
  TriggerDesc *trigDesc = resultRel->trigdesc;
  FdwRoutine *fdwroutine;

  switch (resultRel->rd_rel->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_PARTITIONED_TABLE:
    CheckCmdReplicaIdentity(resultRel, operation);
    break;
  case RELKIND_SEQUENCE:
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change sequence \"%s\"", RelationGetRelationName(resultRel))));
    break;
  case RELKIND_TOASTVALUE:
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change TOAST relation \"%s\"", RelationGetRelationName(resultRel))));
    break;
  case RELKIND_VIEW:

       
                                                                     
                                                                  
                                                                      
                                                                     
                                                               
       
    switch (operation)
    {
    case CMD_INSERT:
      if (!trigDesc || !trigDesc->trig_insert_instead_row)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot insert into view \"%s\"", RelationGetRelationName(resultRel)), errhint("To enable inserting into the view, provide an INSTEAD OF INSERT trigger or an unconditional ON INSERT DO INSTEAD rule.")));
      }
      break;
    case CMD_UPDATE:
      if (!trigDesc || !trigDesc->trig_update_instead_row)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot update view \"%s\"", RelationGetRelationName(resultRel)), errhint("To enable updating the view, provide an INSTEAD OF UPDATE trigger or an unconditional ON UPDATE DO INSTEAD rule.")));
      }
      break;
    case CMD_DELETE:
      if (!trigDesc || !trigDesc->trig_delete_instead_row)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot delete from view \"%s\"", RelationGetRelationName(resultRel)), errhint("To enable deleting from the view, provide an INSTEAD OF DELETE trigger or an unconditional ON DELETE DO INSTEAD rule.")));
      }
      break;
    default:
      elog(ERROR, "unrecognized CmdType: %d", (int)operation);
      break;
    }
    break;
  case RELKIND_MATVIEW:
    if (!MatViewIncrementalMaintenanceIsEnabled())
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change materialized view \"%s\"", RelationGetRelationName(resultRel))));
    }
    break;
  case RELKIND_FOREIGN_TABLE:
                                          
    fdwroutine = resultRelInfo->ri_FdwRoutine;
    switch (operation)
    {
    case CMD_INSERT:
      if (fdwroutine->ExecForeignInsert == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot insert into foreign table \"%s\"", RelationGetRelationName(resultRel))));
      }
      if (fdwroutine->IsForeignRelUpdatable != NULL && (fdwroutine->IsForeignRelUpdatable(resultRel) & (1 << CMD_INSERT)) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("foreign table \"%s\" does not allow inserts", RelationGetRelationName(resultRel))));
      }
      break;
    case CMD_UPDATE:
      if (fdwroutine->ExecForeignUpdate == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot update foreign table \"%s\"", RelationGetRelationName(resultRel))));
      }
      if (fdwroutine->IsForeignRelUpdatable != NULL && (fdwroutine->IsForeignRelUpdatable(resultRel) & (1 << CMD_UPDATE)) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("foreign table \"%s\" does not allow updates", RelationGetRelationName(resultRel))));
      }
      break;
    case CMD_DELETE:
      if (fdwroutine->ExecForeignDelete == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot delete from foreign table \"%s\"", RelationGetRelationName(resultRel))));
      }
      if (fdwroutine->IsForeignRelUpdatable != NULL && (fdwroutine->IsForeignRelUpdatable(resultRel) & (1 << CMD_DELETE)) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("foreign table \"%s\" does not allow deletes", RelationGetRelationName(resultRel))));
      }
      break;
    default:
      elog(ERROR, "unrecognized CmdType: %d", (int)operation);
      break;
    }
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot change relation \"%s\"", RelationGetRelationName(resultRel))));
    break;
  }
}

   
                                                                   
   
                                                                             
                               
   
static void
CheckValidRowMarkRel(Relation rel, RowMarkType markType)
{
  FdwRoutine *fdwroutine;

  switch (rel->rd_rel->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_PARTITIONED_TABLE:
            
    break;
  case RELKIND_SEQUENCE:
                                                              
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot lock rows in sequence \"%s\"", RelationGetRelationName(rel))));
    break;
  case RELKIND_TOASTVALUE:
                                                                
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot lock rows in TOAST relation \"%s\"", RelationGetRelationName(rel))));
    break;
  case RELKIND_VIEW:
                                                                    
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot lock rows in view \"%s\"", RelationGetRelationName(rel))));
    break;
  case RELKIND_MATVIEW:
                                                                     
    if (markType != ROW_MARK_REFERENCE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot lock rows in materialized view \"%s\"", RelationGetRelationName(rel))));
    }
    break;
  case RELKIND_FOREIGN_TABLE:
                                          
    fdwroutine = GetFdwRoutineForRelation(rel, false);
    if (fdwroutine->RefetchForeignRow == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot lock rows in foreign table \"%s\"", RelationGetRelationName(rel))));
    }
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot lock rows in relation \"%s\"", RelationGetRelationName(rel))));
    break;
  }
}

   
                                                         
   
                                                                             
                                                                         
                                                    
   
void
InitResultRelInfo(ResultRelInfo *resultRelInfo, Relation resultRelationDesc, Index resultRelationIndex, ResultRelInfo *partition_root_rri, int instrument_options)
{
  List *partition_check = NIL;

  MemSet(resultRelInfo, 0, sizeof(ResultRelInfo));
  resultRelInfo->type = T_ResultRelInfo;
  resultRelInfo->ri_RangeTableIndex = resultRelationIndex;
  resultRelInfo->ri_RelationDesc = resultRelationDesc;
  resultRelInfo->ri_NumIndices = 0;
  resultRelInfo->ri_IndexRelationDescs = NULL;
  resultRelInfo->ri_IndexRelationInfo = NULL;
                                                                        
  resultRelInfo->ri_TrigDesc = CopyTriggerDesc(resultRelationDesc->trigdesc);
  if (resultRelInfo->ri_TrigDesc)
  {
    int n = resultRelInfo->ri_TrigDesc->numtriggers;

    resultRelInfo->ri_TrigFunctions = (FmgrInfo *)palloc0(n * sizeof(FmgrInfo));
    resultRelInfo->ri_TrigWhenExprs = (ExprState **)palloc0(n * sizeof(ExprState *));
    if (instrument_options)
    {
      resultRelInfo->ri_TrigInstrument = InstrAlloc(n, instrument_options);
    }
  }
  else
  {
    resultRelInfo->ri_TrigFunctions = NULL;
    resultRelInfo->ri_TrigWhenExprs = NULL;
    resultRelInfo->ri_TrigInstrument = NULL;
  }
  if (resultRelationDesc->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
    resultRelInfo->ri_FdwRoutine = GetFdwRoutineForRelation(resultRelationDesc, true);
  }
  else
  {
    resultRelInfo->ri_FdwRoutine = NULL;
  }

                                                    
  resultRelInfo->ri_FdwState = NULL;
  resultRelInfo->ri_usesFdwDirectModify = false;
  resultRelInfo->ri_ConstraintExprs = NULL;
  resultRelInfo->ri_GeneratedExprs = NULL;
  resultRelInfo->ri_junkFilter = NULL;
  resultRelInfo->ri_projectReturning = NULL;
  resultRelInfo->ri_onConflictArbiterIndexes = NIL;
  resultRelInfo->ri_onConflict = NULL;
  resultRelInfo->ri_ReturningSlot = NULL;
  resultRelInfo->ri_TrigOldSlot = NULL;
  resultRelInfo->ri_TrigNewSlot = NULL;

     
                                                                           
                                                                          
                                                                           
                                                                         
                                                                        
                                                                          
                                                                             
                                
     
                                                                            
                                                                     
     
  partition_check = RelationGetPartitionQual(resultRelationDesc);

  resultRelInfo->ri_PartitionCheck = partition_check;
  resultRelInfo->ri_RootResultRelInfo = partition_root_rri;
  resultRelInfo->ri_PartitionInfo = NULL;                       
  resultRelInfo->ri_CopyMultiInsertBuffer = NULL;
}

   
                           
                                                       
   
                                                                              
                                                                               
                                                          
                                                                         
                                                                          
                                      
   
                                                                           
                                                                            
                                                                           
                                                                            
                                                                          
                                                                          
                                                                              
                                
   
ResultRelInfo *
ExecGetTriggerResultRel(EState *estate, Oid relid)
{
  ResultRelInfo *rInfo;
  int nr;
  ListCell *l;
  Relation rel;
  MemoryContext oldcontext;

                                                        
  rInfo = estate->es_result_relations;
  nr = estate->es_num_result_relations;
  while (nr > 0)
  {
    if (RelationGetRelid(rInfo->ri_RelationDesc) == relid)
    {
      return rInfo;
    }
    rInfo++;
    nr--;
  }
                                                                
  rInfo = estate->es_root_result_relations;
  nr = estate->es_num_root_result_relations;
  while (nr > 0)
  {
    if (RelationGetRelid(rInfo->ri_RelationDesc) == relid)
    {
      return rInfo;
    }
    rInfo++;
    nr--;
  }

     
                                                                         
                            
     
  foreach (l, estate->es_tuple_routing_result_relations)
  {
    rInfo = (ResultRelInfo *)lfirst(l);
    if (RelationGetRelid(rInfo->ri_RelationDesc) == relid)
    {
      return rInfo;
    }
  }

                                                                     
  foreach (l, estate->es_trig_target_relations)
  {
    rInfo = (ResultRelInfo *)lfirst(l);
    if (RelationGetRelid(rInfo->ri_RelationDesc) == relid)
    {
      return rInfo;
    }
  }
                                  

     
                                                                   
                                                                             
                                                                            
                                                              
     
  rel = table_open(relid, NoLock);

     
                                              
     
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);
  rInfo = makeNode(ResultRelInfo);
  InitResultRelInfo(rInfo, rel, 0,                             
      NULL, estate->es_instrument);
  estate->es_trig_target_relations = lappend(estate->es_trig_target_relations, rInfo);
  MemoryContextSwitchTo(oldcontext);

     
                                                                           
                                                            
     

  return rInfo;
}

   
                                                                           
   
void
ExecCleanUpTriggerState(EState *estate)
{
  ListCell *l;

  foreach (l, estate->es_trig_target_relations)
  {
    ResultRelInfo *resultRelInfo = (ResultRelInfo *)lfirst(l);

       
                                                                        
                                                                       
                                  
       
    Assert(resultRelInfo->ri_RangeTableIndex == 0);

       
                                                                      
                                                            
       
    Assert(resultRelInfo->ri_NumIndices == 0);

    table_close(resultRelInfo->ri_RelationDesc, NoLock);
  }
}

                                                                    
                        
   
                                                              
                                                                    
   
static void
ExecPostprocessPlan(EState *estate)
{
  ListCell *lc;

     
                                  
     
  estate->es_direction = ForwardScanDirection;

     
                                                                         
                                                                         
                                           
     
  foreach (lc, estate->es_auxmodifytables)
  {
    PlanState *ps = (PlanState *)lfirst(lc);

    for (;;)
    {
      TupleTableSlot *slot;

                                                            
      ResetPerTupleExprContext(estate);

      slot = ExecProcNode(ps);

      if (TupIsNull(slot))
      {
        break;
      }
    }
  }
}

                                                                    
                
   
                                                                  
   
                                                                    
                                                                       
                                                                      
                                                                      
                                                                        
                                                                    
   
static void
ExecEndPlan(PlanState *planstate, EState *estate)
{
  ResultRelInfo *resultRelInfo;
  Index num_relations;
  Index i;
  ListCell *l;

     
                                                       
     
  ExecEndNode(planstate);

     
                      
     
  foreach (l, estate->es_subplanstates)
  {
    PlanState *subplanstate = (PlanState *)lfirst(l);

    ExecEndNode(subplanstate);
  }

     
                                                                      
                                                                           
                                                                             
                  
     
  ExecResetTupleTable(estate->es_tupleTable, false);

     
                                                                       
                   
     
  resultRelInfo = estate->es_result_relations;
  for (i = estate->es_num_result_relations; i > 0; i--)
  {
    ExecCloseIndices(resultRelInfo);
    resultRelInfo++;
  }

     
                                                                      
                                                    
     
  num_relations = estate->es_range_table_size;
  for (i = 0; i < num_relations; i++)
  {
    if (estate->es_relations[i])
    {
      table_close(estate->es_relations[i], NoLock);
    }
  }

                                                   
  ExecCleanUpTriggerState(estate);
}

                                                                    
                
   
                                                                            
                                       
   
                                            
   
                                                                             
                   
                                                                    
   
static void
ExecutePlan(EState *estate, PlanState *planstate, bool use_parallel_mode, CmdType operation, bool sendTuples, uint64 numberTuples, ScanDirection direction, DestReceiver *dest, bool execute_once)
{
  TupleTableSlot *slot;
  uint64 current_tuple_count;

     
                                
     
  current_tuple_count = 0;

     
                        
     
  estate->es_direction = direction;

     
                                                                             
                                                                 
     
  if (!execute_once)
  {
    use_parallel_mode = false;
  }

  estate->es_use_parallel_mode = use_parallel_mode;
  if (use_parallel_mode)
  {
    EnterParallelMode();
  }

     
                                                                           
     
  for (;;)
  {
                                                
    ResetPerTupleExprContext(estate);

       
                                           
       
    slot = ExecProcNode(planstate);

       
                                                                     
                                          
       
    if (TupIsNull(slot))
    {
      break;
    }

       
                                                                        
                
       
                                                                    
                                                                           
                                                          
       
    if (estate->es_junkFilter != NULL)
    {
      slot = ExecFilterJunk(estate->es_junkFilter, slot);
    }

       
                                                                  
                                                                  
       
    if (sendTuples)
    {
         
                                                                         
                                                                        
                       
         
      if (!dest->receiveSlot(slot, dest))
      {
        break;
      }
    }

       
                                                                          
                                                                   
                
       
    if (operation == CMD_SELECT)
    {
      (estate->es_processed)++;
    }

       
                                                                         
                                                                         
                       
       
    current_tuple_count++;
    if (numberTuples && numberTuples == current_tuple_count)
    {
      break;
    }
  }

     
                                                                           
            
     
  if (!(estate->es_top_eflags & EXEC_FLAG_BACKWARD))
  {
    (void)ExecShutdownNode(planstate);
  }

  if (use_parallel_mode)
  {
    ExitParallelMode();
  }
}

   
                                                                           
   
                                                            
   
static const char *
ExecRelCheck(ResultRelInfo *resultRelInfo, TupleTableSlot *slot, EState *estate)
{
  Relation rel = resultRelInfo->ri_RelationDesc;
  int ncheck = rel->rd_att->constr->num_check;
  ConstrCheck *check = rel->rd_att->constr->check;
  ExprContext *econtext;
  MemoryContext oldContext;
  int i;

     
                                                                      
                                                                             
                                                             
     
  if (resultRelInfo->ri_ConstraintExprs == NULL)
  {
    oldContext = MemoryContextSwitchTo(estate->es_query_cxt);
    resultRelInfo->ri_ConstraintExprs = (ExprState **)palloc(ncheck * sizeof(ExprState *));
    for (i = 0; i < ncheck; i++)
    {
      Expr *checkconstr;

      checkconstr = stringToNode(check[i].ccbin);
      resultRelInfo->ri_ConstraintExprs[i] = ExecPrepareExpr(checkconstr, estate);
    }
    MemoryContextSwitchTo(oldContext);
  }

     
                                                                          
                                                          
     
  econtext = GetPerTupleExprContext(estate);

                                                                    
  econtext->ecxt_scantuple = slot;

                                    
  for (i = 0; i < ncheck; i++)
  {
    ExprState *checkconstr = resultRelInfo->ri_ConstraintExprs[i];

       
                                                                           
                                                                        
                 
       
    if (!ExecCheck(checkconstr, econtext))
    {
      return check[i].ccname;
    }
  }

                                  
  return NULL;
}

   
                                                                           
   
                                                                         
                                                                             
                 
   
bool
ExecPartitionCheck(ResultRelInfo *resultRelInfo, TupleTableSlot *slot, EState *estate, bool emitError)
{
  ExprContext *econtext;
  bool success;

     
                                                                          
                                                                           
                                   
     
  if (resultRelInfo->ri_PartitionCheckExpr == NULL)
  {
    List *qual = resultRelInfo->ri_PartitionCheck;

    resultRelInfo->ri_PartitionCheckExpr = ExecPrepareCheck(qual, estate);
  }

     
                                                                          
                                                          
     
  econtext = GetPerTupleExprContext(estate);

                                                                    
  econtext->ecxt_scantuple = slot;

     
                                                                         
                                  
     
  success = ExecCheck(resultRelInfo->ri_PartitionCheckExpr, econtext);

                                                                
  if (!success && emitError)
  {
    ExecPartitionCheckEmitError(resultRelInfo, slot, estate);
  }

  return success;
}

   
                                                                               
                               
   
void
ExecPartitionCheckEmitError(ResultRelInfo *resultRelInfo, TupleTableSlot *slot, EState *estate)
{
  Oid root_relid;
  TupleDesc tupdesc;
  char *val_desc;
  Bitmapset *modifiedCols;

     
                                                                          
                                                                            
                                                                            
                              
     
  if (resultRelInfo->ri_RootResultRelInfo)
  {
    ResultRelInfo *rootrel = resultRelInfo->ri_RootResultRelInfo;
    TupleDesc old_tupdesc;
    AttrNumber *map;

    root_relid = RelationGetRelid(rootrel->ri_RelationDesc);
    tupdesc = RelationGetDescr(rootrel->ri_RelationDesc);

    old_tupdesc = RelationGetDescr(resultRelInfo->ri_RelationDesc);
                       
    map = convert_tuples_by_name_map_if_req(old_tupdesc, tupdesc, gettext_noop("could not convert row type"));

       
                                                                         
                
       
    if (map != NULL)
    {
      slot = execute_attr_map_slot(map, slot, MakeTupleTableSlot(tupdesc, &TTSOpsVirtual));
    }
    modifiedCols = bms_union(ExecGetInsertedCols(rootrel, estate), ExecGetUpdatedCols(rootrel, estate));
  }
  else
  {
    root_relid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
    tupdesc = RelationGetDescr(resultRelInfo->ri_RelationDesc);
    modifiedCols = bms_union(ExecGetInsertedCols(resultRelInfo, estate), ExecGetUpdatedCols(resultRelInfo, estate));
  }

  val_desc = ExecBuildSlotValueDescription(root_relid, slot, tupdesc, modifiedCols, 64);
  ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("new row for relation \"%s\" violates partition constraint", RelationGetRelationName(resultRelInfo->ri_RelationDesc)), val_desc ? errdetail("Failing row contains %s.", val_desc) : 0));
}

   
                                                              
   
                                                               
   
                                              
   
                                                                          
                                                                          
                                                                      
   
void
ExecConstraints(ResultRelInfo *resultRelInfo, TupleTableSlot *slot, EState *estate)
{
  Relation rel = resultRelInfo->ri_RelationDesc;
  TupleDesc tupdesc = RelationGetDescr(rel);
  TupleConstr *constr = tupdesc->constr;
  Bitmapset *modifiedCols;

  Assert(constr || resultRelInfo->ri_PartitionCheck);

  if (constr && constr->has_not_null)
  {
    int natts = tupdesc->natts;
    int attrChk;

    for (attrChk = 1; attrChk <= natts; attrChk++)
    {
      Form_pg_attribute att = TupleDescAttr(tupdesc, attrChk - 1);

      if (att->attnotnull && slot_attisnull(slot, attrChk))
      {
        char *val_desc;
        Relation orig_rel = rel;
        TupleDesc orig_tupdesc = RelationGetDescr(rel);

           
                                                                    
                                                                 
                                                                 
                                                                    
                        
           
        if (resultRelInfo->ri_RootResultRelInfo)
        {
          ResultRelInfo *rootrel = resultRelInfo->ri_RootResultRelInfo;
          AttrNumber *map;

          tupdesc = RelationGetDescr(rootrel->ri_RelationDesc);
                             
          map = convert_tuples_by_name_map_if_req(orig_tupdesc, tupdesc, gettext_noop("could not convert row type"));

             
                                                                    
                                 
             
          if (map != NULL)
          {
            slot = execute_attr_map_slot(map, slot, MakeTupleTableSlot(tupdesc, &TTSOpsVirtual));
          }
          modifiedCols = bms_union(ExecGetInsertedCols(rootrel, estate), ExecGetUpdatedCols(rootrel, estate));
          rel = rootrel->ri_RelationDesc;
        }
        else
        {
          modifiedCols = bms_union(ExecGetInsertedCols(resultRelInfo, estate), ExecGetUpdatedCols(resultRelInfo, estate));
        }
        val_desc = ExecBuildSlotValueDescription(RelationGetRelid(rel), slot, tupdesc, modifiedCols, 64);

        ereport(ERROR, (errcode(ERRCODE_NOT_NULL_VIOLATION), errmsg("null value in column \"%s\" violates not-null constraint", NameStr(att->attname)), val_desc ? errdetail("Failing row contains %s.", val_desc) : 0, errtablecol(orig_rel, attrChk)));
      }
    }
  }

  if (constr && constr->num_check > 0)
  {
    const char *failed;

    if ((failed = ExecRelCheck(resultRelInfo, slot, estate)) != NULL)
    {
      char *val_desc;
      Relation orig_rel = rel;

                                  
      if (resultRelInfo->ri_RootResultRelInfo)
      {
        ResultRelInfo *rootrel = resultRelInfo->ri_RootResultRelInfo;
        TupleDesc old_tupdesc = RelationGetDescr(rel);
        AttrNumber *map;

        tupdesc = RelationGetDescr(rootrel->ri_RelationDesc);
                           
        map = convert_tuples_by_name_map_if_req(old_tupdesc, tupdesc, gettext_noop("could not convert row type"));

           
                                                                  
                               
           
        if (map != NULL)
        {
          slot = execute_attr_map_slot(map, slot, MakeTupleTableSlot(tupdesc, &TTSOpsVirtual));
        }
        modifiedCols = bms_union(ExecGetInsertedCols(rootrel, estate), ExecGetUpdatedCols(rootrel, estate));
        rel = rootrel->ri_RelationDesc;
      }
      else
      {
        modifiedCols = bms_union(ExecGetInsertedCols(resultRelInfo, estate), ExecGetUpdatedCols(resultRelInfo, estate));
      }
      val_desc = ExecBuildSlotValueDescription(RelationGetRelid(rel), slot, tupdesc, modifiedCols, 64);
      ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("new row for relation \"%s\" violates check constraint \"%s\"", RelationGetRelationName(orig_rel), failed), val_desc ? errdetail("Failing row contains %s.", val_desc) : 0, errtableconstraint(orig_rel, failed)));
    }
  }
}

   
                                                                             
                          
   
                                                                                
                                                                             
                                                                             
                     
   
void
ExecWithCheckOptions(WCOKind kind, ResultRelInfo *resultRelInfo, TupleTableSlot *slot, EState *estate)
{
  Relation rel = resultRelInfo->ri_RelationDesc;
  TupleDesc tupdesc = RelationGetDescr(rel);
  ExprContext *econtext;
  ListCell *l1, *l2;

     
                                                                          
                                                          
     
  econtext = GetPerTupleExprContext(estate);

                                                                    
  econtext->ecxt_scantuple = slot;

                                     
  forboth(l1, resultRelInfo->ri_WithCheckOptions, l2, resultRelInfo->ri_WithCheckOptionExprs)
  {
    WithCheckOption *wco = (WithCheckOption *)lfirst(l1);
    ExprState *wcoExpr = (ExprState *)lfirst(l2);

       
                                                                       
             
       
    if (wco->kind != kind)
    {
      continue;
    }

       
                                                                          
                                                                
                                                                      
                                                                           
                                                                       
       
    if (!ExecQual(wcoExpr, econtext))
    {
      char *val_desc;
      Bitmapset *modifiedCols;

      switch (wco->kind)
      {
           
                                                                 
                                                                
                                                                 
                                                                   
                                                                  
                                                                   
                         
           
      case WCO_VIEW_CHECK:
                                                   
        if (resultRelInfo->ri_RootResultRelInfo)
        {
          ResultRelInfo *rootrel = resultRelInfo->ri_RootResultRelInfo;
          TupleDesc old_tupdesc = RelationGetDescr(rel);
          AttrNumber *map;

          tupdesc = RelationGetDescr(rootrel->ri_RelationDesc);
                             
          map = convert_tuples_by_name_map_if_req(old_tupdesc, tupdesc, gettext_noop("could not convert row type"));

             
                                                                 
                                    
             
          if (map != NULL)
          {
            slot = execute_attr_map_slot(map, slot, MakeTupleTableSlot(tupdesc, &TTSOpsVirtual));
          }

          modifiedCols = bms_union(ExecGetInsertedCols(rootrel, estate), ExecGetUpdatedCols(rootrel, estate));
          rel = rootrel->ri_RelationDesc;
        }
        else
        {
          modifiedCols = bms_union(ExecGetInsertedCols(resultRelInfo, estate), ExecGetUpdatedCols(resultRelInfo, estate));
        }
        val_desc = ExecBuildSlotValueDescription(RelationGetRelid(rel), slot, tupdesc, modifiedCols, 64);

        ereport(ERROR, (errcode(ERRCODE_WITH_CHECK_OPTION_VIOLATION), errmsg("new row violates check option for view \"%s\"", wco->relname), val_desc ? errdetail("Failing row contains %s.", val_desc) : 0));
        break;
      case WCO_RLS_INSERT_CHECK:
      case WCO_RLS_UPDATE_CHECK:
        if (wco->polname != NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("new row violates row-level security policy \"%s\" for table \"%s\"", wco->polname, wco->relname)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("new row violates row-level security policy for table \"%s\"", wco->relname)));
        }
        break;
      case WCO_RLS_CONFLICT_CHECK:
        if (wco->polname != NULL)
        {
          ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("new row violates row-level security policy \"%s\" (USING expression) for table \"%s\"", wco->polname, wco->relname)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("new row violates row-level security policy (USING expression) for table \"%s\"", wco->relname)));
        }
        break;
      default:
        elog(ERROR, "unrecognized WCO kind: %u", wco->kind);
        break;
      }
    }
  }
}

   
                                                                            
   
                                                                         
                                                                               
                                                                            
                                                         
   
                                                                              
                                                                              
                                                                           
                                                    
   
                                                                         
                                                                               
                                                                              
                                                                              
                     
   
static char *
ExecBuildSlotValueDescription(Oid reloid, TupleTableSlot *slot, TupleDesc tupdesc, Bitmapset *modifiedCols, int maxfieldlen)
{
  StringInfoData buf;
  StringInfoData collist;
  bool write_comma = false;
  bool write_comma_collist = false;
  int i;
  AclResult aclresult;
  bool table_perm = false;
  bool any_perm = false;

     
                                                                           
                                                                          
             
     
  if (check_enable_rls(reloid, InvalidOid, true) == RLS_ENABLED)
  {
    return NULL;
  }

  initStringInfo(&buf);

  appendStringInfoChar(&buf, '(');

     
                                                                           
                                                                          
                                                                            
                                                                           
               
     
  aclresult = pg_class_aclcheck(reloid, GetUserId(), ACL_SELECT);
  if (aclresult != ACLCHECK_OK)
  {
                                               
    initStringInfo(&collist);
    appendStringInfoChar(&collist, '(');
  }
  else
  {
    table_perm = any_perm = true;
  }

                                                  
  slot_getallattrs(slot);

  for (i = 0; i < tupdesc->natts; i++)
  {
    bool column_perm = false;
    char *val;
    int vallen;
    Form_pg_attribute att = TupleDescAttr(tupdesc, i);

                                
    if (att->attisdropped)
    {
      continue;
    }

    if (!table_perm)
    {
         
                                                                      
                                                                         
                                                                  
                  
         
      aclresult = pg_attribute_aclcheck(reloid, att->attnum, GetUserId(), ACL_SELECT);
      if (bms_is_member(att->attnum - FirstLowInvalidHeapAttributeNumber, modifiedCols) || aclresult == ACLCHECK_OK)
      {
        column_perm = any_perm = true;

        if (write_comma_collist)
        {
          appendStringInfoString(&collist, ", ");
        }
        else
        {
          write_comma_collist = true;
        }

        appendStringInfoString(&collist, NameStr(att->attname));
      }
    }

    if (table_perm || column_perm)
    {
      if (slot->tts_isnull[i])
      {
        val = "null";
      }
      else
      {
        Oid foutoid;
        bool typisvarlena;

        getTypeOutputInfo(att->atttypid, &foutoid, &typisvarlena);
        val = OidOutputFunctionCall(foutoid, slot->tts_values[i]);
      }

      if (write_comma)
      {
        appendStringInfoString(&buf, ", ");
      }
      else
      {
        write_comma = true;
      }

                              
      vallen = strlen(val);
      if (vallen <= maxfieldlen)
      {
        appendStringInfoString(&buf, val);
      }
      else
      {
        vallen = pg_mbcliplen(val, vallen, maxfieldlen);
        appendBinaryStringInfo(&buf, val, vallen);
        appendStringInfoString(&buf, "...");
      }
    }
  }

                                                                        
  if (!any_perm)
  {
    return NULL;
  }

  appendStringInfoChar(&buf, ')');

  if (!table_perm)
  {
    appendStringInfoString(&collist, ") = ");
    appendStringInfoString(&collist, buf.data);

    return collist.data;
  }

  return buf.data;
}

   
                                                                           
                       
   
LockTupleMode
ExecUpdateLockMode(EState *estate, ResultRelInfo *relinfo)
{
  Bitmapset *keyCols;
  Bitmapset *updatedCols;

     
                                                                             
                                                                       
                  
     
  updatedCols = ExecGetAllUpdatedCols(relinfo, estate);
  keyCols = RelationGetIndexAttrBitmap(relinfo->ri_RelationDesc, INDEX_ATTR_BITMAP_KEY);

  if (bms_overlap(keyCols, updatedCols))
  {
    return LockTupleExclusive;
  }

  return LockTupleNoKeyExclusive;
}

   
                                                                             
   
                                                                                
   
ExecRowMark *
ExecFindRowMark(EState *estate, Index rti, bool missing_ok)
{
  if (rti > 0 && rti <= estate->es_range_table_size && estate->es_rowmarks != NULL)
  {
    ExecRowMark *erm = estate->es_rowmarks[rti - 1];

    if (erm)
    {
      return erm;
    }
  }
  if (!missing_ok)
  {
    elog(ERROR, "failed to find ExecRowMark for rangetable index %u", rti);
  }
  return NULL;
}

   
                                                          
   
                                                                          
                                                                          
                                              
   
ExecAuxRowMark *
ExecBuildAuxRowMark(ExecRowMark *erm, List *targetlist)
{
  ExecAuxRowMark *aerm = (ExecAuxRowMark *)palloc0(sizeof(ExecAuxRowMark));
  char resname[32];

  aerm->rowmark = erm;

                                                                
  if (erm->markType != ROW_MARK_COPY)
  {
                                                   
    snprintf(resname, sizeof(resname), "ctid%u", erm->rowmarkId);
    aerm->ctidAttNo = ExecFindJunkAttributeInTlist(targetlist, resname);
    if (!AttributeNumberIsValid(aerm->ctidAttNo))
    {
      elog(ERROR, "could not find junk %s column", resname);
    }
  }
  else
  {
                               
    snprintf(resname, sizeof(resname), "wholerow%u", erm->rowmarkId);
    aerm->wholeAttNo = ExecFindJunkAttributeInTlist(targetlist, resname);
    if (!AttributeNumberIsValid(aerm->wholeAttNo))
    {
      elog(ERROR, "could not find junk %s column", resname);
    }
  }

                                   
  if (erm->rti != erm->prti)
  {
    snprintf(resname, sizeof(resname), "tableoid%u", erm->rowmarkId);
    aerm->toidAttNo = ExecFindJunkAttributeInTlist(targetlist, resname);
    if (!AttributeNumberIsValid(aerm->toidAttNo))
    {
      elog(ERROR, "could not find junk %s column", resname);
    }
  }

  return aerm;
}

   
                                                                         
                                                           
   
                                                                   
   

   
                                                                              
                         
   
                                                
                                     
                                                    
                                                                
                                                      
   
                                                                        
                                                                            
                                                                         
                                                                        
                                                 
                                           
   
                                                                       
                                                      
   
TupleTableSlot *
EvalPlanQual(EPQState *epqstate, Relation relation, Index rti, TupleTableSlot *inputslot)
{
  TupleTableSlot *slot;
  TupleTableSlot *testslot;

  Assert(rti > 0);

     
                                                                            
     
  EvalPlanQualBegin(epqstate);

     
                                                                             
                          
     
  testslot = EvalPlanQualSlot(epqstate, relation, rti);
  if (testslot != inputslot)
  {
    ExecCopySlot(testslot, inputslot);
  }

     
                                                                     
     
  slot = EvalPlanQualNext(epqstate);

     
                                                                           
                                                                          
                                                                             
                                                                            
                                                        
     
  if (!TupIsNull(slot))
  {
    ExecMaterializeSlot(slot);
  }

     
                                                                        
                                                                            
                                        
     
  ExecClearTuple(testslot);

  return slot;
}

   
                                                                       
                                             
   
                                                                       
                             
   
void
EvalPlanQualInit(EPQState *epqstate, EState *parentestate, Plan *subplan, List *auxrowmarks, int epqParam)
{
  Index rtsize = parentestate->es_range_table_size;

                                                             
  epqstate->parentestate = parentestate;
  epqstate->epqParam = epqParam;

     
                                                                           
                                                                       
                                                                           
                                                                
                          
     
  epqstate->tuple_table = NIL;
  epqstate->relsubs_slot = (TupleTableSlot **)palloc0(rtsize * sizeof(TupleTableSlot *));

                                                              
  epqstate->plan = subplan;
  epqstate->arowMarks = auxrowmarks;

                                           
  epqstate->origslot = NULL;
  epqstate->recheckestate = NULL;
  epqstate->recheckplanstate = NULL;
  epqstate->relsubs_rowmark = NULL;
  epqstate->relsubs_done = NULL;
}

   
                                                                
   
                                                                     
   
void
EvalPlanQualSetPlan(EPQState *epqstate, Plan *subplan, List *auxrowmarks)
{
                                                 
  EvalPlanQualEnd(epqstate);
                                       
  epqstate->plan = subplan;
                                            
  epqstate->arowMarks = auxrowmarks;
}

   
                                                                  
   
                                                                   
                                         
   
TupleTableSlot *
EvalPlanQualSlot(EPQState *epqstate, Relation relation, Index rti)
{
  TupleTableSlot **slot;

  Assert(relation);
  Assert(rti > 0 && rti <= epqstate->parentestate->es_range_table_size);
  slot = &epqstate->relsubs_slot[rti - 1];

  if (*slot == NULL)
  {
    MemoryContext oldcontext;

    oldcontext = MemoryContextSwitchTo(epqstate->parentestate->es_query_cxt);
    *slot = table_slot_create(relation, &epqstate->tuple_table);
    MemoryContextSwitchTo(oldcontext);
  }

  return *slot;
}

   
                                                                             
                                                                              
                                                                              
                                                                           
   
bool
EvalPlanQualFetchRowMark(EPQState *epqstate, Index rti, TupleTableSlot *slot)
{
  ExecAuxRowMark *earm = epqstate->relsubs_rowmark[rti - 1];
  ExecRowMark *erm = earm->rowmark;
  Datum datum;
  bool isNull;

  Assert(earm != NULL);
  Assert(epqstate->origslot != NULL);

  if (RowMarkRequiresRowShareLock(erm->markType))
  {
    elog(ERROR, "EvalPlanQual doesn't support locking rowmarks");
  }

                                                             
  if (erm->rti != erm->prti)
  {
    Oid tableoid;

    datum = ExecGetJunkAttribute(epqstate->origslot, earm->toidAttNo, &isNull);
                                                               
    if (isNull)
    {
      return false;
    }

    tableoid = DatumGetObjectId(datum);

    Assert(OidIsValid(erm->relid));
    if (tableoid != erm->relid)
    {
                                            
      return false;
    }
  }

  if (erm->markType == ROW_MARK_REFERENCE)
  {
    Assert(erm->relation != NULL);

                                
    datum = ExecGetJunkAttribute(epqstate->origslot, earm->ctidAttNo, &isNull);
                                                               
    if (isNull)
    {
      return false;
    }

                                                                      
    if (erm->relation->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
    {
      FdwRoutine *fdwroutine;
      bool updated = false;

      fdwroutine = GetFdwRoutineForRelation(erm->relation, false);
                                                                    
      if (fdwroutine->RefetchForeignRow == NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot lock rows in foreign table \"%s\"", RelationGetRelationName(erm->relation))));
      }

      fdwroutine->RefetchForeignRow(epqstate->recheckestate, erm, datum, slot, &updated);
      if (TupIsNull(slot))
      {
        elog(ERROR, "failed to fetch tuple for EvalPlanQual recheck");
      }

         
                                                                        
                                                                        
                                       
         
      return true;
    }
    else
    {
                                           
      if (!table_tuple_fetch_row_version(erm->relation, (ItemPointer)DatumGetPointer(datum), SnapshotAny, slot))
      {
        elog(ERROR, "failed to fetch tuple for EvalPlanQual recheck");
      }
      return true;
    }
  }
  else
  {
    Assert(erm->markType == ROW_MARK_COPY);

                                                  
    datum = ExecGetJunkAttribute(epqstate->origslot, earm->wholeAttNo, &isNull);
                                                               
    if (isNull)
    {
      return false;
    }

    ExecStoreHeapTupleDatum(datum, slot);
    return true;
  }
}

   
                                                         
   
                                                             
   
TupleTableSlot *
EvalPlanQualNext(EPQState *epqstate)
{
  MemoryContext oldcontext;
  TupleTableSlot *slot;

  oldcontext = MemoryContextSwitchTo(epqstate->recheckestate->es_query_cxt);
  slot = ExecProcNode(epqstate->recheckplanstate);
  MemoryContextSwitchTo(oldcontext);

  return slot;
}

   
                                                  
   
void
EvalPlanQualBegin(EPQState *epqstate)
{
  EState *parentestate = epqstate->parentestate;
  EState *recheckestate = epqstate->recheckestate;

  if (recheckestate == NULL)
  {
                                                      
    EvalPlanQualStart(epqstate, epqstate->plan);
  }
  else
  {
       
                                                                    
       
    Index rtsize = parentestate->es_range_table_size;
    PlanState *rcplanstate = epqstate->recheckplanstate;

    MemSet(epqstate->relsubs_done, 0, rtsize * sizeof(bool));

                                                    
    if (parentestate->es_plannedstmt->paramExecTypes != NIL)
    {
      int i;

         
                                                                       
                                                           
                                                   
         
      ExecSetParamPlanMulti(rcplanstate->plan->extParam, GetPerTupleExprContext(parentestate));

      i = list_length(parentestate->es_plannedstmt->paramExecTypes);

      while (--i >= 0)
      {
                                                      
        recheckestate->es_param_exec_vals[i].value = parentestate->es_param_exec_vals[i].value;
        recheckestate->es_param_exec_vals[i].isnull = parentestate->es_param_exec_vals[i].isnull;
      }
    }

       
                                                                      
                                                                       
       
    rcplanstate->chgParam = bms_add_member(rcplanstate->chgParam, epqstate->epqParam);
  }
}

   
                                                 
   
                                                                          
                                                           
   
static void
EvalPlanQualStart(EPQState *epqstate, Plan *planTree)
{
  EState *parentestate = epqstate->parentestate;
  Index rtsize = parentestate->es_range_table_size;
  EState *rcestate;
  MemoryContext oldcontext;
  ListCell *l;

  epqstate->recheckestate = rcestate = CreateExecutorState();

  oldcontext = MemoryContextSwitchTo(rcestate->es_query_cxt);

                                                       
  rcestate->es_epq_active = epqstate;

     
                                                                           
                                                                         
                                                                         
                              
     
                                                                       
                                                                         
                                                                   
                                                                            
                                                                        
                                                                         
                                                                          
     
  rcestate->es_direction = ForwardScanDirection;
  rcestate->es_snapshot = parentestate->es_snapshot;
  rcestate->es_crosscheck_snapshot = parentestate->es_crosscheck_snapshot;
  rcestate->es_range_table = parentestate->es_range_table;
  rcestate->es_range_table_array = parentestate->es_range_table_array;
  rcestate->es_range_table_size = parentestate->es_range_table_size;
  rcestate->es_relations = parentestate->es_relations;
  rcestate->es_queryEnv = parentestate->es_queryEnv;
  rcestate->es_rowmarks = parentestate->es_rowmarks;
  rcestate->es_plannedstmt = parentestate->es_plannedstmt;
  rcestate->es_junkFilter = parentestate->es_junkFilter;
  rcestate->es_output_cid = parentestate->es_output_cid;
  if (parentestate->es_num_result_relations > 0)
  {
    int numResultRelations = parentestate->es_num_result_relations;
    int numRootResultRels = parentestate->es_num_root_result_relations;
    ResultRelInfo *resultRelInfos;

    resultRelInfos = (ResultRelInfo *)palloc(numResultRelations * sizeof(ResultRelInfo));
    memcpy(resultRelInfos, parentestate->es_result_relations, numResultRelations * sizeof(ResultRelInfo));
    rcestate->es_result_relations = resultRelInfos;
    rcestate->es_num_result_relations = numResultRelations;

                                                          
    if (numRootResultRels > 0)
    {
      resultRelInfos = (ResultRelInfo *)palloc(numRootResultRels * sizeof(ResultRelInfo));
      memcpy(resultRelInfos, parentestate->es_root_result_relations, numRootResultRels * sizeof(ResultRelInfo));
      rcestate->es_root_result_relations = resultRelInfos;
      rcestate->es_num_root_result_relations = numRootResultRels;
    }
  }
                                                  
                                                   
  rcestate->es_top_eflags = parentestate->es_top_eflags;
  rcestate->es_instrument = parentestate->es_instrument;
                                             

     
                                                                         
                                                                           
                                                                         
                                                             
     
  rcestate->es_param_list_info = parentestate->es_param_list_info;
  if (parentestate->es_plannedstmt->paramExecTypes != NIL)
  {
    int i;

       
                                                                        
                                                                         
                                                                         
                                                                     
                                                                          
       
                                                                           
                                                                    
                                                                           
                                                                    
                                                                       
                                             
       
                                                                          
                                                                         
                                 
       
    ExecSetParamPlanMulti(planTree->extParam, GetPerTupleExprContext(parentestate));

                                                   
    i = list_length(parentestate->es_plannedstmt->paramExecTypes);
    rcestate->es_param_exec_vals = (ParamExecData *)palloc0(i * sizeof(ParamExecData));
                                                                    
    while (--i >= 0)
    {
                                                    
      rcestate->es_param_exec_vals[i].value = parentestate->es_param_exec_vals[i].value;
      rcestate->es_param_exec_vals[i].isnull = parentestate->es_param_exec_vals[i].isnull;
    }
  }

     
                                                                             
                                                               
                                                                           
                                                                          
                                                                         
          
     
  Assert(rcestate->es_subplanstates == NIL);
  foreach (l, parentestate->es_plannedstmt->subplans)
  {
    Plan *subplan = (Plan *)lfirst(l);
    PlanState *subplanstate;

    subplanstate = ExecInitNode(subplan, rcestate, 0);
    rcestate->es_subplanstates = lappend(rcestate->es_subplanstates, subplanstate);
  }

     
                                                     
                                                                         
              
     
  epqstate->relsubs_rowmark = (ExecAuxRowMark **)palloc0(rtsize * sizeof(ExecAuxRowMark *));
  foreach (l, epqstate->arowMarks)
  {
    ExecAuxRowMark *earm = (ExecAuxRowMark *)lfirst(l);

    epqstate->relsubs_rowmark[earm->rowmark->rti - 1] = earm;
  }

     
                                                              
     
  epqstate->relsubs_done = (bool *)palloc0(rtsize * sizeof(bool));

     
                                                                            
                                                                           
                                                     
     
  epqstate->recheckplanstate = ExecInitNode(planTree, rcestate, 0);

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                          
                                                 
   
                                                                             
                                                                         
                                                                          
                                                                         
                                                                        
   
void
EvalPlanQualEnd(EPQState *epqstate)
{
  EState *estate = epqstate->recheckestate;
  Index rtsize;
  MemoryContext oldcontext;
  ListCell *l;

  rtsize = epqstate->parentestate->es_range_table_size;

     
                                                                             
                                                                    
     
  if (epqstate->tuple_table != NIL)
  {
    memset(epqstate->relsubs_slot, 0, rtsize * sizeof(TupleTableSlot *));
    ExecResetTupleTable(epqstate->tuple_table, true);
    epqstate->tuple_table = NIL;
  }

                                                 
  if (estate == NULL)
  {
    return;
  }

  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

  ExecEndNode(epqstate->recheckplanstate);

  foreach (l, estate->es_subplanstates)
  {
    PlanState *subplanstate = (PlanState *)lfirst(l);

    ExecEndNode(subplanstate);
  }

                                                                         
  ExecResetTupleTable(estate->es_tupleTable, false);

                                                                  
  ExecCleanUpTriggerState(estate);

  MemoryContextSwitchTo(oldcontext);

  FreeExecutorState(estate);

                          
  epqstate->origslot = NULL;
  epqstate->recheckestate = NULL;
  epqstate->recheckplanstate = NULL;
  epqstate->relsubs_rowmark = NULL;
  epqstate->relsubs_done = NULL;
}
