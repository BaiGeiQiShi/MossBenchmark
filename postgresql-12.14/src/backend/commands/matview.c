                                                                            
   
             
                               
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "commands/cluster.h"
#include "commands/matview.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "miscadmin.h"
#include "parser/parse_relation.h"
#include "pgstat.h"
#include "rewrite/rewriteHandler.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

typedef struct
{
  DestReceiver pub;                                       
  Oid transientoid;                                          
                                                        
  Relation transientrel;                             
  CommandId output_cid;                                         
  int ti_options;                                                      
  BulkInsertState bistate;                        
} DR_transientrel;

static int matview_maintenance_depth = 0;

static void
transientrel_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static bool
transientrel_receive(TupleTableSlot *slot, DestReceiver *self);
static void
transientrel_shutdown(DestReceiver *self);
static void
transientrel_destroy(DestReceiver *self);
static uint64
refresh_matview_datafill(DestReceiver *dest, Query *query, const char *queryString);
static char *
make_temptable_name_n(char *tempname, int n);
static void
refresh_by_match_merge(Oid matviewOid, Oid tempOid, Oid relowner, int save_sec_context);
static void
refresh_by_heap_swap(Oid matviewOid, Oid OIDNewHeap, char relpersistence);
static bool
is_usable_unique_index(Relation indexRel);
static void
OpenMatViewIncrementalMaintenance(void);
static void
CloseMatViewIncrementalMaintenance(void);

   
                            
                                                   
   
                                                                     
   
void
SetMatViewPopulatedState(Relation relation, bool newstate)
{
  Relation pgrel;
  HeapTuple tuple;

  Assert(relation->rd_rel->relkind == RELKIND_MATVIEW);

     
                                                                            
                                                                           
              
     
  pgrel = table_open(RelationRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(RelationGetRelid(relation)));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", RelationGetRelid(relation));
  }

  ((Form_pg_class)GETSTRUCT(tuple))->relispopulated = newstate;

  CatalogTupleUpdate(pgrel, &tuple->t_self, tuple);

  heap_freetuple(tuple);
  table_close(pgrel, RowExclusiveLock);

     
                                                                      
              
     
  CommandCounterIncrement();
}

   
                                                                     
   
                                                                             
                                                                               
                                                                             
                                             
   
                                                                       
                                                                          
                                                                          
                                                     
   
                                                                               
                                                                                
                                
   
                                                                            
                                                            
   
ObjectAddress
ExecRefreshMatView(RefreshMatViewStmt *stmt, const char *queryString, ParamListInfo params, char *completionTag)
{
  Oid matviewOid;
  Relation matviewRel;
  RewriteRule *rule;
  List *actions;
  Query *dataQuery;
  Oid tableSpace;
  Oid relowner;
  Oid OIDNewHeap;
  DestReceiver *dest;
  uint64 processed = 0;
  bool concurrent;
  LOCKMODE lockmode;
  char relpersistence;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;
  ObjectAddress address;

                                          
  concurrent = stmt->concurrent;
  lockmode = concurrent ? ExclusiveLock : AccessExclusiveLock;

     
                                          
     
  matviewOid = RangeVarGetRelidExtended(stmt->relation, lockmode, 0, RangeVarCallbackOwnsTable, NULL);
  matviewRel = table_open(matviewOid, NoLock);
  relowner = matviewRel->rd_rel->relowner;

     
                                                                         
                                                                         
                                                      
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

                                            
  if (matviewRel->rd_rel->relkind != RELKIND_MATVIEW)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("\"%s\" is not a materialized view", RelationGetRelationName(matviewRel))));
  }

                                                                  
  if (concurrent && !RelationIsPopulated(matviewRel))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("CONCURRENTLY cannot be used when the materialized view is not populated")));
  }

                                                               
  if (concurrent && stmt->skipData)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("CONCURRENTLY and WITH NO DATA options cannot be used together")));
  }

     
                                                                            
                                                 
     
  if (matviewRel->rd_rel->relhasrules == false || matviewRel->rd_rules->numLocks < 1)
  {
    elog(ERROR, "materialized view \"%s\" is missing rewrite information", RelationGetRelationName(matviewRel));
  }

  if (matviewRel->rd_rules->numLocks > 1)
  {
    elog(ERROR, "materialized view \"%s\" has too many rules", RelationGetRelationName(matviewRel));
  }

  rule = matviewRel->rd_rules->rules[0];
  if (rule->event != CMD_SELECT || !(rule->isInstead))
  {
    elog(ERROR, "the rule for materialized view \"%s\" is not a SELECT INSTEAD OF rule", RelationGetRelationName(matviewRel));
  }

  actions = rule->actions;
  if (list_length(actions) != 1)
  {
    elog(ERROR, "the rule for materialized view \"%s\" is not a single action", RelationGetRelationName(matviewRel));
  }

     
                                                                            
                                                                    
     
  if (concurrent)
  {
    List *indexoidlist = RelationGetIndexList(matviewRel);
    ListCell *indexoidscan;
    bool hasUniqueIndex = false;

    foreach (indexoidscan, indexoidlist)
    {
      Oid indexoid = lfirst_oid(indexoidscan);
      Relation indexRel;

      indexRel = index_open(indexoid, AccessShareLock);
      hasUniqueIndex = is_usable_unique_index(indexRel);
      index_close(indexRel, AccessShareLock);
      if (hasUniqueIndex)
      {
        break;
      }
    }

    list_free(indexoidlist);

    if (!hasUniqueIndex)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot refresh materialized view \"%s\" concurrently", quote_qualified_identifier(get_namespace_name(RelationGetNamespace(matviewRel)), RelationGetRelationName(matviewRel))), errhint("Create a unique index with no WHERE clause on one or more columns of the materialized view.")));
    }
  }

     
                                                                          
                                               
     
  dataQuery = linitial_node(Query, actions);

     
                                                                            
                    
     
                                                                             
                                     
     
  CheckTableNotInUse(matviewRel, "REFRESH MATERIALIZED VIEW");

     
                                                                           
                        
     
  SetMatViewPopulatedState(matviewRel, !stmt->skipData);

                                                                             
  if (concurrent)
  {
    tableSpace = GetDefaultTablespace(RELPERSISTENCE_TEMP, false);
    relpersistence = RELPERSISTENCE_TEMP;
  }
  else
  {
    tableSpace = matviewRel->rd_rel->reltablespace;
    relpersistence = matviewRel->rd_rel->relpersistence;
  }

     
                                                                             
                                                                           
                    
     
  OIDNewHeap = make_new_heap(matviewOid, tableSpace, relpersistence, ExclusiveLock);
  LockRelationOid(OIDNewHeap, AccessExclusiveLock);
  dest = CreateTransientRelDestReceiver(OIDNewHeap);

                                     
  if (!stmt->skipData)
  {
    processed = refresh_matview_datafill(dest, dataQuery, queryString);
  }

                                                        
  if (concurrent)
  {
    int old_depth = matview_maintenance_depth;

    PG_TRY();
    {
      refresh_by_match_merge(matviewOid, OIDNewHeap, relowner, save_sec_context);
    }
    PG_CATCH();
    {
      matview_maintenance_depth = old_depth;
      PG_RE_THROW();
    }
    PG_END_TRY();
    Assert(matview_maintenance_depth == old_depth);
  }
  else
  {
    refresh_by_heap_swap(matviewOid, OIDNewHeap, relpersistence);

       
                                                                          
                                                                          
                                                                      
                                                           
       
    pgstat_count_truncate(matviewRel);
    if (!stmt->skipData)
    {
      pgstat_count_heap_insert(matviewRel, processed);
    }
  }

  table_close(matviewRel, NoLock);

                                 
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

  ObjectAddressSet(address, RelationRelationId, matviewOid);

  return address;
}

   
                            
   
                                                                      
                                         
   
                                    
   
static uint64
refresh_matview_datafill(DestReceiver *dest, Query *query, const char *queryString)
{
  List *rewritten;
  PlannedStmt *plan;
  QueryDesc *queryDesc;
  Query *copied_query;
  uint64 processed;

                                                                      
  copied_query = copyObject(query);
  AcquireRewriteLocks(copied_query, true, false);
  rewritten = QueryRewrite(copied_query);

                                                                         
  if (list_length(rewritten) != 1)
  {
    elog(ERROR, "unexpected rewrite result for REFRESH MATERIALIZED VIEW");
  }
  query = (Query *)linitial(rewritten);

                                       
  CHECK_FOR_INTERRUPTS();

                                                                
  plan = pg_plan_query(query, 0, NULL);

     
                                                                         
                                                                             
                                                                        
                                                            
     
  PushCopiedSnapshot(GetActiveSnapshot());
  UpdateActiveSnapshotCommandId();

                                                                    
  queryDesc = CreateQueryDesc(plan, queryString, GetActiveSnapshot(), InvalidSnapshot, dest, NULL, NULL, 0);

                                                            
  ExecutorStart(queryDesc, 0);

                    
  ExecutorRun(queryDesc, ForwardScanDirection, 0L, true);

  processed = queryDesc->estate->es_processed;

                    
  ExecutorFinish(queryDesc);
  ExecutorEnd(queryDesc);

  FreeQueryDesc(queryDesc);

  PopActiveSnapshot();

  return processed;
}

DestReceiver *
CreateTransientRelDestReceiver(Oid transientoid)
{
  DR_transientrel *self = (DR_transientrel *)palloc0(sizeof(DR_transientrel));

  self->pub.receiveSlot = transientrel_receive;
  self->pub.rStartup = transientrel_startup;
  self->pub.rShutdown = transientrel_shutdown;
  self->pub.rDestroy = transientrel_destroy;
  self->pub.mydest = DestTransientRel;
  self->transientoid = transientoid;

  return (DestReceiver *)self;
}

   
                                             
   
static void
transientrel_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
  DR_transientrel *myState = (DR_transientrel *)self;
  Relation transientrel;

  transientrel = table_open(myState->transientoid, NoLock);

     
                                                              
     
  myState->transientrel = transientrel;
  myState->output_cid = GetCurrentCommandId(true);

     
                                                                      
                                                             
     
  myState->ti_options = TABLE_INSERT_SKIP_FSM | TABLE_INSERT_FROZEN;
  if (!XLogIsNeeded())
  {
    myState->ti_options |= TABLE_INSERT_SKIP_WAL;
  }
  myState->bistate = GetBulkInsertState();

                                                                  
  Assert(RelationGetTargetBlock(transientrel) == InvalidBlockNumber);
}

   
                                              
   
static bool
transientrel_receive(TupleTableSlot *slot, DestReceiver *self)
{
  DR_transientrel *myState = (DR_transientrel *)self;

     
                                                                     
                                                                           
                                                                        
                                                                        
                                                                        
                                                       
     

  table_tuple_insert(myState->transientrel, slot, myState->output_cid, myState->ti_options, myState->bistate);

                                                                         

  return true;
}

   
                                          
   
static void
transientrel_shutdown(DestReceiver *self)
{
  DR_transientrel *myState = (DR_transientrel *)self;

  FreeBulkInsertState(myState->bistate);

  table_finish_bulk_insert(myState->transientrel, myState->ti_options);

                                                      
  table_close(myState->transientrel, NoLock);
  myState->transientrel = NULL;
}

   
                                                        
   
static void
transientrel_destroy(DestReceiver *self)
{
  pfree(self);
}

   
                                                                            
                                                                     
                                    
   
                                                                              
                                                                          
                                                                     
                                                               
   
static char *
make_temptable_name_n(char *tempname, int n)
{
  StringInfoData namebuf;

  initStringInfo(&namebuf);
  appendStringInfoString(&namebuf, tempname);
  appendStringInfo(&namebuf, "_%d", n);
  return namebuf.data;
}

   
                          
   
                                                                            
                     
   
                                                                        
                                                                              
                                                                               
                                                                              
                                                                               
                                                                             
                                                                         
                                                                            
                                                                          
                                                                          
   
                                                                              
                                                                          
                                               
   
                                                                       
                                                                        
           
   
                                                                              
                                                                            
                                                                           
                                                                              
                                                                             
                 
   
static void
refresh_by_match_merge(Oid matviewOid, Oid tempOid, Oid relowner, int save_sec_context)
{
  StringInfoData querybuf;
  Relation matviewRel;
  Relation tempRel;
  char *matviewname;
  char *tempname;
  char *diffname;
  TupleDesc tupdesc;
  bool foundUniqueIndex;
  List *indexoidlist;
  ListCell *indexoidscan;
  int16 relnatts;
  Oid *opUsedForQual;

  initStringInfo(&querybuf);
  matviewRel = table_open(matviewOid, NoLock);
  matviewname = quote_qualified_identifier(get_namespace_name(RelationGetNamespace(matviewRel)), RelationGetRelationName(matviewRel));
  tempRel = table_open(tempOid, NoLock);
  tempname = quote_qualified_identifier(get_namespace_name(RelationGetNamespace(tempRel)), RelationGetRelationName(tempRel));
  diffname = make_temptable_name_n(tempname, 2);

  relnatts = RelationGetNumberOfAttributes(matviewRel);

                         
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

                                                     
  appendStringInfo(&querybuf, "ANALYZE %s", tempname);
  if (SPI_exec(querybuf.data, 0) != SPI_OK_UTILITY)
  {
    elog(ERROR, "SPI_exec failed: %s", querybuf.data);
  }

     
                                                                          
                                                                            
                                                                             
                                                                          
                                   
     
                                                                           
                                                                           
                                           
     
  resetStringInfo(&querybuf);
  appendStringInfo(&querybuf,
      "SELECT newdata.*::%s FROM %s newdata "
      "WHERE newdata.* IS NOT NULL AND EXISTS "
      "(SELECT 1 FROM %s newdata2 WHERE newdata2.* IS NOT NULL "
      "AND newdata2.* OPERATOR(pg_catalog.*=) newdata.* "
      "AND newdata2.ctid OPERATOR(pg_catalog.<>) "
      "newdata.ctid)",
      tempname, tempname, tempname);
  if (SPI_execute(querybuf.data, false, 1) != SPI_OK_SELECT)
  {
    elog(ERROR, "SPI_exec failed: %s", querybuf.data);
  }
  if (SPI_processed > 0)
  {
       
                                                                           
                                                                           
                                                                           
                                                                        
                                                         
       
    ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION), errmsg("new data for materialized view \"%s\" contains duplicate rows without any null columns", RelationGetRelationName(matviewRel)), errdetail("Row: %s", SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1))));
  }

  SetUserIdAndSecContext(relowner, save_sec_context | SECURITY_LOCAL_USERID_CHANGE);

                                                             
  resetStringInfo(&querybuf);
  appendStringInfo(&querybuf,
      "CREATE TEMP TABLE %s AS "
      "SELECT mv.ctid AS tid, newdata.*::%s AS newdata "
      "FROM %s mv FULL JOIN %s newdata ON (",
      diffname, tempname, matviewname, tempname);

     
                                                                             
                                                                          
                                                                            
                       
     
  tupdesc = matviewRel->rd_att;
  opUsedForQual = (Oid *)palloc0(sizeof(Oid) * relnatts);
  foundUniqueIndex = false;

  indexoidlist = RelationGetIndexList(matviewRel);

  foreach (indexoidscan, indexoidlist)
  {
    Oid indexoid = lfirst_oid(indexoidscan);
    Relation indexRel;

    indexRel = index_open(indexoid, RowExclusiveLock);
    if (is_usable_unique_index(indexRel))
    {
      Form_pg_index indexStruct = indexRel->rd_index;
      int indnkeyatts = indexStruct->indnkeyatts;
      oidvector *indclass;
      Datum indclassDatum;
      bool isnull;
      int i;

                                           
      indclassDatum = SysCacheGetAttr(INDEXRELID, indexRel->rd_indextuple, Anum_pg_index_indclass, &isnull);
      Assert(!isnull);
      indclass = (oidvector *)DatumGetPointer(indclassDatum);

                                                      
      for (i = 0; i < indnkeyatts; i++)
      {
        int attnum = indexStruct->indkey.values[i];
        Oid opclass = indclass->values[i];
        Form_pg_attribute attr = TupleDescAttr(tupdesc, attnum - 1);
        Oid attrtype = attr->atttypid;
        HeapTuple cla_ht;
        Form_pg_opclass cla_tup;
        Oid opfamily;
        Oid opcintype;
        Oid op;
        const char *leftop;
        const char *rightop;

           
                                                                     
                                                                   
           
        cla_ht = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
        if (!HeapTupleIsValid(cla_ht))
        {
          elog(ERROR, "cache lookup failed for opclass %u", opclass);
        }
        cla_tup = (Form_pg_opclass)GETSTRUCT(cla_ht);
        Assert(cla_tup->opcmethod == BTREE_AM_OID);
        opfamily = cla_tup->opcfamily;
        opcintype = cla_tup->opcintype;
        ReleaseSysCache(cla_ht);

        op = get_opfamily_member(opfamily, opcintype, opcintype, BTEqualStrategyNumber);
        if (!OidIsValid(op))
        {
          elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", BTEqualStrategyNumber, opcintype, opcintype, opfamily);
        }

           
                                                                       
                                                                     
                        
           
                                                                   
                                                                      
                                                                     
                                                                   
                          
           
        if (opUsedForQual[attnum - 1] == op)
        {
          continue;
        }
        opUsedForQual[attnum - 1] = op;

           
                                                         
           
        if (foundUniqueIndex)
        {
          appendStringInfoString(&querybuf, " AND ");
        }

        leftop = quote_qualified_identifier("newdata", NameStr(attr->attname));
        rightop = quote_qualified_identifier("mv", NameStr(attr->attname));

        generate_operator_clause(&querybuf, leftop, attrtype, op, rightop, attrtype);

        foundUniqueIndex = true;
      }
    }

                                                                        
    index_close(indexRel, NoLock);
  }

  list_free(indexoidlist);

     
                                                                    
     
                                                                             
                                                                       
                                                                          
     
  Assert(foundUniqueIndex);

  appendStringInfoString(&querybuf, " AND newdata.* OPERATOR(pg_catalog.*=) mv.*) "
                                    "WHERE newdata.* IS NULL OR mv.* IS NULL "
                                    "ORDER BY tid");

                                          
  if (SPI_exec(querybuf.data, 0) != SPI_OK_UTILITY)
  {
    elog(ERROR, "SPI_exec failed: %s", querybuf.data);
  }

  SetUserIdAndSecContext(relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);

     
                                                                             
                                                                             
     

                               
  resetStringInfo(&querybuf);
  appendStringInfo(&querybuf, "ANALYZE %s", diffname);
  if (SPI_exec(querybuf.data, 0) != SPI_OK_UTILITY)
  {
    elog(ERROR, "SPI_exec failed: %s", querybuf.data);
  }

  OpenMatViewIncrementalMaintenance();

                                                        
  resetStringInfo(&querybuf);
  appendStringInfo(&querybuf,
      "DELETE FROM %s mv WHERE ctid OPERATOR(pg_catalog.=) ANY "
      "(SELECT diff.tid FROM %s diff "
      "WHERE diff.tid IS NOT NULL "
      "AND diff.newdata IS NULL)",
      matviewname, diffname);
  if (SPI_exec(querybuf.data, 0) != SPI_OK_DELETE)
  {
    elog(ERROR, "SPI_exec failed: %s", querybuf.data);
  }

                        
  resetStringInfo(&querybuf);
  appendStringInfo(&querybuf,
      "INSERT INTO %s SELECT (diff.newdata).* "
      "FROM %s diff WHERE tid IS NULL",
      matviewname, diffname);
  if (SPI_exec(querybuf.data, 0) != SPI_OK_INSERT)
  {
    elog(ERROR, "SPI_exec failed: %s", querybuf.data);
  }

                                                     
  CloseMatViewIncrementalMaintenance();
  table_close(tempRel, NoLock);
  table_close(matviewRel, NoLock);

                             
  resetStringInfo(&querybuf);
  appendStringInfo(&querybuf, "DROP TABLE %s, %s", diffname, tempname);
  if (SPI_exec(querybuf.data, 0) != SPI_OK_UTILITY)
  {
    elog(ERROR, "SPI_exec failed: %s", querybuf.data);
  }

                          
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }
}

   
                                                                            
                                                                              
                                                                         
   
static void
refresh_by_heap_swap(Oid matviewOid, Oid OIDNewHeap, char relpersistence)
{
  finish_heap_swap(matviewOid, OIDNewHeap, false, false, true, true, RecentXmin, ReadNextMultiXactId(), relpersistence);
}

   
                                                            
   
static bool
is_usable_unique_index(Relation indexRel)
{
  Form_pg_index indexStruct = indexRel->rd_index;

     
                                                                        
                                                                       
                                                                            
                                                                           
                                                                            
                
     
  if (indexStruct->indisunique && indexStruct->indimmediate && indexRel->rd_rel->relam == BTREE_AM_OID && indexStruct->indisvalid && RelationGetIndexPredicate(indexRel) == NIL && indexStruct->indnatts > 0)
  {
       
                                                                           
                                                                     
                                                                         
                                                                          
                         
       
    int numatts = indexStruct->indnatts;
    int i;

    for (i = 0; i < numatts; i++)
    {
      int attnum = indexStruct->indkey.values[i];

      if (attnum <= 0)
      {
        return false;
      }
    }
    return true;
  }
  return false;
}

   
                                                                               
                                                                             
                                                                            
                                         
   
                                                                             
                                                                            
                                                                               
                                      
   
bool
MatViewIncrementalMaintenanceIsEnabled(void)
{
  return matview_maintenance_depth > 0;
}

static void
OpenMatViewIncrementalMaintenance(void)
{
  matview_maintenance_depth++;
}

static void
CloseMatViewIncrementalMaintenance(void)
{
  matview_maintenance_depth--;
  Assert(matview_maintenance_depth >= 0);
}
