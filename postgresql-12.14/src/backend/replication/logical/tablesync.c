                                                                            
               
                                    
   
                                                                
   
                  
                                                 
   
         
                                                                        
                          
   
                                                                         
                                                                            
                                                                              
                            
   
                                                                       
                                                                     
                                                    
                                                                          
                                                                    
                                                            
                                                                 
                                                                       
                                   
   
                                                                  
                                                                           
                                
                                                                              
                                                                     
                                                                         
                                           
                                                                            
                                                                           
                                                                       
                                                                     
                                                                         
                   
                                                                             
                                                                       
                                                                          
                                 
   
                                                                                    
                        
   
                                                                       
                                                                          
                                                                        
                                    
   
                                   
                           
             
                               
               
                              
                        
              
                                
             
               
                       
                     
               
                             
                       
              
                               
              
                              
                                     
              
                                
             
               
                             
                                 
                     
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"

#include "access/table.h"
#include "access/xact.h"

#include "catalog/pg_subscription_rel.h"
#include "catalog/pg_type.h"

#include "commands/copy.h"

#include "parser/parse_relation.h"

#include "replication/logicallauncher.h"
#include "replication/logicalrelation.h"
#include "replication/walreceiver.h"
#include "replication/worker_internal.h"

#include "utils/snapmgr.h"
#include "storage/ipc.h"

#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

static bool table_states_valid = false;

StringInfo copybuf = NULL;

   
                                            
   
static void
pg_attribute_noreturn() finish_sync_worker(void)
{
     
                                                                        
                                            
     
  if (IsTransactionState())
  {
    CommitTransactionCommand();
    pgstat_report_stat(false);
  }

                             
  XLogFlush(GetXLogWriteRecPtr());

  StartTransactionCommand();
  ereport(LOG, (errmsg("logical replication table synchronization worker for subscription \"%s\", table \"%s\" has finished", MySubscription->name, get_rel_name(MyLogicalRepWorker->relid))));
  CommitTransactionCommand();

                                                 
  logicalrep_worker_wakeup(MyLogicalRepWorker->subid, InvalidOid);

                       
  proc_exit(0);
}

   
                                                                              
                 
   
                                                           
   
                                                                                  
                   
   
static bool
wait_for_relation_state_change(Oid relid, char expected_state)
{
  char state;

  for (;;)
  {
    LogicalRepWorker *worker;
    XLogRecPtr statelsn;

    CHECK_FOR_INTERRUPTS();

                                                                 
    PushActiveSnapshot(GetLatestSnapshot());
    state = GetSubscriptionRelState(MyLogicalRepWorker->subid, relid, &statelsn, true);
    PopActiveSnapshot();

    if (state == SUBREL_STATE_UNKNOWN)
    {
      return false;
    }

    if (state == expected_state)
    {
      return true;
    }

                                                                    
    LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);

                                                                        
    worker = logicalrep_worker_find(MyLogicalRepWorker->subid, am_tablesync_worker() ? InvalidOid : relid, false);
    LWLockRelease(LogicalRepWorkerLock);
    if (!worker)
    {
      return false;
    }

    (void)WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 1000L, WAIT_EVENT_LOGICAL_SYNC_STATE_CHANGE);

    ResetLatch(MyLatch);
  }

  return false;
}

   
                                                                        
                               
   
                                                           
   
                                                      
   
static bool
wait_for_worker_state_change(char expected_state)
{
  int rc;

  for (;;)
  {
    LogicalRepWorker *worker;

    CHECK_FOR_INTERRUPTS();

       
                                                                          
                                                                         
       
    if (MyLogicalRepWorker->relstate == expected_state)
    {
      return true;
    }

       
                                                                   
                
       
    LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);
    worker = logicalrep_worker_find(MyLogicalRepWorker->subid, InvalidOid, false);
    if (worker && worker->proc)
    {
      logicalrep_worker_wakeup_ptr(worker);
    }
    LWLockRelease(LogicalRepWorkerLock);
    if (!worker)
    {
      break;
    }

       
                                                                          
                                                              
       
    rc = WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, 1000L, WAIT_EVENT_LOGICAL_SYNC_STATE_CHANGE);

    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
    }
  }

  return false;
}

   
                                        
   
void
invalidate_syncing_table_states(Datum arg, int cacheid, uint32 hashvalue)
{
  table_states_valid = false;
}

   
                                                                     
           
   
                                                                      
                                                                            
                        
   
static void
process_syncing_tables_for_sync(XLogRecPtr current_lsn)
{
  Assert(IsTransactionState());

  SpinLockAcquire(&MyLogicalRepWorker->relmutex);

  if (MyLogicalRepWorker->relstate == SUBREL_STATE_CATCHUP && current_lsn >= MyLogicalRepWorker->relstate_lsn)
  {
    TimeLineID tli;

    MyLogicalRepWorker->relstate = SUBREL_STATE_SYNCDONE;
    MyLogicalRepWorker->relstate_lsn = current_lsn;

    SpinLockRelease(&MyLogicalRepWorker->relmutex);

    UpdateSubscriptionRelState(MyLogicalRepWorker->subid, MyLogicalRepWorker->relid, MyLogicalRepWorker->relstate, MyLogicalRepWorker->relstate_lsn);

    walrcv_endstreaming(LogRepWorkerWalRcvConn, &tli);
    finish_sync_worker();
  }
  else
  {
    SpinLockRelease(&MyLogicalRepWorker->relmutex);
  }
}

   
                                                                   
   
                                                                          
                                                            
                                                            
   
                                                                              
                                                                      
                                                                             
                                                                               
                                                                             
                                         
   
                                                                         
                                                                           
                                   
   
                                                                             
                                                
   
static void
process_syncing_tables_for_apply(XLogRecPtr current_lsn)
{
  struct tablesync_start_time_mapping
  {
    Oid relid;
    TimestampTz last_start_time;
  };
  static List *table_states = NIL;
  static HTAB *last_start_times = NULL;
  ListCell *lc;
  bool started_tx = false;

  Assert(!IsTransactionState());

                                                                        
  if (!table_states_valid)
  {
    MemoryContext oldctx;
    List *rstates;
    ListCell *lc;
    SubscriptionRelState *rstate;

                             
    list_free_deep(table_states);
    table_states = NIL;

    StartTransactionCommand();
    started_tx = true;

                                     
    rstates = GetSubscriptionNotReadyRelations(MySubscription->oid);

                                                                   
    oldctx = MemoryContextSwitchTo(CacheMemoryContext);
    foreach (lc, rstates)
    {
      rstate = palloc(sizeof(SubscriptionRelState));
      memcpy(rstate, lfirst(lc), sizeof(SubscriptionRelState));
      table_states = lappend(table_states, rstate);
    }
    MemoryContextSwitchTo(oldctx);

    table_states_valid = true;
  }

     
                                                                             
                                                                            
              
     
  if (table_states && !last_start_times)
  {
    HASHCTL ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(Oid);
    ctl.entrysize = sizeof(struct tablesync_start_time_mapping);
    last_start_times = hash_create("Logical replication table sync worker start times", 256, &ctl, HASH_ELEM | HASH_BLOBS);
  }

     
                                                                      
                                 
     
  else if (!table_states && last_start_times)
  {
    hash_destroy(last_start_times);
    last_start_times = NULL;
  }

     
                                                     
     
  foreach (lc, table_states)
  {
    SubscriptionRelState *rstate = (SubscriptionRelState *)lfirst(lc);

    if (rstate->state == SUBREL_STATE_SYNCDONE)
    {
         
                                                                      
                                                                        
                                            
         
      if (current_lsn >= rstate->lsn)
      {
        rstate->state = SUBREL_STATE_READY;
        rstate->lsn = current_lsn;
        if (!started_tx)
        {
          StartTransactionCommand();
          started_tx = true;
        }

        UpdateSubscriptionRelState(MyLogicalRepWorker->subid, rstate->relid, rstate->state, rstate->lsn);
      }
    }
    else
    {
      LogicalRepWorker *syncworker;

         
                                                   
         
      LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);

      syncworker = logicalrep_worker_find(MyLogicalRepWorker->subid, rstate->relid, false);

      if (syncworker)
      {
                                                     
        SpinLockAcquire(&syncworker->relmutex);
        rstate->state = syncworker->relstate;
        rstate->lsn = syncworker->relstate_lsn;
        if (rstate->state == SUBREL_STATE_SYNCWAIT)
        {
             
                                                                    
                              
             
          syncworker->relstate = SUBREL_STATE_CATCHUP;
          syncworker->relstate_lsn = Max(syncworker->relstate_lsn, current_lsn);
        }
        SpinLockRelease(&syncworker->relmutex);

                                                         
        if (rstate->state == SUBREL_STATE_SYNCWAIT)
        {
                                                                    
          if (syncworker->proc)
          {
            logicalrep_worker_wakeup_ptr(syncworker);
          }

                                              
          LWLockRelease(LogicalRepWorkerLock);

             
                                                                    
                                                   
             
          if (!started_tx)
          {
            StartTransactionCommand();
            started_tx = true;
          }

          wait_for_relation_state_change(rstate->relid, SUBREL_STATE_SYNCDONE);
        }
        else
        {
          LWLockRelease(LogicalRepWorkerLock);
        }
      }
      else
      {
           
                                                                
                                                                     
                     
           
        int nsyncworkers = logicalrep_sync_worker_count(MyLogicalRepWorker->subid);

                                            
        LWLockRelease(LogicalRepWorkerLock);

           
                                                                   
                                 
           
        if (nsyncworkers < max_sync_workers_per_subscription)
        {
          TimestampTz now = GetCurrentTimestamp();
          struct tablesync_start_time_mapping *hentry;
          bool found;

          hentry = hash_search(last_start_times, &rstate->relid, HASH_ENTER, &found);

          if (!found || TimestampDifferenceExceeds(hentry->last_start_time, now, wal_retrieve_retry_interval))
          {
            logicalrep_worker_launch(MyLogicalRepWorker->dbid, MySubscription->oid, MySubscription->name, MyLogicalRepWorker->userid, rstate->relid);
            hentry->last_start_time = now;
          }
        }
      }
    }
  }

  if (started_tx)
  {
    CommitTransactionCommand();
    pgstat_report_stat(false);
  }
}

   
                                                                           
   
void
process_syncing_tables(XLogRecPtr current_lsn)
{
  if (am_tablesync_worker())
  {
    process_syncing_tables_for_sync(current_lsn);
  }
  else
  {
    process_syncing_tables_for_apply(current_lsn);
  }
}

   
                                                                      
   
static List *
make_copy_attnamelist(LogicalRepRelMapEntry *rel)
{
  List *attnamelist = NIL;
  int i;

  for (i = 0; i < rel->remoterel.natts; i++)
  {
    attnamelist = lappend(attnamelist, makeString(rel->remoterel.attnames[i]));
  }

  return attnamelist;
}

   
                                                                       
                                                          
   
static int
copy_read_data(void *outbuf, int minread, int maxread)
{
  int bytesread = 0;
  int avail;

                                                                   
  avail = copybuf->len - copybuf->cursor;
  if (avail)
  {
    if (avail > maxread)
    {
      avail = maxread;
    }
    memcpy(outbuf, &copybuf->data[copybuf->cursor], avail);
    copybuf->cursor += avail;
    maxread -= avail;
    bytesread += avail;
  }

  while (maxread > 0 && bytesread < minread)
  {
    pgsocket fd = PGINVALID_SOCKET;
    int len;
    char *buf = NULL;

    for (;;)
    {
                              
      len = walrcv_receive(LogRepWorkerWalRcvConn, &buf, &fd);

      CHECK_FOR_INTERRUPTS();

      if (len == 0)
      {
        break;
      }
      else if (len < 0)
      {
        return bytesread;
      }
      else
      {
                              
        copybuf->data = buf;
        copybuf->len = len;
        copybuf->cursor = 0;

        avail = copybuf->len - copybuf->cursor;
        if (avail > maxread)
        {
          avail = maxread;
        }
        memcpy(outbuf, &copybuf->data[copybuf->cursor], avail);
        outbuf = (void *)((char *)outbuf + avail);
        copybuf->cursor += avail;
        maxread -= avail;
        bytesread += avail;
      }

      if (maxread <= 0 || bytesread >= minread)
      {
        return bytesread;
      }
    }

       
                                    
       
    (void)WaitLatchOrSocket(MyLatch, WL_SOCKET_READABLE | WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH, fd, 1000L, WAIT_EVENT_LOGICAL_SYNC_DATA);

    ResetLatch(MyLatch);
  }

  return bytesread;
}

   
                                                                         
                                        
   
static void
fetch_remote_table_info(char *nspname, char *relname, LogicalRepRelation *lrel)
{
  WalRcvExecResult *res;
  StringInfoData cmd;
  TupleTableSlot *slot;
  Oid tableRow[3] = {OIDOID, CHAROID, CHAROID};
  Oid attrRow[4] = {TEXTOID, OIDOID, INT4OID, BOOLOID};
  bool isnull;
  char relkind;
  int natt;

  lrel->nspname = nspname;
  lrel->relname = relname;

                                             
  initStringInfo(&cmd);
  appendStringInfo(&cmd,
      "SELECT c.oid, c.relreplident, c.relkind"
      "  FROM pg_catalog.pg_class c"
      "  INNER JOIN pg_catalog.pg_namespace n"
      "        ON (c.relnamespace = n.oid)"
      " WHERE n.nspname = %s"
      "   AND c.relname = %s",
      quote_literal_cstr(nspname), quote_literal_cstr(relname));
  res = walrcv_exec(LogRepWorkerWalRcvConn, cmd.data, lengthof(tableRow), tableRow);

  if (res->status != WALRCV_OK_TUPLES)
  {
    ereport(ERROR, (errmsg("could not fetch table info for table \"%s.%s\" from publisher: %s", nspname, relname, res->err)));
  }

  slot = MakeSingleTupleTableSlot(res->tupledesc, &TTSOpsMinimalTuple);
  if (!tuplestore_gettupleslot(res->tuplestore, true, false, slot))
  {
    ereport(ERROR, (errmsg("table \"%s.%s\" not found on publisher", nspname, relname)));
  }

  lrel->remoteid = DatumGetObjectId(slot_getattr(slot, 1, &isnull));
  Assert(!isnull);
  lrel->replident = DatumGetChar(slot_getattr(slot, 2, &isnull));
  Assert(!isnull);
  relkind = DatumGetChar(slot_getattr(slot, 3, &isnull));
  Assert(!isnull);

     
                                                                          
                                                                             
                             
     
  if (relkind != RELKIND_RELATION)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("logical replication source relation \"%s.%s\" is not a table", nspname, relname)));
  }

  ExecDropSingleTupleTableSlot(slot);
  walrcv_clear_result(res);

                          
  resetStringInfo(&cmd);
  appendStringInfo(&cmd,
      "SELECT a.attname,"
      "       a.atttypid,"
      "       a.atttypmod,"
      "       a.attnum = ANY(i.indkey)"
      "  FROM pg_catalog.pg_attribute a"
      "  LEFT JOIN pg_catalog.pg_index i"
      "       ON (i.indexrelid = pg_get_replica_identity_index(%u))"
      " WHERE a.attnum > 0::pg_catalog.int2"
      "   AND NOT a.attisdropped %s"
      "   AND a.attrelid = %u"
      " ORDER BY a.attnum",
      lrel->remoteid, (walrcv_server_version(LogRepWorkerWalRcvConn) >= 120000 ? "AND a.attgenerated = ''" : ""), lrel->remoteid);
  res = walrcv_exec(LogRepWorkerWalRcvConn, cmd.data, lengthof(attrRow), attrRow);

  if (res->status != WALRCV_OK_TUPLES)
  {
    ereport(ERROR, (errmsg("could not fetch table info for table \"%s.%s\": %s", nspname, relname, res->err)));
  }

                                                                          
  lrel->attnames = palloc0(MaxTupleAttributeNumber * sizeof(char *));
  lrel->atttyps = palloc0(MaxTupleAttributeNumber * sizeof(Oid));
  lrel->attkeys = NULL;

  natt = 0;
  slot = MakeSingleTupleTableSlot(res->tupledesc, &TTSOpsMinimalTuple);
  while (tuplestore_gettupleslot(res->tuplestore, true, false, slot))
  {
    lrel->attnames[natt] = TextDatumGetCString(slot_getattr(slot, 1, &isnull));
    Assert(!isnull);
    lrel->atttyps[natt] = DatumGetObjectId(slot_getattr(slot, 2, &isnull));
    Assert(!isnull);
    if (DatumGetBool(slot_getattr(slot, 4, &isnull)))
    {
      lrel->attkeys = bms_add_member(lrel->attkeys, natt);
    }

                              
    if (++natt >= MaxTupleAttributeNumber)
    {
      elog(ERROR, "too many columns in remote table \"%s.%s\"", nspname, relname);
    }

    ExecClearTuple(slot);
  }
  ExecDropSingleTupleTableSlot(slot);

  lrel->natts = natt;

  walrcv_clear_result(res);
  pfree(cmd.data);
}

   
                                                 
   
                                                         
   
static void
copy_table(Relation rel)
{
  LogicalRepRelMapEntry *relmapentry;
  LogicalRepRelation lrel;
  WalRcvExecResult *res;
  StringInfoData cmd;
  CopyState cstate;
  List *attnamelist;
  ParseState *pstate;

                                        
  fetch_remote_table_info(get_namespace_name(RelationGetNamespace(rel)), RelationGetRelationName(rel), &lrel);

                                     
  logicalrep_relmap_update(&lrel);

                                                
  relmapentry = logicalrep_rel_open(lrel.remoteid, NoLock);
  Assert(rel == relmapentry->localrel);

                                    
  initStringInfo(&cmd);
  appendStringInfo(&cmd, "COPY %s TO STDOUT", quote_qualified_identifier(lrel.nspname, lrel.relname));
  res = walrcv_exec(LogRepWorkerWalRcvConn, cmd.data, 0, NULL);
  pfree(cmd.data);
  if (res->status != WALRCV_OK_COPY_OUT)
  {
    ereport(ERROR, (errmsg("could not start initial contents copy for table \"%s.%s\": %s", lrel.nspname, lrel.relname, res->err)));
  }
  walrcv_clear_result(res);

  copybuf = makeStringInfo();

  pstate = make_parsestate(NULL);
  addRangeTableEntryForRelation(pstate, rel, AccessShareLock, NULL, false, false);

  attnamelist = make_copy_attnamelist(relmapentry);
  cstate = BeginCopyFrom(pstate, rel, NULL, false, copy_read_data, attnamelist, NIL);

                   
  (void)CopyFrom(cstate);

  logicalrep_rel_close(relmapentry, NoLock);
}

   
                                               
   
                                                                  
   
char *
LogicalRepSyncTableStart(XLogRecPtr *origin_startpos)
{
  char *slotname;
  char *err;
  char relstate;
  XLogRecPtr relstate_lsn;

                                                     
  StartTransactionCommand();
  relstate = GetSubscriptionRelState(MyLogicalRepWorker->subid, MyLogicalRepWorker->relid, &relstate_lsn, true);
  CommitTransactionCommand();

  SpinLockAcquire(&MyLogicalRepWorker->relmutex);
  MyLogicalRepWorker->relstate = relstate;
  MyLogicalRepWorker->relstate_lsn = relstate_lsn;
  SpinLockRelease(&MyLogicalRepWorker->relmutex);

     
                                                                             
                                                                            
                                                                          
                                                                            
                                       
     
  StaticAssertStmt(NAMEDATALEN >= 32, "NAMEDATALEN too small");                 
  slotname = psprintf("%.*s_%u_sync_%u", NAMEDATALEN - 28, MySubscription->slotname, MySubscription->oid, MyLogicalRepWorker->relid);

     
                                                                       
                                                                           
                                                           
     
  LogRepWorkerWalRcvConn = walrcv_connect(MySubscription->conninfo, true, slotname, &err);
  if (LogRepWorkerWalRcvConn == NULL)
  {
    ereport(ERROR, (errmsg("could not connect to the publisher: %s", err)));
  }

  switch (MyLogicalRepWorker->relstate)
  {
  case SUBREL_STATE_INIT:
  case SUBREL_STATE_DATASYNC:
  {
    Relation rel;
    WalRcvExecResult *res;

    SpinLockAcquire(&MyLogicalRepWorker->relmutex);
    MyLogicalRepWorker->relstate = SUBREL_STATE_DATASYNC;
    MyLogicalRepWorker->relstate_lsn = InvalidXLogRecPtr;
    SpinLockRelease(&MyLogicalRepWorker->relmutex);

                                                         
    StartTransactionCommand();
    UpdateSubscriptionRelState(MyLogicalRepWorker->subid, MyLogicalRepWorker->relid, MyLogicalRepWorker->relstate, MyLogicalRepWorker->relstate_lsn);
    CommitTransactionCommand();
    pgstat_report_stat(false);

       
                                                                  
       
    StartTransactionCommand();

       
                                                             
                                                                   
                                                              
                                                                   
                                                       
       
    rel = table_open(MyLogicalRepWorker->relid, RowExclusiveLock);

       
                                                                
                                                                   
                                         
       
    res = walrcv_exec(LogRepWorkerWalRcvConn,
        "BEGIN READ ONLY ISOLATION LEVEL "
        "REPEATABLE READ",
        0, NULL);
    if (res->status != WALRCV_OK_COMMAND)
    {
      ereport(ERROR, (errmsg("table copy could not start transaction on publisher"), errdetail("The error was: %s", res->err)));
    }
    walrcv_clear_result(res);

       
                                                   
       
                                                                 
                                                                 
                                                                 
                 
       
    walrcv_create_slot(LogRepWorkerWalRcvConn, slotname, true, CRS_USE_SNAPSHOT, origin_startpos);

    PushActiveSnapshot(GetTransactionSnapshot());
    copy_table(rel);
    PopActiveSnapshot();

    res = walrcv_exec(LogRepWorkerWalRcvConn, "COMMIT", 0, NULL);
    if (res->status != WALRCV_OK_COMMAND)
    {
      ereport(ERROR, (errmsg("table copy could not finish transaction on publisher"), errdetail("The error was: %s", res->err)));
    }
    walrcv_clear_result(res);

    table_close(rel, NoLock);

                                
    CommandCounterIncrement();

       
                                                                 
                  
       
    SpinLockAcquire(&MyLogicalRepWorker->relmutex);
    MyLogicalRepWorker->relstate = SUBREL_STATE_SYNCWAIT;
    MyLogicalRepWorker->relstate_lsn = *origin_startpos;
    SpinLockRelease(&MyLogicalRepWorker->relmutex);

                                                           
    wait_for_worker_state_change(SUBREL_STATE_CATCHUP);

                 
                                               
                                                                   
                                                               
                                                                
                                                     
                                                                
                                           
                 
       
    if (*origin_startpos >= MyLogicalRepWorker->relstate_lsn)
    {
         
                                                             
                                                          
         
      UpdateSubscriptionRelState(MyLogicalRepWorker->subid, MyLogicalRepWorker->relid, SUBREL_STATE_SYNCDONE, *origin_startpos);
      finish_sync_worker();
    }
    break;
  }
  case SUBREL_STATE_SYNCDONE:
  case SUBREL_STATE_READY:
  case SUBREL_STATE_UNKNOWN:

       
                                                                       
                                                                     
               
       
    finish_sync_worker();
    break;
  default:
    elog(ERROR, "unknown relation state \"%c\"", MyLogicalRepWorker->relstate);
  }

  return slotname;
}
