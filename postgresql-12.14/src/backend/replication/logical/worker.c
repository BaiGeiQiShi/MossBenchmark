                                                                            
            
                                                    
   
                                                                
   
                  
                                              
   
         
                                                                              
                                             
   
                                                                      
                                                                    
                                                       
   
                                                                         
                                                                             
   
                                                                            
   

#include "postgres.h"

#include "access/table.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "libpq/pqsignal.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "postmaster/bgworker.h"
#include "postmaster/postmaster.h"
#include "postmaster/walwriter.h"
#include "replication/decode.h"
#include "replication/logical.h"
#include "replication/logicalproto.h"
#include "replication/logicalrelation.h"
#include "replication/logicalworker.h"
#include "replication/origin.h"
#include "replication/reorderbuffer.h"
#include "replication/snapbuild.h"
#include "replication/walreceiver.h"
#include "replication/worker_internal.h"
#include "rewrite/rewriteHandler.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/timeout.h"

#define NAPTIME_PER_CYCLE 1000                                         

typedef struct FlushPosition
{
  dlist_node node;
  XLogRecPtr local_end;
  XLogRecPtr remote_end;
} FlushPosition;

static dlist_head lsn_mapping = DLIST_STATIC_INIT(lsn_mapping);

typedef struct SlotErrCallbackArg
{
  LogicalRepRelMapEntry *rel;
  int remote_attnum;
} SlotErrCallbackArg;

static MemoryContext ApplyMessageContext = NULL;
MemoryContext ApplyContext = NULL;

WalReceiverConn *LogRepWorkerWalRcvConn = NULL;

Subscription *MySubscription = NULL;
bool MySubscriptionValid = false;

bool in_remote_transaction = false;
static XLogRecPtr remote_final_lsn = InvalidXLogRecPtr;

static void
send_feedback(XLogRecPtr recvpos, bool force, bool requestReply);

static void
store_flush_position(XLogRecPtr remote_lsn);

static void
maybe_reread_subscription(void);

                                  
static volatile sig_atomic_t got_SIGHUP = false;

   
                                                        
   
                                                                        
                                                                            
                                                                       
   
                                                                              
                                                                      
                                                                         
                                                                            
                              
   
static bool
should_apply_changes_for_rel(LogicalRepRelMapEntry *rel)
{
  if (am_tablesync_worker())
  {
    return MyLogicalRepWorker->relid == rel->localreloid;
  }
  else
  {
    return (rel->state == SUBREL_STATE_READY || (rel->state == SUBREL_STATE_SYNCDONE && rel->statelsn <= remote_final_lsn));
  }
}

   
                                                                          
   
                                                                          
                          
                                                                            
   
static void
begin_replication_step(void)
{
  SetCurrentStatementStartTimestamp();

  if (!IsTransactionState())
  {
    StartTransactionCommand();
    maybe_reread_subscription();
  }

  PushActiveSnapshot(GetTransactionSnapshot());

  MemoryContextSwitchTo(ApplyMessageContext);
}

   
                                                    
                                                            
   
                                                                    
                                                                 
   
static void
end_replication_step(void)
{
  PopActiveSnapshot();

  CommandCounterIncrement();
}

   
                                                                        
                         
   
                                           
   
static EState *
create_estate_for_relation(LogicalRepRelMapEntry *rel)
{
  EState *estate;
  ResultRelInfo *resultRelInfo;
  RangeTblEntry *rte;

  estate = CreateExecutorState();

  rte = makeNode(RangeTblEntry);
  rte->rtekind = RTE_RELATION;
  rte->relid = RelationGetRelid(rel->localrel);
  rte->relkind = rel->localrel->rd_rel->relkind;
  rte->rellockmode = AccessShareLock;
  ExecInitRangeTable(estate, list_make1(rte));

  resultRelInfo = makeNode(ResultRelInfo);
  InitResultRelInfo(resultRelInfo, rel->localrel, 1, NULL, 0);

  estate->es_result_relations = resultRelInfo;
  estate->es_num_result_relations = 1;
  estate->es_result_relation_info = resultRelInfo;

  estate->es_output_cid = GetCurrentCommandId(true);

                                        
  AfterTriggerBeginQuery();

  return estate;
}

   
                                                                  
                                 
   
static void
finish_estate(EState *estate)
{
                                         
  AfterTriggerEndQuery(estate);

                
  ExecResetTupleTable(estate->es_tupleTable, false);
  FreeExecutorState(estate);
}

   
                                                                        
                     
   
                                                                              
                         
   
static void
slot_fill_defaults(LogicalRepRelMapEntry *rel, EState *estate, TupleTableSlot *slot)
{
  TupleDesc desc = RelationGetDescr(rel->localrel);
  int num_phys_attrs = desc->natts;
  int i;
  int attnum, num_defaults = 0;
  int *defmap;
  ExprState **defexprs;
  ExprContext *econtext;

  econtext = GetPerTupleExprContext(estate);

                                                                          
  if (num_phys_attrs == rel->remoterel.natts)
  {
    return;
  }

  defmap = (int *)palloc(num_phys_attrs * sizeof(int));
  defexprs = (ExprState **)palloc(num_phys_attrs * sizeof(ExprState *));

  for (attnum = 0; attnum < num_phys_attrs; attnum++)
  {
    Expr *defexpr;

    if (TupleDescAttr(desc, attnum)->attisdropped || TupleDescAttr(desc, attnum)->attgenerated)
    {
      continue;
    }

    if (rel->attrmap[attnum] >= 0)
    {
      continue;
    }

    defexpr = (Expr *)build_column_default(rel->localrel, attnum + 1);

    if (defexpr != NULL)
    {
                                              
      defexpr = expression_planner(defexpr);

                                                           
      defexprs[num_defaults] = ExecInitExpr(defexpr, NULL);
      defmap[num_defaults] = attnum;
      num_defaults++;
    }
  }

  for (i = 0; i < num_defaults; i++)
  {
    slot->tts_values[defmap[i]] = ExecEvalExpr(defexprs[i], econtext, &slot->tts_isnull[defmap[i]]);
  }
}

   
                                                                           
                                              
   
static void
slot_store_error_callback(void *arg)
{
  SlotErrCallbackArg *errarg = (SlotErrCallbackArg *)arg;
  LogicalRepRelMapEntry *rel;

                                                           
  if (errarg->remote_attnum < 0)
  {
    return;
  }

  rel = errarg->rel;
  errcontext("processing remote data for replication target relation \"%s.%s\" column \"%s\"", rel->remoterel.nspname, rel->remoterel.relname, rel->remoterel.attnames[errarg->remote_attnum]);
}

   
                                          
                                                                         
               
   
static void
slot_store_cstrings(TupleTableSlot *slot, LogicalRepRelMapEntry *rel, char **values)
{
  int natts = slot->tts_tupleDescriptor->natts;
  int i;
  SlotErrCallbackArg errarg;
  ErrorContextCallback errcallback;

  ExecClearTuple(slot);

                                                       
  errarg.rel = rel;
  errarg.remote_attnum = -1;
  errcallback.callback = slot_store_error_callback;
  errcallback.arg = (void *)&errarg;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                                                             
  for (i = 0; i < natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(slot->tts_tupleDescriptor, i);
    int remoteattnum = rel->attrmap[i];

    if (!att->attisdropped && remoteattnum >= 0 && values[remoteattnum] != NULL)
    {
      Oid typinput;
      Oid typioparam;

      errarg.remote_attnum = remoteattnum;

      getTypeInputInfo(att->atttypid, &typinput, &typioparam);
      slot->tts_values[i] = OidInputFunctionCall(typinput, values[remoteattnum], typioparam, att->atttypmod);
      slot->tts_isnull[i] = false;

      errarg.remote_attnum = -1;
    }
    else
    {
         
                                                                        
                                                             
                              
         
      slot->tts_values[i] = (Datum)0;
      slot->tts_isnull[i] = true;
    }
  }

                                   
  error_context_stack = errcallback.previous;

  ExecStoreVirtualTuple(slot);
}

   
                                                                  
                                                                         
                                     
                                                                
                                                                      
                  
                                                                         
                                                                            
                                                                              
   
static void
slot_modify_cstrings(TupleTableSlot *slot, TupleTableSlot *srcslot, LogicalRepRelMapEntry *rel, char **values, bool *replaces)
{
  int natts = slot->tts_tupleDescriptor->natts;
  int i;
  SlotErrCallbackArg errarg;
  ErrorContextCallback errcallback;

                                                                         
  ExecClearTuple(slot);

     
                                                                            
                             
     
  Assert(natts == srcslot->tts_tupleDescriptor->natts);
  slot_getallattrs(srcslot);
  memcpy(slot->tts_values, srcslot->tts_values, natts * sizeof(Datum));
  memcpy(slot->tts_isnull, srcslot->tts_isnull, natts * sizeof(bool));

                                                                            
  errarg.rel = rel;
  errarg.remote_attnum = -1;
  errcallback.callback = slot_store_error_callback;
  errcallback.arg = (void *)&errarg;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                                                          
  for (i = 0; i < natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(slot->tts_tupleDescriptor, i);
    int remoteattnum = rel->attrmap[i];

    if (remoteattnum < 0)
    {
      continue;
    }

    if (!replaces[remoteattnum])
    {
      continue;
    }

    if (values[remoteattnum] != NULL)
    {
      Oid typinput;
      Oid typioparam;

      errarg.remote_attnum = remoteattnum;

      getTypeInputInfo(att->atttypid, &typinput, &typioparam);
      slot->tts_values[i] = OidInputFunctionCall(typinput, values[remoteattnum], typioparam, att->atttypmod);
      slot->tts_isnull[i] = false;

      errarg.remote_attnum = -1;
    }
    else
    {
      slot->tts_values[i] = (Datum)0;
      slot->tts_isnull[i] = true;
    }
  }

                                   
  error_context_stack = errcallback.previous;

                                                                       
  ExecStoreVirtualTuple(slot);
}

   
                         
   
static void
apply_handle_begin(StringInfo s)
{
  LogicalRepBeginData begin_data;

  logicalrep_read_begin(s, &begin_data);

  remote_final_lsn = begin_data.final_lsn;

  in_remote_transaction = true;

  pgstat_report_activity(STATE_RUNNING, NULL);
}

   
                          
   
                                              
   
static void
apply_handle_commit(StringInfo s)
{
  LogicalRepCommitData commit_data;

  logicalrep_read_commit(s, &commit_data);

  if (commit_data.commit_lsn != remote_final_lsn)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg_internal("incorrect commit LSN %X/%X in commit message (expected %X/%X)", (uint32)(commit_data.commit_lsn >> 32), (uint32)commit_data.commit_lsn, (uint32)(remote_final_lsn >> 32), (uint32)remote_final_lsn)));
  }

                                                              
  if (IsTransactionState() && !am_tablesync_worker())
  {
       
                                                                    
                                  
       
    replorigin_session_origin_lsn = commit_data.end_lsn;
    replorigin_session_origin_timestamp = commit_data.committime;

    CommitTransactionCommand();
    pgstat_report_stat(false);

    store_flush_position(commit_data.end_lsn);
  }
  else
  {
                                                                        
    AcceptInvalidationMessages();
    maybe_reread_subscription();
  }

  in_remote_transaction = false;

                                                                   
  process_syncing_tables(commit_data.end_lsn);

  pgstat_report_activity(STATE_IDLE, NULL);
}

   
                          
   
                                              
   
static void
apply_handle_origin(StringInfo s)
{
     
                                                                           
                    
     
  if (!in_remote_transaction || (IsTransactionState() && !am_tablesync_worker()))
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("ORIGIN message sent out of order")));
  }
}

   
                            
   
                                                                         
                                                                           
                                                                             
                             
   
static void
apply_handle_relation(StringInfo s)
{
  LogicalRepRelation *rel;

  rel = logicalrep_read_rel(s);
  logicalrep_relmap_update(rel);
}

   
                        
   
                                                                              
                                                                              
                                                                         
                        
   
static void
apply_handle_type(StringInfo s)
{
  LogicalRepTyp typ;

  logicalrep_read_typ(s, &typ);
}

   
                                                                     
   
                                             
   
static Oid
GetRelationIdentityOrPK(Relation rel)
{
  Oid idxoid;

  idxoid = RelationGetReplicaIndex(rel);

  if (!OidIsValid(idxoid))
  {
    idxoid = RelationGetPrimaryKeyIndex(rel);
  }

  return idxoid;
}

   
                          
   
static void
apply_handle_insert(StringInfo s)
{
  LogicalRepRelMapEntry *rel;
  LogicalRepTupleData newtup;
  LogicalRepRelId relid;
  EState *estate;
  TupleTableSlot *remoteslot;
  MemoryContext oldctx;

  begin_replication_step();

  relid = logicalrep_read_insert(s, &newtup);
  rel = logicalrep_rel_open(relid, RowExclusiveLock);
  if (!should_apply_changes_for_rel(rel))
  {
       
                                                                  
                                              
       
    logicalrep_rel_close(rel, RowExclusiveLock);
    end_replication_step();
    return;
  }

                                      
  estate = create_estate_for_relation(rel);
  remoteslot = ExecInitExtraTupleSlot(estate, RelationGetDescr(rel->localrel), &TTSOpsVirtual);

                                                  
  oldctx = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
  slot_store_cstrings(remoteslot, rel, newtup.values);
  slot_fill_defaults(rel, estate, remoteslot);
  MemoryContextSwitchTo(oldctx);

  ExecOpenIndices(estate->es_result_relation_info, false);

                      
  ExecSimpleRelationInsert(estate, remoteslot);

                
  ExecCloseIndices(estate->es_result_relation_info);

  finish_estate(estate);

  logicalrep_rel_close(rel, NoLock);

  end_replication_step();
}

   
                                                                    
                                  
   
static void
check_relation_updatable(LogicalRepRelMapEntry *rel)
{
                            
  if (rel->updatable)
  {
    return;
  }

     
                                                                             
                              
     
  if (OidIsValid(GetRelationIdentityOrPK(rel->localrel)))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("publisher did not send replica identity column "
                                                                              "expected by the logical replication target relation \"%s.%s\"",
                                                                           rel->remoterel.nspname, rel->remoterel.relname)));
  }

  ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("logical replication target relation \"%s.%s\" has "
                                                                            "neither REPLICA IDENTITY index nor PRIMARY "
                                                                            "KEY and published relation does not have "
                                                                            "REPLICA IDENTITY FULL",
                                                                         rel->remoterel.nspname, rel->remoterel.relname)));
}

   
                          
   
                     
   
static void
apply_handle_update(StringInfo s)
{
  LogicalRepRelMapEntry *rel;
  LogicalRepRelId relid;
  Oid idxoid;
  EState *estate;
  EPQState epqstate;
  LogicalRepTupleData oldtup;
  LogicalRepTupleData newtup;
  bool has_oldtup;
  TupleTableSlot *localslot;
  TupleTableSlot *remoteslot;
  RangeTblEntry *target_rte;
  bool found;
  MemoryContext oldctx;

  begin_replication_step();

  relid = logicalrep_read_update(s, &has_oldtup, &oldtup, &newtup);
  rel = logicalrep_rel_open(relid, RowExclusiveLock);
  if (!should_apply_changes_for_rel(rel))
  {
       
                                                                  
                                              
       
    logicalrep_rel_close(rel, RowExclusiveLock);
    end_replication_step();
    return;
  }

                                      
  check_relation_updatable(rel);

                                      
  estate = create_estate_for_relation(rel);
  remoteslot = ExecInitExtraTupleSlot(estate, RelationGetDescr(rel->localrel), &TTSOpsVirtual);
  localslot = table_slot_create(rel->localrel, &estate->es_tupleTable);
  EvalPlanQualInit(&epqstate, estate, NULL, NIL, -1);

     
                                                                            
                                                                      
                                                                   
                                                                            
                                                         
     
  target_rte = list_nth(estate->es_range_table, 0);
  for (int i = 0; i < remoteslot->tts_tupleDescriptor->natts; i++)
  {
    Form_pg_attribute att = TupleDescAttr(remoteslot->tts_tupleDescriptor, i);
    int remoteattnum = rel->attrmap[i];

    if (!att->attisdropped && remoteattnum >= 0)
    {
      if (newtup.changed[remoteattnum])
      {
        target_rte->updatedCols = bms_add_member(target_rte->updatedCols, i + 1 - FirstLowInvalidHeapAttributeNumber);
      }
    }
  }

                                                                         
  fill_extraUpdatedCols(target_rte, rel->localrel);

  ExecOpenIndices(estate->es_result_relation_info, false);

                               
  oldctx = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
  slot_store_cstrings(remoteslot, rel, has_oldtup ? oldtup.values : newtup.values);
  MemoryContextSwitchTo(oldctx);

     
                                                                           
                                 
     
  idxoid = GetRelationIdentityOrPK(rel->localrel);
  Assert(OidIsValid(idxoid) || (rel->remoterel.replident == REPLICA_IDENTITY_FULL && has_oldtup));

  if (OidIsValid(idxoid))
  {
    found = RelationFindReplTupleByIndex(rel->localrel, idxoid, LockTupleExclusive, remoteslot, localslot);
  }
  else
  {
    found = RelationFindReplTupleSeq(rel->localrel, LockTupleExclusive, remoteslot, localslot);
  }

  ExecClearTuple(remoteslot);

     
                  
     
                                                                        
     
  if (found)
  {
                                                    
    oldctx = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
    slot_modify_cstrings(remoteslot, localslot, rel, newtup.values, newtup.changed);
    MemoryContextSwitchTo(oldctx);

    EvalPlanQualSetSlot(&epqstate, remoteslot);

                               
    ExecSimpleRelationUpdate(estate, &epqstate, localslot, remoteslot);
  }
  else
  {
       
                                                   
       
                                                                  
       
    elog(DEBUG1,
        "logical replication did not find row for update "
        "in replication target relation \"%s\"",
        RelationGetRelationName(rel->localrel));
  }

                
  EvalPlanQualEnd(&epqstate);
  ExecCloseIndices(estate->es_result_relation_info);

  finish_estate(estate);

  logicalrep_rel_close(rel, NoLock);

  end_replication_step();
}

   
                          
   
                     
   
static void
apply_handle_delete(StringInfo s)
{
  LogicalRepRelMapEntry *rel;
  LogicalRepTupleData oldtup;
  LogicalRepRelId relid;
  Oid idxoid;
  EState *estate;
  EPQState epqstate;
  TupleTableSlot *remoteslot;
  TupleTableSlot *localslot;
  bool found;
  MemoryContext oldctx;

  begin_replication_step();

  relid = logicalrep_read_delete(s, &oldtup);
  rel = logicalrep_rel_open(relid, RowExclusiveLock);
  if (!should_apply_changes_for_rel(rel))
  {
       
                                                                  
                                              
       
    logicalrep_rel_close(rel, RowExclusiveLock);
    end_replication_step();
    return;
  }

                                      
  check_relation_updatable(rel);

                                      
  estate = create_estate_for_relation(rel);
  remoteslot = ExecInitExtraTupleSlot(estate, RelationGetDescr(rel->localrel), &TTSOpsVirtual);
  localslot = table_slot_create(rel->localrel, &estate->es_tupleTable);
  EvalPlanQualInit(&epqstate, estate, NULL, NIL, -1);

  ExecOpenIndices(estate->es_result_relation_info, false);

                                                        
  oldctx = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));
  slot_store_cstrings(remoteslot, rel, oldtup.values);
  MemoryContextSwitchTo(oldctx);

     
                                                                           
                                 
     
  idxoid = GetRelationIdentityOrPK(rel->localrel);
  Assert(OidIsValid(idxoid) || (rel->remoterel.replident == REPLICA_IDENTITY_FULL));

  if (OidIsValid(idxoid))
  {
    found = RelationFindReplTupleByIndex(rel->localrel, idxoid, LockTupleExclusive, remoteslot, localslot);
  }
  else
  {
    found = RelationFindReplTupleSeq(rel->localrel, LockTupleExclusive, remoteslot, localslot);
  }
                           
  if (found)
  {
    EvalPlanQualSetSlot(&epqstate, localslot);

                               
    ExecSimpleRelationDelete(estate, &epqstate, localslot);
  }
  else
  {
                                                     
    elog(DEBUG1,
        "logical replication could not find row for delete "
        "in replication target relation \"%s\"",
        RelationGetRelationName(rel->localrel));
  }

                
  EvalPlanQualEnd(&epqstate);
  ExecCloseIndices(estate->es_result_relation_info);

  finish_estate(estate);

  logicalrep_rel_close(rel, NoLock);

  end_replication_step();
}

   
                            
   
                     
   
static void
apply_handle_truncate(StringInfo s)
{
  bool cascade = false;
  bool restart_seqs = false;
  List *remote_relids = NIL;
  List *remote_rels = NIL;
  List *rels = NIL;
  List *relids = NIL;
  List *relids_logged = NIL;
  ListCell *lc;
  LOCKMODE lockmode = AccessExclusiveLock;

  begin_replication_step();

  remote_relids = logicalrep_read_truncate(s, &cascade, &restart_seqs);

  foreach (lc, remote_relids)
  {
    LogicalRepRelId relid = lfirst_oid(lc);
    LogicalRepRelMapEntry *rel;

    rel = logicalrep_rel_open(relid, lockmode);
    if (!should_apply_changes_for_rel(rel))
    {
         
                                                                    
                                                
         
      logicalrep_rel_close(rel, lockmode);
      continue;
    }

    remote_rels = lappend(remote_rels, rel);
    rels = lappend(rels, rel->localrel);
    relids = lappend_oid(relids, rel->localreloid);
    if (RelationIsLogicallyLogged(rel->localrel))
    {
      relids_logged = lappend_oid(relids_logged, rel->localreloid);
    }
  }

     
                                                                             
                                                                      
                                              
     
  ExecuteTruncateGuts(rels, relids, relids_logged, DROP_RESTRICT, restart_seqs);

  foreach (lc, remote_rels)
  {
    LogicalRepRelMapEntry *rel = lfirst(lc);

    logicalrep_rel_close(rel, NoLock);
  }

  end_replication_step();
}

   
                                                    
   
static void
apply_dispatch(StringInfo s)
{
  char action = pq_getmsgbyte(s);

  switch (action)
  {
               
  case 'B':
    apply_handle_begin(s);
    break;
                
  case 'C':
    apply_handle_commit(s);
    break;
                
  case 'I':
    apply_handle_insert(s);
    break;
                
  case 'U':
    apply_handle_update(s);
    break;
                
  case 'D':
    apply_handle_delete(s);
    break;
                  
  case 'T':
    apply_handle_truncate(s);
    break;
                  
  case 'R':
    apply_handle_relation(s);
    break;
              
  case 'Y':
    apply_handle_type(s);
    break;
                
  case 'O':
    apply_handle_origin(s);
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid logical replication message type \"%c\"", action)));
  }
}

   
                                                                              
   
                                                                              
                                                                          
                                                                              
                                                                            
                                                                              
                           
   
                                                                            
                       
   
static void
get_flush_position(XLogRecPtr *write, XLogRecPtr *flush, bool *have_pending_txes)
{
  dlist_mutable_iter iter;
  XLogRecPtr local_flush = GetFlushRecPtr();

  *write = InvalidXLogRecPtr;
  *flush = InvalidXLogRecPtr;

  dlist_foreach_modify(iter, &lsn_mapping)
  {
    FlushPosition *pos = dlist_container(FlushPosition, node, iter.cur);

    *write = pos->remote_end;

    if (pos->local_end <= local_flush)
    {
      *flush = pos->remote_end;
      dlist_delete(iter.cur);
      pfree(pos);
    }
    else
    {
         
                                                                         
                                                                     
                                             
         
      pos = dlist_tail_element(FlushPosition, node, &lsn_mapping);
      *write = pos->remote_end;
      *have_pending_txes = true;
      return;
    }
  }

  *have_pending_txes = !dlist_is_empty(&lsn_mapping);
}

   
                                                             
   
static void
store_flush_position(XLogRecPtr remote_lsn)
{
  FlushPosition *flushpos;

                                            
  MemoryContextSwitchTo(ApplyContext);

                         
  flushpos = (FlushPosition *)palloc(sizeof(FlushPosition));
  flushpos->local_end = XactLastCommitEnd;
  flushpos->remote_end = remote_lsn;

  dlist_push_tail(&lsn_mapping, &flushpos->node);
  MemoryContextSwitchTo(ApplyMessageContext);
}

                                      
static void
UpdateWorkerStats(XLogRecPtr last_lsn, TimestampTz send_time, bool reply)
{
  MyLogicalRepWorker->last_lsn = last_lsn;
  MyLogicalRepWorker->last_send_time = send_time;
  MyLogicalRepWorker->last_recv_time = GetCurrentTimestamp();
  if (reply)
  {
    MyLogicalRepWorker->reply_lsn = last_lsn;
    MyLogicalRepWorker->reply_time = send_time;
  }
}

   
                    
   
static void
LogicalRepApplyLoop(XLogRecPtr last_received)
{
  TimestampTz last_recv_timestamp = GetCurrentTimestamp();
  bool ping_sent = false;

     
                                                                           
                       
     
  ApplyMessageContext = AllocSetContextCreate(ApplyContext, "ApplyMessageContext", ALLOCSET_DEFAULT_SIZES);

                                             
  pgstat_report_activity(STATE_IDLE, NULL);

                                               
  for (;;)
  {
    pgsocket fd = PGINVALID_SOCKET;
    int rc;
    int len;
    char *buf = NULL;
    bool endofstream = false;
    long wait_time;

    CHECK_FOR_INTERRUPTS();

    MemoryContextSwitchTo(ApplyMessageContext);

    len = walrcv_receive(LogRepWorkerWalRcvConn, &buf, &fd);

    if (len != 0)
    {
                                                                  
      for (;;)
      {
        CHECK_FOR_INTERRUPTS();

        if (len == 0)
        {
          break;
        }
        else if (len < 0)
        {
          ereport(LOG, (errmsg("data stream from publisher has ended")));
          endofstream = true;
          break;
        }
        else
        {
          int c;
          StringInfoData s;

                              
          last_recv_timestamp = GetCurrentTimestamp();
          ping_sent = false;

                                                                       
          MemoryContextSwitchTo(ApplyMessageContext);

          s.data = buf;
          s.len = len;
          s.cursor = 0;
          s.maxlen = -1;

          c = pq_getmsgbyte(&s);

          if (c == 'w')
          {
            XLogRecPtr start_lsn;
            XLogRecPtr end_lsn;
            TimestampTz send_time;

            start_lsn = pq_getmsgint64(&s);
            end_lsn = pq_getmsgint64(&s);
            send_time = pq_getmsgint64(&s);

            if (last_received < start_lsn)
            {
              last_received = start_lsn;
            }

            if (last_received < end_lsn)
            {
              last_received = end_lsn;
            }

            UpdateWorkerStats(last_received, send_time, false);

            apply_dispatch(&s);
          }
          else if (c == 'k')
          {
            XLogRecPtr end_lsn;
            TimestampTz timestamp;
            bool reply_requested;

            end_lsn = pq_getmsgint64(&s);
            timestamp = pq_getmsgint64(&s);
            reply_requested = pq_getmsgbyte(&s);

            if (last_received < end_lsn)
            {
              last_received = end_lsn;
            }

            send_feedback(last_received, reply_requested, false);
            UpdateWorkerStats(last_received, timestamp, true);
          }
                                                            

          MemoryContextReset(ApplyMessageContext);
        }

        len = walrcv_receive(LogRepWorkerWalRcvConn, &buf, &fd);
      }
    }

                                   
    send_feedback(last_received, false, false);

    if (!in_remote_transaction)
    {
         
                                                                      
                                                                     
              
         
      AcceptInvalidationMessages();
      maybe_reread_subscription();

                                                      
      process_syncing_tables(last_received);
    }

                             
    MemoryContextResetAndDeleteChildren(ApplyMessageContext);
    MemoryContextSwitchTo(TopMemoryContext);

                                                      
    if (endofstream)
    {
      TimeLineID tli;

      walrcv_endstreaming(LogRepWorkerWalRcvConn, &tli);
      break;
    }

       
                                                                        
                                                                           
                                                                          
                                                                     
               
       
    if (!dlist_is_empty(&lsn_mapping))
    {
      wait_time = WalWriterDelay;
    }
    else
    {
      wait_time = NAPTIME_PER_CYCLE;
    }

    rc = WaitLatchOrSocket(MyLatch, WL_SOCKET_READABLE | WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, fd, wait_time, WAIT_EVENT_LOGICAL_APPLY_MAIN);

    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      CHECK_FOR_INTERRUPTS();
    }

    if (got_SIGHUP)
    {
      got_SIGHUP = false;
      ProcessConfigFile(PGC_SIGHUP);
    }

    if (rc & WL_TIMEOUT)
    {
         
                                                                      
                                                                      
                                                    
                                                                     
                                                                  
                                   
         
      bool requestReply = false;

         
                                                                       
                           
         
      if (wal_receiver_timeout > 0)
      {
        TimestampTz now = GetCurrentTimestamp();
        TimestampTz timeout;

        timeout = TimestampTzPlusMilliseconds(last_recv_timestamp, wal_receiver_timeout);

        if (now >= timeout)
        {
          ereport(ERROR, (errmsg("terminating logical replication worker due to timeout")));
        }

                                                   
        if (!ping_sent)
        {
          timeout = TimestampTzPlusMilliseconds(last_recv_timestamp, (wal_receiver_timeout / 2));
          if (now >= timeout)
          {
            requestReply = true;
            ping_sent = true;
          }
        }
      }

      send_feedback(last_received, requestReply, requestReply);
    }
  }
}

   
                                                   
   
                                                                               
                                         
   
static void
send_feedback(XLogRecPtr recvpos, bool force, bool requestReply)
{
  static StringInfo reply_message = NULL;
  static TimestampTz send_time = 0;

  static XLogRecPtr last_recvpos = InvalidXLogRecPtr;
  static XLogRecPtr last_writepos = InvalidXLogRecPtr;
  static XLogRecPtr last_flushpos = InvalidXLogRecPtr;

  XLogRecPtr writepos;
  XLogRecPtr flushpos;
  TimestampTz now;
  bool have_pending_txes;

     
                                                                         
                                                
     
  if (!force && wal_receiver_status_interval <= 0)
  {
    return;
  }

                                        
  if (recvpos < last_recvpos)
  {
    recvpos = last_recvpos;
  }

  get_flush_position(&writepos, &flushpos, &have_pending_txes);

     
                                                                             
                                                              
     
  if (!have_pending_txes)
  {
    flushpos = writepos = recvpos;
  }

  if (writepos < last_writepos)
  {
    writepos = last_writepos;
  }

  if (flushpos < last_flushpos)
  {
    flushpos = last_flushpos;
  }

  now = GetCurrentTimestamp();

                                                       
  if (!force && writepos == last_writepos && flushpos == last_flushpos && !TimestampDifferenceExceeds(send_time, now, wal_receiver_status_interval * 1000))
  {
    return;
  }
  send_time = now;

  if (!reply_message)
  {
    MemoryContext oldctx = MemoryContextSwitchTo(ApplyContext);

    reply_message = makeStringInfo();
    MemoryContextSwitchTo(oldctx);
  }
  else
  {
    resetStringInfo(reply_message);
  }

  pq_sendbyte(reply_message, 'r');
  pq_sendint64(reply_message, recvpos);                
  pq_sendint64(reply_message, flushpos);               
  pq_sendint64(reply_message, writepos);               
  pq_sendint64(reply_message, now);                       
  pq_sendbyte(reply_message, requestReply);                     

  elog(DEBUG2, "sending feedback (force %d) to recv %X/%X, write %X/%X, flush %X/%X", force, (uint32)(recvpos >> 32), (uint32)recvpos, (uint32)(writepos >> 32), (uint32)writepos, (uint32)(flushpos >> 32), (uint32)flushpos);

  walrcv_send(LogRepWorkerWalRcvConn, reply_message->data, reply_message->len);

  if (recvpos > last_recvpos)
  {
    last_recvpos = recvpos;
  }
  if (writepos > last_writepos)
  {
    last_writepos = writepos;
  }
  if (flushpos > last_flushpos)
  {
    last_flushpos = flushpos;
  }
}

   
                                                                  
   
static void
maybe_reread_subscription(void)
{
  MemoryContext oldctx;
  Subscription *newsub;
  bool started_tx = false;

                                                              
  if (MySubscriptionValid)
  {
    return;
  }

                                                                       
  if (!IsTransactionState())
  {
    StartTransactionCommand();
    started_tx = true;
  }

                                                
  oldctx = MemoryContextSwitchTo(ApplyContext);

  newsub = GetSubscription(MyLogicalRepWorker->subid, true);

     
                                                                           
                                                         
     
  if (!newsub)
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" will "
                         "stop because the subscription was removed",
                     MySubscription->name)));

    proc_exit(0);
  }

     
                                                                            
                                                                      
     
  if (!newsub->enabled)
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" will "
                         "stop because the subscription was disabled",
                     MySubscription->name)));

    proc_exit(0);
  }

     
                                                                        
             
     
  if (strcmp(newsub->conninfo, MySubscription->conninfo) != 0)
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" will "
                         "restart because the connection information was changed",
                     MySubscription->name)));

    proc_exit(0);
  }

     
                                                          
                                                                     
     
  if (strcmp(newsub->name, MySubscription->name) != 0)
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" will "
                         "restart because subscription was renamed",
                     MySubscription->name)));

    proc_exit(0);
  }

                                                           
  Assert(newsub->slotname);

     
                                                                            
                                           
     
  if (strcmp(newsub->slotname, MySubscription->slotname) != 0)
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" will "
                         "restart because the replication slot name was changed",
                     MySubscription->name)));

    proc_exit(0);
  }

     
                                                                       
             
     
  if (!equal(newsub->publications, MySubscription->publications))
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" will "
                         "restart because subscription's publications were changed",
                     MySubscription->name)));

    proc_exit(0);
  }

                                                             
  if (newsub->dbid != MySubscription->dbid)
  {
    elog(ERROR, "subscription %u changed unexpectedly", MyLogicalRepWorker->subid);
  }

                                                          
  FreeSubscription(MySubscription);
  MySubscription = newsub;

  MemoryContextSwitchTo(oldctx);

                                                                
  SetConfigOption("synchronous_commit", MySubscription->synccommit, PGC_BACKEND, PGC_S_OVERRIDE);

  if (started_tx)
  {
    CommitTransactionCommand();
  }

  MySubscriptionValid = true;
}

   
                                                     
   
static void
subscription_change_cb(Datum arg, int cacheid, uint32 hashvalue)
{
  MySubscriptionValid = false;
}

                                                                      
static void
logicalrep_worker_sighup(SIGNAL_ARGS)
{
  int save_errno = errno;

  got_SIGHUP = true;

                                                   
  SetLatch(MyLatch);

  errno = save_errno;
}

                                                  
void
ApplyWorkerMain(Datum main_arg)
{
  int worker_slot = DatumGetInt32(main_arg);
  MemoryContext oldctx;
  char originname[NAMEDATALEN];
  XLogRecPtr origin_startpos;
  char *myslotname;
  WalRcvStreamOptions options;

                      
  logicalrep_worker_attach(worker_slot);

                             
  pqsignal(SIGHUP, logicalrep_worker_sighup);
  pqsignal(SIGTERM, die);
  BackgroundWorkerUnblockSignals();

     
                                                                             
                                                                  
     

                                          
  MyLogicalRepWorker->last_send_time = MyLogicalRepWorker->last_recv_time = MyLogicalRepWorker->reply_time = GetCurrentTimestamp();

                                         
  load_file("libpqwalreceiver", false);

                                                
  SetConfigOption("session_replication_role", "replica", PGC_SUSET, PGC_S_OVERRIDE);

                                
  BackgroundWorkerInitializeConnectionByOid(MyLogicalRepWorker->dbid, MyLogicalRepWorker->userid, 0);

     
                                                                           
                                    
     
  SetConfigOption("search_path", "", PGC_SUSET, PGC_S_OVERRIDE);

                                                             
  ApplyContext = AllocSetContextCreate(TopMemoryContext, "ApplyContext", ALLOCSET_DEFAULT_SIZES);
  StartTransactionCommand();
  oldctx = MemoryContextSwitchTo(ApplyContext);

  MySubscription = GetSubscription(MyLogicalRepWorker->subid, true);
  if (!MySubscription)
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription %u will not "
                         "start because the subscription was removed during startup",
                     MyLogicalRepWorker->subid)));
    proc_exit(0);
  }

  MySubscriptionValid = true;
  MemoryContextSwitchTo(oldctx);

  if (!MySubscription->enabled)
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" will not "
                         "start because the subscription was disabled during startup",
                     MySubscription->name)));

    proc_exit(0);
  }

                                                               
  SetConfigOption("synchronous_commit", MySubscription->synccommit, PGC_BACKEND, PGC_S_OVERRIDE);

                                                    
  CacheRegisterSyscacheCallback(SUBSCRIPTIONOID, subscription_change_cb, (Datum)0);

  if (am_tablesync_worker())
  {
    ereport(LOG, (errmsg("logical replication table synchronization worker for subscription \"%s\", table \"%s\" has started", MySubscription->name, get_rel_name(MyLogicalRepWorker->relid))));
  }
  else
  {
    ereport(LOG, (errmsg("logical replication apply worker for subscription \"%s\" has started", MySubscription->name)));
  }

  CommitTransactionCommand();

                                                        
  elog(DEBUG1, "connecting to publisher using connection string \"%s\"", MySubscription->conninfo);

  if (am_tablesync_worker())
  {
    char *syncslotname;

                                                                 
    syncslotname = LogicalRepSyncTableStart(&origin_startpos);

                                                                          
    oldctx = MemoryContextSwitchTo(ApplyContext);
    myslotname = pstrdup(syncslotname);
    MemoryContextSwitchTo(oldctx);

    pfree(syncslotname);
  }
  else
  {
                                   
    RepOriginId originid;
    TimeLineID startpointTLI;
    char *err;

    myslotname = MySubscription->slotname;

       
                                                                       
                                                                           
                               
       
    if (!myslotname)
    {
      ereport(ERROR, (errmsg("subscription has no replication slot set")));
    }

                                            
    StartTransactionCommand();
    snprintf(originname, sizeof(originname), "pg_%u", MySubscription->oid);
    originid = replorigin_by_name(originname, true);
    if (!OidIsValid(originid))
    {
      originid = replorigin_create(originname);
    }
    replorigin_session_setup(originid);
    replorigin_session_origin = originid;
    origin_startpos = replorigin_session_get_progress(false);
    CommitTransactionCommand();

    LogRepWorkerWalRcvConn = walrcv_connect(MySubscription->conninfo, true, MySubscription->name, &err);
    if (LogRepWorkerWalRcvConn == NULL)
    {
      ereport(ERROR, (errmsg("could not connect to the publisher: %s", err)));
    }

       
                                                                          
                                                                         
       
    (void)walrcv_identify_system(LogRepWorkerWalRcvConn, &startpointTLI);
  }

     
                                                                           
                                      
     
  CacheRegisterSyscacheCallback(SUBSCRIPTIONRELMAP, invalidate_syncing_table_states, (Datum)0);

                                                    
  options.logical = true;
  options.startpoint = origin_startpos;
  options.slotname = myslotname;
  options.proto.logical.proto_version = LOGICALREP_PROTO_VERSION_NUM;
  options.proto.logical.publication_names = MySubscription->publications;

                                                   
  walrcv_startstreaming(LogRepWorkerWalRcvConn, &options);

                          
  LogicalRepApplyLoop(origin_startpos);

  proc_exit(0);
}

   
                                                    
   
bool
IsLogicalWorker(void)
{
  return MyLogicalRepWorker != NULL;
}
