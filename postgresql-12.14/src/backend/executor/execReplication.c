                                                                            
   
                     
                                                             
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

   
                                                                             
                                            
   
                                              
   
                                                                        
                                                                    
   
static bool
build_replindex_scan_key(ScanKey skey, Relation rel, Relation idxrel, TupleTableSlot *searchslot)
{
  int attoff;
  bool isnull;
  Datum indclassDatum;
  oidvector *opclass;
  int2vector *indkey = &idxrel->rd_index->indkey;
  bool hasnulls = false;

  Assert(RelationGetReplicaIndex(rel) == RelationGetRelid(idxrel) || RelationGetPrimaryKeyIndex(rel) == RelationGetRelid(idxrel));

  indclassDatum = SysCacheGetAttr(INDEXRELID, idxrel->rd_indextuple, Anum_pg_index_indclass, &isnull);
  Assert(!isnull);
  opclass = (oidvector *)DatumGetPointer(indclassDatum);

                                                       
  for (attoff = 0; attoff < IndexRelationGetNumberOfKeyAttributes(idxrel); attoff++)
  {
    Oid operator;
    Oid opfamily;
    RegProcedure regop;
    int pkattno = attoff + 1;
    int mainattno = indkey->values[attoff];
    Oid optype = get_opclass_input_type(opclass->values[attoff]);

       
                                                                          
                                  
       
    opfamily = get_opclass_family(opclass->values[attoff]);

    operator= get_opfamily_member(opfamily, optype, optype, BTEqualStrategyNumber);
    if (!OidIsValid(operator))
    {
      elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", BTEqualStrategyNumber, optype, optype, opfamily);
    }

    regop = get_opcode(operator);

                                 
    ScanKeyInit(&skey[attoff], pkattno, BTEqualStrategyNumber, regop, searchslot->tts_values[mainattno - 1]);

    skey[attoff].sk_collation = idxrel->rd_indcollation[attoff];

                               
    if (searchslot->tts_isnull[mainattno - 1])
    {
      hasnulls = true;
      skey[attoff].sk_flags |= SK_ISNULL;
    }
  }

  return hasnulls;
}

   
                                                        
   
                                                                               
                                                       
   
bool
RelationFindReplTupleByIndex(Relation rel, Oid idxoid, LockTupleMode lockmode, TupleTableSlot *searchslot, TupleTableSlot *outslot)
{
  ScanKeyData skey[INDEX_MAX_KEYS];
  IndexScanDesc scan;
  SnapshotData snap;
  TransactionId xwait;
  Relation idxrel;
  bool found;

                       
  idxrel = index_open(idxoid, RowExclusiveLock);

                            
  InitDirtySnapshot(snap);
  scan = index_beginscan(rel, idxrel, &snap, IndexRelationGetNumberOfKeyAttributes(idxrel), 0);

                       
  build_replindex_scan_key(skey, rel, idxrel, searchslot);

retry:
  found = false;

  index_rescan(scan, skey, IndexRelationGetNumberOfKeyAttributes(idxrel), NULL, 0);

                             
  if (index_getnext_slot(scan, ForwardScanDirection, outslot))
  {
    found = true;
    ExecMaterializeSlot(outslot);

    xwait = TransactionIdIsValid(snap.xmin) ? snap.xmin : snap.xmax;

       
                                                                          
              
       
    if (TransactionIdIsValid(xwait))
    {
      XactLockTableWait(xwait, NULL, NULL, XLTW_None);
      goto retry;
    }
  }

                                                    
  if (found)
  {
    TM_FailureData tmfd;
    TM_Result res;

    PushActiveSnapshot(GetLatestSnapshot());

    res = table_tuple_lock(rel, &(outslot->tts_tid), GetLatestSnapshot(), outslot, GetCurrentCommandId(false), lockmode, LockWaitBlock, 0                           , &tmfd);

    PopActiveSnapshot();

    switch (res)
    {
    case TM_Ok:
      break;
    case TM_Updated:
                                      
      if (ItemPointerIndicatesMovedPartitions(&tmfd.ctid))
      {
        ereport(LOG, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("tuple to be locked was already moved to another partition due to concurrent update, retrying")));
      }
      else
      {
        ereport(LOG, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("concurrent update, retrying")));
      }
      goto retry;
    case TM_Deleted:
                                      
      ereport(LOG, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("concurrent delete, retrying")));
      goto retry;
    case TM_Invisible:
      elog(ERROR, "attempted to lock invisible tuple");
      break;
    default:
      elog(ERROR, "unexpected table_tuple_lock status: %u", res);
      break;
    }
  }

  index_endscan(scan);

                                        
  index_close(idxrel, NoLock);

  return found;
}

   
                                                                          
   
static bool
tuples_equal(TupleTableSlot *slot1, TupleTableSlot *slot2)
{
  int attrnum;

  Assert(slot1->tts_tupleDescriptor->natts == slot2->tts_tupleDescriptor->natts);

  slot_getallattrs(slot1);
  slot_getallattrs(slot2);

                                         
  for (attrnum = 0; attrnum < slot1->tts_tupleDescriptor->natts; attrnum++)
  {
    Form_pg_attribute att;
    TypeCacheEntry *typentry;

       
                                                                          
             
       
    if (slot1->tts_isnull[attrnum] != slot2->tts_isnull[attrnum])
    {
      return false;
    }

       
                                                       
       
    if (slot1->tts_isnull[attrnum] || slot2->tts_isnull[attrnum])
    {
      continue;
    }

    att = TupleDescAttr(slot1->tts_tupleDescriptor, attrnum);

    typentry = lookup_type_cache(att->atttypid, TYPECACHE_EQ_OPR_FINFO);
    if (!OidIsValid(typentry->eq_opr_finfo.fn_oid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify an equality operator for type %s", format_type_be(att->atttypid))));
    }

    if (!DatumGetBool(FunctionCall2Coll(&typentry->eq_opr_finfo, att->attcollation, slot1->tts_values[attrnum], slot2->tts_values[attrnum])))
    {
      return false;
    }
  }

  return true;
}

   
                                                                  
   
                                                                               
                                                       
   
                                                     
   
                                                                            
   
bool
RelationFindReplTupleSeq(Relation rel, LockTupleMode lockmode, TupleTableSlot *searchslot, TupleTableSlot *outslot)
{
  TupleTableSlot *scanslot;
  TableScanDesc scan;
  SnapshotData snap;
  TransactionId xwait;
  bool found;
  TupleDesc desc PG_USED_FOR_ASSERTS_ONLY = RelationGetDescr(rel);

  Assert(equalTupleDescs(desc, outslot->tts_tupleDescriptor));

                          
  InitDirtySnapshot(snap);
  scan = table_beginscan(rel, &snap, 0, NULL);
  scanslot = table_slot_create(rel, NULL);

retry:
  found = false;

  table_rescan(scan, NULL);

                             
  while (table_scan_getnextslot(scan, ForwardScanDirection, scanslot))
  {
    if (!tuples_equal(scanslot, searchslot))
    {
      continue;
    }

    found = true;
    ExecCopySlot(outslot, scanslot);

    xwait = TransactionIdIsValid(snap.xmin) ? snap.xmin : snap.xmax;

       
                                                                          
              
       
    if (TransactionIdIsValid(xwait))
    {
      XactLockTableWait(xwait, NULL, NULL, XLTW_None);
      goto retry;
    }

                                             
    break;
  }

                                                    
  if (found)
  {
    TM_FailureData tmfd;
    TM_Result res;

    PushActiveSnapshot(GetLatestSnapshot());

    res = table_tuple_lock(rel, &(outslot->tts_tid), GetLatestSnapshot(), outslot, GetCurrentCommandId(false), lockmode, LockWaitBlock, 0                           , &tmfd);

    PopActiveSnapshot();

    switch (res)
    {
    case TM_Ok:
      break;
    case TM_Updated:
                                      
      if (ItemPointerIndicatesMovedPartitions(&tmfd.ctid))
      {
        ereport(LOG, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("tuple to be locked was already moved to another partition due to concurrent update, retrying")));
      }
      else
      {
        ereport(LOG, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("concurrent update, retrying")));
      }
      goto retry;
    case TM_Deleted:
                                      
      ereport(LOG, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("concurrent delete, retrying")));
      goto retry;
    case TM_Invisible:
      elog(ERROR, "attempted to lock invisible tuple");
      break;
    default:
      elog(ERROR, "unexpected table_tuple_lock status: %u", res);
      break;
    }
  }

  table_endscan(scan);
  ExecDropSingleTupleTableSlot(scanslot);

  return found;
}

   
                                                                             
                                                     
   
                                                  
   
void
ExecSimpleRelationInsert(EState *estate, TupleTableSlot *slot)
{
  bool skip_tuple = false;
  ResultRelInfo *resultRelInfo = estate->es_result_relation_info;
  Relation rel = resultRelInfo->ri_RelationDesc;

                                       
  Assert(rel->rd_rel->relkind == RELKIND_RELATION);

  CheckCmdReplicaIdentity(rel, CMD_INSERT);

                                  
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_before_row)
  {
    if (!ExecBRInsertTriggers(estate, resultRelInfo, slot))
    {
      skip_tuple = true;                   
    }
  }

  if (!skip_tuple)
  {
    List *recheckIndexes = NIL;

                                          
    if (rel->rd_att->constr && rel->rd_att->constr->has_generated_stored)
    {
      ExecComputeStoredGenerated(estate, slot);
    }

                                            
    if (rel->rd_att->constr)
    {
      ExecConstraints(resultRelInfo, slot, estate);
    }
    if (resultRelInfo->ri_PartitionCheck)
    {
      ExecPartitionCheck(resultRelInfo, slot, estate, true);
    }

                                                             
    simple_table_tuple_insert(resultRelInfo->ri_RelationDesc, slot);

    if (resultRelInfo->ri_NumIndices > 0)
    {
      recheckIndexes = ExecInsertIndexTuples(slot, estate, false, NULL, NIL);
    }

                                   
    ExecARInsertTriggers(estate, resultRelInfo, slot, recheckIndexes, NULL);

       
                                                                           
                                                                        
                                                          
       

    list_free(recheckIndexes);
  }
}

   
                                                                  
                                                                         
   
                                                  
   
void
ExecSimpleRelationUpdate(EState *estate, EPQState *epqstate, TupleTableSlot *searchslot, TupleTableSlot *slot)
{
  bool skip_tuple = false;
  ResultRelInfo *resultRelInfo = estate->es_result_relation_info;
  Relation rel = resultRelInfo->ri_RelationDesc;
  ItemPointer tid = &(searchslot->tts_tid);

                                       
  Assert(rel->rd_rel->relkind == RELKIND_RELATION);

  CheckCmdReplicaIdentity(rel, CMD_UPDATE);

                                  
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_update_before_row)
  {
    if (!ExecBRUpdateTriggers(estate, epqstate, resultRelInfo, tid, NULL, slot))
    {
      skip_tuple = true;                   
    }
  }

  if (!skip_tuple)
  {
    List *recheckIndexes = NIL;
    bool update_indexes;

                                          
    if (rel->rd_att->constr && rel->rd_att->constr->has_generated_stored)
    {
      ExecComputeStoredGenerated(estate, slot);
    }

                                            
    if (rel->rd_att->constr)
    {
      ExecConstraints(resultRelInfo, slot, estate);
    }
    if (resultRelInfo->ri_PartitionCheck)
    {
      ExecPartitionCheck(resultRelInfo, slot, estate, true);
    }

    simple_table_tuple_update(rel, tid, slot, estate->es_snapshot, &update_indexes);

    if (resultRelInfo->ri_NumIndices > 0 && update_indexes)
    {
      recheckIndexes = ExecInsertIndexTuples(slot, estate, false, NULL, NIL);
    }

                                   
    ExecARUpdateTriggers(estate, resultRelInfo, tid, NULL, slot, recheckIndexes, NULL);

    list_free(recheckIndexes);
  }
}

   
                                                                        
                         
   
                                                  
   
void
ExecSimpleRelationDelete(EState *estate, EPQState *epqstate, TupleTableSlot *searchslot)
{
  bool skip_tuple = false;
  ResultRelInfo *resultRelInfo = estate->es_result_relation_info;
  Relation rel = resultRelInfo->ri_RelationDesc;
  ItemPointer tid = &searchslot->tts_tid;

  CheckCmdReplicaIdentity(rel, CMD_DELETE);

                                  
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_delete_before_row)
  {
    skip_tuple = !ExecBRDeleteTriggers(estate, epqstate, resultRelInfo, tid, NULL, NULL);
  }

  if (!skip_tuple)
  {
                              
    simple_table_tuple_delete(rel, tid, estate->es_snapshot);

                                   
    ExecARDeleteTriggers(estate, resultRelInfo, tid, NULL, NULL);
  }
}

   
                                                                   
   
void
CheckCmdReplicaIdentity(Relation rel, CmdType cmd)
{
  PublicationActions *pubactions;

                                                        
  if (cmd != CMD_UPDATE && cmd != CMD_DELETE)
  {
    return;
  }

                                                            
  if (rel->rd_rel->relreplident == REPLICA_IDENTITY_FULL || OidIsValid(RelationGetReplicaIndex(rel)))
  {
    return;
  }

     
                                                                       
     
                                                      
     
  pubactions = GetRelationPublicationActions(rel);
  if (cmd == CMD_UPDATE && pubactions->pubupdate)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot update table \"%s\" because it does not have a replica identity and publishes updates", RelationGetRelationName(rel)), errhint("To enable updating the table, set REPLICA IDENTITY using ALTER TABLE.")));
  }
  else if (cmd == CMD_DELETE && pubactions->pubdelete)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot delete from table \"%s\" because it does not have a replica identity and publishes deletes", RelationGetRelationName(rel)), errhint("To enable deleting from the table, set REPLICA IDENTITY using ALTER TABLE.")));
  }
}

   
                                                      
   
                                                                
   
void
CheckSubscriptionRelkind(char relkind, const char *nspname, const char *relname)
{
     
                                                                           
                                                             
     
  if (relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot use relation \"%s.%s\" as logical replication target", nspname, relname), errdetail("\"%s.%s\" is a partitioned table.", nspname, relname)));
  }
  else if (relkind == RELKIND_FOREIGN_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot use relation \"%s.%s\" as logical replication target", nspname, relname), errdetail("\"%s.%s\" is a foreign table.", nspname, relname)));
  }

  if (relkind != RELKIND_RELATION)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot use relation \"%s.%s\" as logical replication target", nspname, relname), errdetail("\"%s.%s\" is not a table.", nspname, relname)));
  }
}
