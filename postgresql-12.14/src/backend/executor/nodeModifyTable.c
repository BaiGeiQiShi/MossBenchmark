                                                                            
   
                     
                                           
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
                      
                                                          
                                                             
                                                        
                                                        
   
          
                                                                   
                                                                         
                                                                         
                                                                     
                                                                   
                                                                      
                                                                          
                                   
   
                                                                     
                                                                         
                                                                           
                                                                  
                                                            
   

#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "commands/trigger.h"
#include "executor/execPartition.h"
#include "executor/executor.h"
#include "executor/nodeModifyTable.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "rewrite/rewriteHandler.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/memutils.h"
#include "utils/rel.h"

static bool
ExecOnConflictUpdate(ModifyTableState *mtstate, ResultRelInfo *resultRelInfo, ItemPointer conflictTid, TupleTableSlot *planSlot, TupleTableSlot *excludedSlot, EState *estate, bool canSetTag, TupleTableSlot **returning);
static TupleTableSlot *
ExecPrepareTupleRouting(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, ResultRelInfo *targetRelInfo, TupleTableSlot *slot);
static ResultRelInfo *
getTargetResultRelInfo(ModifyTableState *node);
static void
ExecSetupChildParentMapForSubplan(ModifyTableState *mtstate);
static TupleConversionMap *
tupconv_map_for_subplan(ModifyTableState *node, int whichplan);

   
                                                                       
                             
   
                                                                     
                                                                            
                                                                          
                                                                 
   
                                                                        
                                            
   
static void
ExecCheckPlanOutput(Relation resultRel, List *targetList)
{
  TupleDesc resultDesc = RelationGetDescr(resultRel);
  int attno = 0;
  ListCell *lc;

  foreach (lc, targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);
    Form_pg_attribute attr;

    if (tle->resjunk)
    {
      continue;                              
    }

    if (attno >= resultDesc->natts)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table row type and query-specified row type do not match"), errdetail("Query has too many columns.")));
    }
    attr = TupleDescAttr(resultDesc, attno);
    attno++;

    if (!attr->attisdropped)
    {
                                          
      if (exprType((Node *)tle->expr) != attr->atttypid)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table row type and query-specified row type do not match"), errdetail("Table has type %s at ordinal position %d, but query expects %s.", format_type_be(attr->atttypid), attno, format_type_be(exprType((Node *)tle->expr)))));
      }
    }
    else
    {
         
                                                                        
                                                                        
                                                         
         
      if (!IsA(tle->expr, Const) || !((Const *)tle->expr)->constisnull)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table row type and query-specified row type do not match"), errdetail("Query provides a value for a dropped column at ordinal position %d.", attno)));
      }
    }
  }
  if (attno != resultDesc->natts)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("table row type and query-specified row type do not match"), errdetail("Query has too few columns.")));
  }
}

   
                                                      
   
                                                
                                       
                                                                   
                                                             
   
                                                                         
                                                                      
                                                  
   
                                                                               
               
   
                                           
   
static TupleTableSlot *
ExecProcessReturning(ProjectionInfo *projectReturning, Oid resultRelOid, TupleTableSlot *tupleSlot, TupleTableSlot *planSlot)
{
  ExprContext *econtext = projectReturning->pi_exprContext;

                                                                         
  if (tupleSlot)
  {
    econtext->ecxt_scantuple = tupleSlot;
  }
  else
  {
    Assert(econtext->ecxt_scantuple);
  }
  econtext->ecxt_outertuple = planSlot;

     
                                                                           
                                                                     
     
  econtext->ecxt_scantuple->tts_tableOid = resultRelOid;

                                         
  return ExecProject(projectReturning);
}

   
                                                    
   
                                                                                
                                                                               
                                                                             
                                                                                
   
static void
ExecCheckTupleVisible(EState *estate, Relation rel, TupleTableSlot *slot)
{
  if (!IsolationUsesXactSnapshot())
  {
    return;
  }

  if (!table_tuple_satisfies_snapshot(rel, slot, estate->es_snapshot))
  {
    Datum xminDatum;
    TransactionId xmin;
    bool isnull;

    xminDatum = slot_getsysattr(slot, MinTransactionIdAttributeNumber, &isnull);
    Assert(!isnull);
    xmin = DatumGetTransactionId(xminDatum);

       
                                                                      
                                                                         
                                                                     
                                                                         
       
    if (!TransactionIdIsCurrentTransactionId(xmin))
    {
      ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent update")));
    }
  }
}

   
                                                                         
   
static void
ExecCheckTIDVisible(EState *estate, ResultRelInfo *relinfo, ItemPointer tid, TupleTableSlot *tempSlot)
{
  Relation rel = relinfo->ri_RelationDesc;

                                         
  if (!IsolationUsesXactSnapshot())
  {
    return;
  }

  if (!table_tuple_fetch_row_version(rel, tid, SnapshotAny, tempSlot))
  {
    elog(ERROR, "failed to fetch conflicting tuple for ON CONFLICT");
  }
  ExecCheckTupleVisible(estate, rel, tempSlot);
  ExecClearTuple(tempSlot);
}

   
                                                
   
void
ExecComputeStoredGenerated(EState *estate, TupleTableSlot *slot)
{
  ResultRelInfo *resultRelInfo = estate->es_result_relation_info;
  Relation rel = resultRelInfo->ri_RelationDesc;
  TupleDesc tupdesc = RelationGetDescr(rel);
  int natts = tupdesc->natts;
  MemoryContext oldContext;
  Datum *values;
  bool *nulls;

  Assert(tupdesc->constr && tupdesc->constr->has_generated_stored);

     
                                                                      
                                                                          
                                                                       
     
  if (resultRelInfo->ri_GeneratedExprs == NULL)
  {
    oldContext = MemoryContextSwitchTo(estate->es_query_cxt);

    resultRelInfo->ri_GeneratedExprs = (ExprState **)palloc(natts * sizeof(ExprState *));

    for (int i = 0; i < natts; i++)
    {
      if (TupleDescAttr(tupdesc, i)->attgenerated == ATTRIBUTE_GENERATED_STORED)
      {
        Expr *expr;

        expr = (Expr *)build_column_default(rel, i + 1);
        if (expr == NULL)
        {
          elog(ERROR, "no generation expression found for column number %d of table \"%s\"", i + 1, RelationGetRelationName(rel));
        }

        resultRelInfo->ri_GeneratedExprs[i] = ExecPrepareExpr(expr, estate);
      }
    }

    MemoryContextSwitchTo(oldContext);
  }

  oldContext = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

  values = palloc(sizeof(*values) * natts);
  nulls = palloc(sizeof(*nulls) * natts);

  slot_getallattrs(slot);
  memcpy(nulls, slot->tts_isnull, sizeof(*nulls) * natts);

  for (int i = 0; i < natts; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

    if (attr->attgenerated == ATTRIBUTE_GENERATED_STORED)
    {
      ExprContext *econtext;
      Datum val;
      bool isnull;

      econtext = GetPerTupleExprContext(estate);
      econtext->ecxt_scantuple = slot;

      val = ExecEvalExpr(resultRelInfo->ri_GeneratedExprs[i], econtext, &isnull);

         
                                                                         
                                                          
         
      if (!isnull)
      {
        val = datumCopy(val, attr->attbyval, attr->attlen);
      }

      values[i] = val;
      nulls[i] = isnull;
    }
    else
    {
      if (!nulls[i])
      {
        values[i] = datumCopy(slot->tts_values[i], attr->attbyval, attr->attlen);
      }
    }
  }

  ExecClearTuple(slot);
  memcpy(slot->tts_values, values, sizeof(*values) * natts);
  memcpy(slot->tts_isnull, nulls, sizeof(*nulls) * natts);
  ExecStoreVirtualTuple(slot);
  ExecMaterializeSlot(slot);

  MemoryContextSwitchTo(oldContext);
}

                                                                    
               
   
                                                                     
                                                            
   
                                                    
                                                                   
                                                              
                                                                   
                                                                
   
                                                                       
                                                                        
                                                                       
   
                                                     
                                                                    
   
static TupleTableSlot *
ExecInsert(ModifyTableState *mtstate, TupleTableSlot *slot, TupleTableSlot *planSlot, TupleTableSlot *srcSlot, ResultRelInfo *returningRelInfo, EState *estate, bool canSetTag)
{
  ResultRelInfo *resultRelInfo;
  Relation resultRelationDesc;
  List *recheckIndexes = NIL;
  TupleTableSlot *result = NULL;
  TransitionCaptureState *ar_insert_trig_tcs;
  ModifyTable *node = (ModifyTable *)mtstate->ps.plan;
  OnConflictAction onconflict = node->onConflictAction;

  ExecMaterializeSlot(slot);

     
                                                      
     
  resultRelInfo = estate->es_result_relation_info;
  resultRelationDesc = resultRelInfo->ri_RelationDesc;

     
                                 
     
                                                                           
                                                                       
                                                                          
                                                                            
                                                                        
     
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_before_row)
  {
    if (!ExecBRInsertTriggers(estate, resultRelInfo, slot))
    {
      return NULL;                   
    }
  }

                                      
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_instead_row)
  {
    if (!ExecIRInsertTriggers(estate, resultRelInfo, slot))
    {
      return NULL;                   
    }
  }
  else if (resultRelInfo->ri_FdwRoutine)
  {
       
                                                                     
                                                            
       
    slot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

       
                                        
       
    if (resultRelationDesc->rd_att->constr && resultRelationDesc->rd_att->constr->has_generated_stored)
    {
      ExecComputeStoredGenerated(estate, slot);
    }

       
                                                    
       
    slot = resultRelInfo->ri_FdwRoutine->ExecForeignInsert(estate, resultRelInfo, slot, planSlot);

    if (slot == NULL)                   
    {
      return NULL;
    }

       
                                                                       
                                                                          
                                                                      
       
    slot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);
  }
  else
  {
    WCOKind wco_kind;

       
                                                                          
                                                                       
       
    slot->tts_tableOid = RelationGetRelid(resultRelationDesc);

       
                                        
       
    if (resultRelationDesc->rd_att->constr && resultRelationDesc->rd_att->constr->has_generated_stored)
    {
      ExecComputeStoredGenerated(estate, slot);
    }

       
                                          
       
                                                                          
                                                                      
                                                                          
                                                                     
                                        
       
    wco_kind = (mtstate->operation == CMD_UPDATE) ? WCO_RLS_UPDATE_CHECK : WCO_RLS_INSERT_CHECK;

       
                                                                           
                                         
       
    if (resultRelInfo->ri_WithCheckOptions != NIL)
    {
      ExecWithCheckOptions(wco_kind, resultRelInfo, slot, estate);
    }

       
                                           
       
    if (resultRelationDesc->rd_att->constr)
    {
      ExecConstraints(resultRelInfo, slot, estate);
    }

       
                                                                          
                                                                           
                                                          
       
    if (resultRelInfo->ri_PartitionCheck && (resultRelInfo->ri_RootResultRelInfo == NULL || (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_insert_before_row)))
    {
      ExecPartitionCheck(resultRelInfo, slot, estate, true);
    }

    if (onconflict != ONCONFLICT_NONE && resultRelInfo->ri_NumIndices > 0)
    {
                                            
      uint32 specToken;
      ItemPointerData conflictTid;
      bool specConflict;
      List *arbiterIndexes;

      arbiterIndexes = resultRelInfo->ri_onConflictArbiterIndexes;

         
                                                        
         
                                                                         
                                                                        
                                                                       
                                                         
         
                                                                      
                                                                      
                                                                        
                                
         
    vlock:
      CHECK_FOR_INTERRUPTS();
      specConflict = false;
      if (!ExecCheckIndexConstraints(slot, estate, &conflictTid, arbiterIndexes))
      {
                                            
        if (onconflict == ONCONFLICT_UPDATE)
        {
             
                                                                  
                                                                     
                                                                 
                    
             
          TupleTableSlot *returning = NULL;

          if (ExecOnConflictUpdate(mtstate, resultRelInfo, &conflictTid, planSlot, slot, estate, canSetTag, &returning))
          {
            InstrCountTuples2(&mtstate->ps, 1);
            return returning;
          }
          else
          {
            goto vlock;
          }
        }
        else
        {
             
                                                                     
                                                                     
                                                  
             
                                                                     
                                                                   
                                                                     
                                                      
                                                              
             
          Assert(onconflict == ONCONFLICT_NOTHING);
          ExecCheckTIDVisible(estate, resultRelInfo, &conflictTid, ExecGetReturningSlot(estate, resultRelInfo));
          InstrCountTuples2(&mtstate->ps, 1);
          return NULL;
        }
      }

         
                                                                    
                                                                        
                                                                   
                                                        
         
      specToken = SpeculativeInsertionLockAcquire(GetCurrentTransactionId());

                                                        
      table_tuple_insert_speculative(resultRelationDesc, slot, estate->es_output_cid, 0, NULL, specToken);

                                          
      recheckIndexes = ExecInsertIndexTuples(slot, estate, true, &specConflict, arbiterIndexes);

                                                
      table_tuple_complete_speculative(resultRelationDesc, slot, specToken, !specConflict);

         
                                                                      
                                                                         
                                                                         
                                                                         
                                  
         
      SpeculativeInsertionLockRelease(GetCurrentTransactionId());

         
                                                                      
                                                                        
                                                 
         
      if (specConflict)
      {
        list_free(recheckIndexes);
        goto vlock;
      }

                                                             
    }
    else
    {
                                     
      table_tuple_insert(resultRelationDesc, slot, estate->es_output_cid, 0, NULL);

                                          
      if (resultRelInfo->ri_NumIndices > 0)
      {
        recheckIndexes = ExecInsertIndexTuples(slot, estate, false, NULL, NIL);
      }
    }
  }

  if (canSetTag)
  {
    (estate->es_processed)++;
    setLastTid(&slot->tts_tid);
  }

     
                                                                           
                                                                           
                                                                          
                                              
     
  ar_insert_trig_tcs = mtstate->mt_transition_capture;
  if (mtstate->operation == CMD_UPDATE && mtstate->mt_transition_capture && mtstate->mt_transition_capture->tcs_update_new_table)
  {
    ExecARUpdateTriggers(estate, resultRelInfo, NULL, NULL, slot, NULL, mtstate->mt_transition_capture);

       
                                                                     
                                                            
       
    ar_insert_trig_tcs = NULL;
  }

                                 
  ExecARInsertTriggers(estate, resultRelInfo, slot, recheckIndexes, ar_insert_trig_tcs);

  list_free(recheckIndexes);

     
                                                                        
                                                                      
                                                                           
                                           
     
                                                                           
                                                                     
     
                                                                            
                                    
     
  if (resultRelInfo->ri_WithCheckOptions != NIL)
  {
    ExecWithCheckOptions(WCO_VIEW_CHECK, resultRelInfo, slot, estate);
  }

                                    
  if (returningRelInfo->ri_projectReturning)
  {
       
                                                                      
                                                                          
                                                                   
                                                                 
                                                                          
                                                                        
                                                                     
       
                                                                         
                                                                          
                                                                       
                                                                           
                
       
    if (returningRelInfo != resultRelInfo && slot != srcSlot)
    {
      Relation srcRelationDesc = returningRelInfo->ri_RelationDesc;
      AttrNumber *map;

      map = convert_tuples_by_name_map_if_req(RelationGetDescr(resultRelationDesc), RelationGetDescr(srcRelationDesc), gettext_noop("could not convert row type"));
      if (map)
      {
        TupleTableSlot *origSlot = slot;

        slot = execute_attr_map_slot(map, slot, srcSlot);
        slot->tts_tid = origSlot->tts_tid;
        slot->tts_tableOid = origSlot->tts_tableOid;
        pfree(map);
      }
    }

    result = ExecProcessReturning(returningRelInfo->ri_projectReturning, RelationGetRelid(resultRelationDesc), slot, planSlot);
  }

  return result;
}

                                                                    
               
   
                                                                  
                                    
   
                                                                
                                                             
                                                                 
                                                                  
                                                                 
                                                                   
                                                                  
                                                                     
                                                                 
                                                                     
                                                                  
                                                                  
   
                                                     
                                                                    
   
static TupleTableSlot *
ExecDelete(ModifyTableState *mtstate, ItemPointer tupleid, HeapTuple oldtuple, TupleTableSlot *planSlot, EPQState *epqstate, EState *estate, bool processReturning, bool canSetTag, bool changingPart, bool *tupleDeleted, TupleTableSlot **epqreturnslot)
{
  ResultRelInfo *resultRelInfo;
  Relation resultRelationDesc;
  TM_Result result;
  TM_FailureData tmfd;
  TupleTableSlot *slot = NULL;
  TransitionCaptureState *ar_delete_trig_tcs;

  if (tupleDeleted)
  {
    *tupleDeleted = false;
  }

     
                                                      
     
  resultRelInfo = estate->es_result_relation_info;
  resultRelationDesc = resultRelInfo->ri_RelationDesc;

                                  
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_delete_before_row)
  {
    bool dodelete;

    dodelete = ExecBRDeleteTriggers(estate, epqstate, resultRelInfo, tupleid, oldtuple, epqreturnslot);

    if (!dodelete)                   
    {
      return NULL;
    }
  }

                                      
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_delete_instead_row)
  {
    bool dodelete;

    Assert(oldtuple != NULL);
    dodelete = ExecIRDeleteTriggers(estate, resultRelInfo, oldtuple);

    if (!dodelete)                   
    {
      return NULL;
    }
  }
  else if (resultRelInfo->ri_FdwRoutine)
  {
       
                                                    
       
                                                                       
                                                                
       
    slot = ExecGetReturningSlot(estate, resultRelInfo);
    slot = resultRelInfo->ri_FdwRoutine->ExecForeignDelete(estate, resultRelInfo, slot, planSlot);

    if (slot == NULL)                   
    {
      return NULL;
    }

       
                                                                     
                                                           
       
    if (TTS_EMPTY(slot))
    {
      ExecStoreAllNullTuple(slot);
    }

    slot->tts_tableOid = RelationGetRelid(resultRelationDesc);
  }
  else
  {
       
                        
       
                                                                       
                                                                           
                                                                     
                                                                        
                          
       
  ldelete:;
    result = table_tuple_delete(resultRelationDesc, tupleid, estate->es_output_cid, estate->es_snapshot, estate->es_crosscheck_snapshot, true                      , &tmfd, changingPart);

    switch (result)
    {
    case TM_SelfModified:

         
                                                                
                                                               
                                                                    
                                                                   
                                                                   
                                                          
         
                                                              
                                                                   
                                                                     
                                                                     
                                                                
                                                                    
                                                                
                                                                     
                                                                    
                                                                  
                 
         
                                                                    
                                                                  
                           
         
      if (tmfd.cmax != estate->es_output_cid)
      {
        ereport(ERROR, (errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION), errmsg("tuple to be deleted was already modified by an operation triggered by the current command"), errhint("Consider using an AFTER trigger instead of a BEFORE trigger to propagate changes to other rows.")));
      }

                                                        
      return NULL;

    case TM_Ok:
      break;

    case TM_Updated:
    {
      TupleTableSlot *inputslot;
      TupleTableSlot *epqslot;

      if (IsolationUsesXactSnapshot())
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent update")));
      }

         
                                                             
                                                   
         
      EvalPlanQualBegin(epqstate);
      inputslot = EvalPlanQualSlot(epqstate, resultRelationDesc, resultRelInfo->ri_RangeTableIndex);

      result = table_tuple_lock(resultRelationDesc, tupleid, estate->es_snapshot, inputslot, estate->es_output_cid, LockTupleExclusive, LockWaitBlock, TUPLE_LOCK_FLAG_FIND_LAST_VERSION, &tmfd);

      switch (result)
      {
      case TM_Ok:
        Assert(tmfd.traversed);
        epqslot = EvalPlanQual(epqstate, resultRelationDesc, resultRelInfo->ri_RangeTableIndex, inputslot);
        if (TupIsNull(epqslot))
        {
                                                           
          return NULL;
        }

           
                                                       
                        
           
        if (epqreturnslot)
        {
          *epqreturnslot = epqslot;
          return NULL;
        }
        else
        {
          goto ldelete;
        }

      case TM_SelfModified:

           
                                                        
                                                          
                                                        
                                                           
                                                       
                
           
                                                
                                       
           
        if (tmfd.cmax != estate->es_output_cid)
        {
          ereport(ERROR, (errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION), errmsg("tuple to be deleted was already modified by an operation triggered by the current command"), errhint("Consider using an AFTER trigger instead of a BEFORE trigger to propagate changes to other rows.")));
        }
        return NULL;

      case TM_Deleted:
                                                  
        return NULL;

      default:

           
                                                           
                                                       
                                                         
                         
           
                                                          
                                          
                                              
           
        elog(ERROR, "unexpected table_tuple_lock status: %u", result);
        return NULL;
      }

      Assert(false);
      break;
    }

    case TM_Deleted:
      if (IsolationUsesXactSnapshot())
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent delete")));
      }
                                                
      return NULL;

    default:
      elog(ERROR, "unrecognized table_tuple_delete status: %u", result);
      return NULL;
    }

       
                                                                          
                                             
       
                                                                           
                                                                        
                                                                       
       
  }

  if (canSetTag)
  {
    (estate->es_processed)++;
  }

                                                      
  if (tupleDeleted)
  {
    *tupleDeleted = true;
  }

     
                                                                           
                                                                           
                                                                          
                                              
     
  ar_delete_trig_tcs = mtstate->mt_transition_capture;
  if (mtstate->operation == CMD_UPDATE && mtstate->mt_transition_capture && mtstate->mt_transition_capture->tcs_update_old_table)
  {
    ExecARUpdateTriggers(estate, resultRelInfo, tupleid, oldtuple, NULL, NULL, mtstate->mt_transition_capture);

       
                                                                     
                                                            
       
    ar_delete_trig_tcs = NULL;
  }

                                 
  ExecARDeleteTriggers(estate, resultRelInfo, tupleid, oldtuple, ar_delete_trig_tcs);

                                                     
  if (processReturning && resultRelInfo->ri_projectReturning)
  {
       
                                                                         
                                                           
       
    TupleTableSlot *rslot;

    if (resultRelInfo->ri_FdwRoutine)
    {
                                                                    
      Assert(!TupIsNull(slot));
    }
    else
    {
      slot = ExecGetReturningSlot(estate, resultRelInfo);
      if (oldtuple != NULL)
      {
        ExecForceStoreHeapTuple(oldtuple, slot, false);
      }
      else
      {
        if (!table_tuple_fetch_row_version(resultRelationDesc, tupleid, SnapshotAny, slot))
        {
          elog(ERROR, "failed to fetch deleted tuple for DELETE RETURNING");
        }
      }
    }

    rslot = ExecProcessReturning(resultRelInfo->ri_projectReturning, RelationGetRelid(resultRelationDesc), slot, planSlot);

       
                                                                      
                                                   
       
    ExecMaterializeSlot(rslot);

    ExecClearTuple(slot);

    return rslot;
  }

  return NULL;
}

                                                                    
               
   
                                                        
                                                     
                                                          
                                                          
                                                       
                                   
   
                                                           
                                                                 
                                                                
                                                                    
                                                               
                                                                
                                                                  
                          
   
                                                     
                                                                    
   
static TupleTableSlot *
ExecUpdate(ModifyTableState *mtstate, ItemPointer tupleid, HeapTuple oldtuple, TupleTableSlot *slot, TupleTableSlot *planSlot, EPQState *epqstate, EState *estate, bool canSetTag)
{
  ResultRelInfo *resultRelInfo;
  Relation resultRelationDesc;
  TM_Result result;
  TM_FailureData tmfd;
  List *recheckIndexes = NIL;
  TupleConversionMap *saved_tcs_map = NULL;

     
                                                     
     
  if (IsBootstrapProcessingMode())
  {
    elog(ERROR, "cannot UPDATE during bootstrap");
  }

  ExecMaterializeSlot(slot);

     
                                                      
     
  resultRelInfo = estate->es_result_relation_info;
  resultRelationDesc = resultRelInfo->ri_RelationDesc;

                                  
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_update_before_row)
  {
    if (!ExecBRUpdateTriggers(estate, epqstate, resultRelInfo, tupleid, oldtuple, slot))
    {
      return NULL;                   
    }
  }

                                      
  if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_update_instead_row)
  {
    if (!ExecIRUpdateTriggers(estate, resultRelInfo, oldtuple, slot))
    {
      return NULL;                   
    }
  }
  else if (resultRelInfo->ri_FdwRoutine)
  {
       
                                                                     
                                                            
       
    slot->tts_tableOid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

       
                                        
       
    if (resultRelationDesc->rd_att->constr && resultRelationDesc->rd_att->constr->has_generated_stored)
    {
      ExecComputeStoredGenerated(estate, slot);
    }

       
                                                  
       
    slot = resultRelInfo->ri_FdwRoutine->ExecForeignUpdate(estate, resultRelInfo, slot, planSlot);

    if (slot == NULL)                   
    {
      return NULL;
    }

       
                                                                       
                                                                          
                                                                      
       
    slot->tts_tableOid = RelationGetRelid(resultRelationDesc);
  }
  else
  {
    LockTupleMode lockmode;
    bool partition_constraint_failed;
    bool update_indexes;

       
                                                                          
                                                                       
       
    slot->tts_tableOid = RelationGetRelid(resultRelationDesc);

       
                                        
       
    if (resultRelationDesc->rd_att->constr && resultRelationDesc->rd_att->constr->has_generated_stored)
    {
      ExecComputeStoredGenerated(estate, slot);
    }

       
                                                
       
                                                                           
                                                                         
                                                                          
                                                                           
                                                            
       
  lreplace:;

                                                       
    ExecMaterializeSlot(slot);

       
                                                                          
                                                                          
                                                                           
                                                                          
                                                                       
       
    partition_constraint_failed = resultRelInfo->ri_PartitionCheck && !ExecPartitionCheck(resultRelInfo, slot, estate, false);

    if (!partition_constraint_failed && resultRelInfo->ri_WithCheckOptions != NIL)
    {
         
                                                                        
                                                
         
      ExecWithCheckOptions(WCO_RLS_UPDATE_CHECK, resultRelInfo, slot, estate);
    }

       
                                                                       
                  
       
    if (partition_constraint_failed)
    {
      bool tuple_deleted;
      TupleTableSlot *ret_slot;
      TupleTableSlot *orig_slot = slot;
      TupleTableSlot *epqslot = NULL;
      PartitionTupleRouting *proute = mtstate->mt_partition_tuple_routing;
      int map_index;
      TupleConversionMap *tupconv_map;

         
                                                                  
                                                                       
                                                                         
                                 
         
      if (((ModifyTable *)mtstate->ps.plan)->onConflictAction == ONCONFLICT_UPDATE)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("invalid ON UPDATE specification"), errdetail("The result tuple would appear in a different partition than the original tuple.")));
      }

         
                                                                     
                                                                 
                                               
         
      if (proute == NULL)
      {
        ExecPartitionCheckEmitError(resultRelInfo, slot, estate);
      }

         
                                                                     
                                                         
         
      ExecDelete(mtstate, tupleid, oldtuple, planSlot, epqstate, estate, false, false                , true /* changingPart */, &tuple_deleted, &epqslot);

         
                                                                         
                                                                       
                                                                         
                                                                      
                                                                      
                
         
                                                                    
                                                                      
                                                                  
                                                                         
                                                                         
                                                                      
                                                                      
                                                                    
                                                                     
                                                                    
                       
         
      if (!tuple_deleted)
      {
           
                                                                  
                                                                       
                                                                  
                                                                      
                                               
           
        if (TupIsNull(epqslot))
        {
          return NULL;
        }
        else
        {
          slot = ExecFilterJunk(resultRelInfo->ri_junkFilter, epqslot);
          goto lreplace;
        }
      }

         
                                                                        
                                                                       
                                                                       
                                                                        
                                                                
         
      if (mtstate->mt_transition_capture)
      {
        saved_tcs_map = mtstate->mt_transition_capture->tcs_map;
      }

         
                                                                        
                                                                      
                                                                         
                                                                     
                                                                  
                                                                
         
      map_index = resultRelInfo - mtstate->resultRelInfo;
      Assert(map_index >= 0 && map_index < mtstate->mt_nplans);
      tupconv_map = tupconv_map_for_subplan(mtstate, map_index);
      if (tupconv_map != NULL)
      {
        slot = execute_attr_map_slot(tupconv_map->attrMap, slot, mtstate->mt_root_tuple_slot);
      }

         
                                                                        
                        
         
      Assert(mtstate->rootResultRelInfo != NULL);
      slot = ExecPrepareTupleRouting(mtstate, estate, proute, mtstate->rootResultRelInfo, slot);

      ret_slot = ExecInsert(mtstate, slot, planSlot, orig_slot, resultRelInfo, estate, canSetTag);

                                                         
      estate->es_result_relation_info = resultRelInfo;
      if (mtstate->mt_transition_capture)
      {
        mtstate->mt_transition_capture->tcs_original_insert_tuple = NULL;
        mtstate->mt_transition_capture->tcs_map = saved_tcs_map;
      }

      return ret_slot;
    }

       
                                                                      
                                                                           
                                                                           
                                              
       
    if (resultRelationDesc->rd_att->constr)
    {
      ExecConstraints(resultRelInfo, slot, estate);
    }

       
                              
       
                                                                       
                                                                           
                                                                     
                                                                        
                          
       
    result = table_tuple_update(resultRelationDesc, tupleid, slot, estate->es_output_cid, estate->es_snapshot, estate->es_crosscheck_snapshot, true                      , &tmfd, &lockmode, &update_indexes);

    switch (result)
    {
    case TM_SelfModified:

         
                                                                
                                                               
                                                                    
                                                                   
                                                                     
                                                            
                                     
         
                                                              
                                                                   
                                                                     
                                                                   
                                                                
                                                                 
                                                              
                                                               
                 
         
                                                                    
                                                                    
                                                          
         
      if (tmfd.cmax != estate->es_output_cid)
      {
        ereport(ERROR, (errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION), errmsg("tuple to be updated was already modified by an operation triggered by the current command"), errhint("Consider using an AFTER trigger instead of a BEFORE trigger to propagate changes to other rows.")));
      }

                                                        
      return NULL;

    case TM_Ok:
      break;

    case TM_Updated:
    {
      TupleTableSlot *inputslot;
      TupleTableSlot *epqslot;

      if (IsolationUsesXactSnapshot())
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent update")));
      }

         
                                                             
                                                   
         
      inputslot = EvalPlanQualSlot(epqstate, resultRelationDesc, resultRelInfo->ri_RangeTableIndex);

      result = table_tuple_lock(resultRelationDesc, tupleid, estate->es_snapshot, inputslot, estate->es_output_cid, lockmode, LockWaitBlock, TUPLE_LOCK_FLAG_FIND_LAST_VERSION, &tmfd);

      switch (result)
      {
      case TM_Ok:
        Assert(tmfd.traversed);

        epqslot = EvalPlanQual(epqstate, resultRelationDesc, resultRelInfo->ri_RangeTableIndex, inputslot);
        if (TupIsNull(epqslot))
        {
                                                           
          return NULL;
        }

        slot = ExecFilterJunk(resultRelInfo->ri_junkFilter, epqslot);
        goto lreplace;

      case TM_Deleted:
                                                  
        return NULL;

      case TM_SelfModified:

           
                                                        
                                                          
                                                        
                                                       
                                                      
                                
           
                                                
                                       
           
        if (tmfd.cmax != estate->es_output_cid)
        {
          ereport(ERROR, (errcode(ERRCODE_TRIGGERED_DATA_CHANGE_VIOLATION), errmsg("tuple to be updated was already modified by an operation triggered by the current command"), errhint("Consider using an AFTER trigger instead of a BEFORE trigger to propagate changes to other rows.")));
        }
        return NULL;

      default:
                                                       
        elog(ERROR, "unexpected table_tuple_lock status: %u", result);
        return NULL;
      }
    }

    break;

    case TM_Deleted:
      if (IsolationUsesXactSnapshot())
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent delete")));
      }
                                                
      return NULL;

    default:
      elog(ERROR, "unrecognized table_tuple_update status: %u", result);
      return NULL;
    }

                                                     
    if (resultRelInfo->ri_NumIndices > 0 && update_indexes)
    {
      recheckIndexes = ExecInsertIndexTuples(slot, estate, false, NULL, NIL);
    }
  }

  if (canSetTag)
  {
    (estate->es_processed)++;
  }

                                 
  ExecARUpdateTriggers(estate, resultRelInfo, tupleid, oldtuple, slot, recheckIndexes, mtstate->operation == CMD_INSERT ? mtstate->mt_oc_transition_capture : mtstate->mt_transition_capture);

  list_free(recheckIndexes);

     
                                                                        
                                                                      
                                                                          
                                         
     
                                                                            
                                    
     
  if (resultRelInfo->ri_WithCheckOptions != NIL)
  {
    ExecWithCheckOptions(WCO_VIEW_CHECK, resultRelInfo, slot, estate);
  }

                                    
  if (resultRelInfo->ri_projectReturning)
  {
    return ExecProcessReturning(resultRelInfo->ri_projectReturning, RelationGetRelid(resultRelationDesc), slot, planSlot);
  }

  return NULL;
}

   
                                                                           
   
                                                                      
                                                                      
                                                                
              
   
                                                                       
                                                  
   
static bool
ExecOnConflictUpdate(ModifyTableState *mtstate, ResultRelInfo *resultRelInfo, ItemPointer conflictTid, TupleTableSlot *planSlot, TupleTableSlot *excludedSlot, EState *estate, bool canSetTag, TupleTableSlot **returning)
{
  ExprContext *econtext = mtstate->ps.ps_ExprContext;
  Relation relation = resultRelInfo->ri_RelationDesc;
  ExprState *onConflictSetWhere = resultRelInfo->ri_onConflict->oc_WhereClause;
  TupleTableSlot *existing = resultRelInfo->ri_onConflict->oc_Existing;
  TM_FailureData tmfd;
  LockTupleMode lockmode;
  TM_Result test;
  Datum xminDatum;
  TransactionId xmin;
  bool isnull;

                                  
  lockmode = ExecUpdateLockMode(estate, resultRelInfo);

     
                                                                       
                                                                     
                                                                         
                   
     
  test = table_tuple_lock(relation, conflictTid, estate->es_snapshot, existing, estate->es_output_cid, lockmode, LockWaitBlock, 0, &tmfd);
  switch (test)
  {
  case TM_Ok:
                  
    break;

  case TM_Invisible:

       
                                                                     
                                                                  
                                            
       
                                                                    
                                                                     
                                                                       
                                                                       
                 
       
                                                                      
                                                                       
                                                                       
                                                
       
    xminDatum = slot_getsysattr(existing, MinTransactionIdAttributeNumber, &isnull);
    Assert(!isnull);
    xmin = DatumGetTransactionId(xminDatum);

    if (TransactionIdIsCurrentTransactionId(xmin))
    {
      ereport(ERROR, (errcode(ERRCODE_CARDINALITY_VIOLATION), errmsg("ON CONFLICT DO UPDATE command cannot affect row a second time"), errhint("Ensure that no rows proposed for insertion within the same command have duplicate constrained values.")));
    }

                               
    elog(ERROR, "attempted to lock invisible tuple");
    break;

  case TM_SelfModified:

       
                                                                       
                                                                       
                                       
       
    elog(ERROR, "unexpected self-updated tuple");
    break;

  case TM_Updated:
    if (IsolationUsesXactSnapshot())
    {
      ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent update")));
    }

       
                                                                       
                                                                       
                                                                      
                             
       
    Assert(!ItemPointerIndicatesMovedPartitions(&tmfd.ctid));

       
                                                     
       
                                                                    
                                                                   
                                                                    
       
    ExecClearTuple(existing);
    return false;

  case TM_Deleted:
    if (IsolationUsesXactSnapshot())
    {
      ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent delete")));
    }

                             
    Assert(!ItemPointerIndicatesMovedPartitions(&tmfd.ctid));
    ExecClearTuple(existing);
    return false;

  default:
    elog(ERROR, "unrecognized table_tuple_lock status: %u", test);
  }

                                     

     
                                                                          
                                    
     
                                                                          
                                                                  
     
                                                                        
                                                                           
                                                                    
                                                                           
               
     
  ExecCheckTupleVisible(estate, relation, existing);

     
                                                                        
                                                                             
                                                                          
                                                                        
                        
     
  econtext->ecxt_scantuple = existing;
  econtext->ecxt_innertuple = excludedSlot;
  econtext->ecxt_outertuple = NULL;

  if (!ExecQual(onConflictSetWhere, econtext))
  {
    ExecClearTuple(existing);                       
    InstrCountFiltered1(&mtstate->ps, 1);
    return true;                          
  }

  if (resultRelInfo->ri_WithCheckOptions != NIL)
  {
       
                                                                     
                                                                          
       
                                                                       
                                                                        
                                                                   
                           
       
                                                                         
                                                                     
                                                                           
                                                                        
                              
       
    ExecWithCheckOptions(WCO_RLS_CONFLICT_CHECK, resultRelInfo, existing, mtstate->ps.state);
  }

                                     
  ExecProject(resultRelInfo->ri_onConflict->oc_ProjInfo);

     
                                                                         
                                                                            
                                                                             
                                                                       
                                                                             
                                    
     

                                      
  *returning = ExecUpdate(mtstate, conflictTid, NULL, resultRelInfo->ri_onConflict->oc_ProjSlot, planSlot, &mtstate->mt_epqstate, mtstate->ps.state, canSetTag);

     
                                                                            
                                                                           
            
     
  ExecClearTuple(existing);
  return true;
}

   
                                          
   
static void
fireBSTriggers(ModifyTableState *node)
{
  ModifyTable *plan = (ModifyTable *)node->ps.plan;
  ResultRelInfo *resultRelInfo = node->resultRelInfo;

     
                                                                          
                                                                          
                                    
     
  if (node->rootResultRelInfo != NULL)
  {
    resultRelInfo = node->rootResultRelInfo;
  }

  switch (node->operation)
  {
  case CMD_INSERT:
    ExecBSInsertTriggers(node->ps.state, resultRelInfo);
    if (plan->onConflictAction == ONCONFLICT_UPDATE)
    {
      ExecBSUpdateTriggers(node->ps.state, resultRelInfo);
    }
    break;
  case CMD_UPDATE:
    ExecBSUpdateTriggers(node->ps.state, resultRelInfo);
    break;
  case CMD_DELETE:
    ExecBSDeleteTriggers(node->ps.state, resultRelInfo);
    break;
  default:
    elog(ERROR, "unknown operation");
    break;
  }
}

   
                                        
   
                                  
                                                                   
                                                                              
                   
                                 
   
static ResultRelInfo *
getTargetResultRelInfo(ModifyTableState *node)
{
     
                                                                             
                                                             
     
  if (node->rootResultRelInfo != NULL)
  {
    return node->rootResultRelInfo;
  }
  else
  {
    return node->resultRelInfo;
  }
}

   
                                         
   
static void
fireASTriggers(ModifyTableState *node)
{
  ModifyTable *plan = (ModifyTable *)node->ps.plan;
  ResultRelInfo *resultRelInfo = getTargetResultRelInfo(node);

  switch (node->operation)
  {
  case CMD_INSERT:
    if (plan->onConflictAction == ONCONFLICT_UPDATE)
    {
      ExecASUpdateTriggers(node->ps.state, resultRelInfo, node->mt_oc_transition_capture);
    }
    ExecASInsertTriggers(node->ps.state, resultRelInfo, node->mt_transition_capture);
    break;
  case CMD_UPDATE:
    ExecASUpdateTriggers(node->ps.state, resultRelInfo, node->mt_transition_capture);
    break;
  case CMD_DELETE:
    ExecASDeleteTriggers(node->ps.state, resultRelInfo, node->mt_transition_capture);
    break;
  default:
    elog(ERROR, "unknown operation");
    break;
  }
}

   
                                                                      
             
   
static void
ExecSetupTransitionCaptureState(ModifyTableState *mtstate, EState *estate)
{
  ModifyTable *plan = (ModifyTable *)mtstate->ps.plan;
  ResultRelInfo *targetRelInfo = getTargetResultRelInfo(mtstate);

                                                                      
  mtstate->mt_transition_capture = MakeTransitionCaptureState(targetRelInfo->ri_TrigDesc, RelationGetRelid(targetRelInfo->ri_RelationDesc), mtstate->operation);
  if (plan->operation == CMD_INSERT && plan->onConflictAction == ONCONFLICT_UPDATE)
  {
    mtstate->mt_oc_transition_capture = MakeTransitionCaptureState(targetRelInfo->ri_TrigDesc, RelationGetRelid(targetRelInfo->ri_RelationDesc), CMD_UPDATE);
  }

     
                                                                            
                                                                           
                                                                       
                                                 
     
  if (mtstate->mt_transition_capture != NULL || mtstate->mt_oc_transition_capture != NULL)
  {
    ExecSetupChildParentMapForSubplan(mtstate);

       
                                                                           
                                                                        
                                                                          
                                                
       
    if (mtstate->mt_transition_capture && mtstate->operation != CMD_INSERT)
    {
      mtstate->mt_transition_capture->tcs_map = tupconv_map_for_subplan(mtstate, 0);
    }
  }
}

   
                                                             
   
                                                                         
                                                    
   
                                                                        
                                                                        
   
                                                              
   
static TupleTableSlot *
ExecPrepareTupleRouting(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, ResultRelInfo *targetRelInfo, TupleTableSlot *slot)
{
  ResultRelInfo *partrel;
  PartitionRoutingInfo *partrouteinfo;
  TupleConversionMap *map;

     
                                                                             
                                                                         
                                                                          
                                                                           
                                                          
     
  partrel = ExecFindPartition(mtstate, targetRelInfo, proute, slot, estate);
  partrouteinfo = partrel->ri_PartitionInfo;
  Assert(partrouteinfo != NULL);

     
                                                            
     
  estate->es_result_relation_info = partrel;

     
                                                                             
                                                            
     
  if (mtstate->mt_transition_capture != NULL)
  {
    if (partrel->ri_TrigDesc && partrel->ri_TrigDesc->trig_insert_before_row)
    {
         
                                                                       
                                                                        
         
      mtstate->mt_transition_capture->tcs_original_insert_tuple = NULL;
      mtstate->mt_transition_capture->tcs_map = partrouteinfo->pi_PartitionToRootMap;
    }
    else
    {
         
                                                                     
                                                 
         
      mtstate->mt_transition_capture->tcs_original_insert_tuple = slot;
      mtstate->mt_transition_capture->tcs_map = NULL;
    }
  }
  if (mtstate->mt_oc_transition_capture != NULL)
  {
    mtstate->mt_oc_transition_capture->tcs_map = partrouteinfo->pi_PartitionToRootMap;
  }

     
                                      
     
  map = partrouteinfo->pi_RootToPartitionMap;
  if (map != NULL)
  {
    TupleTableSlot *new_slot = partrouteinfo->pi_PartitionTupleSlot;

    slot = execute_attr_map_slot(map->attrMap, slot, new_slot);
  }

  return slot;
}

   
                                                                                
   
                                                                               
                                                                               
              
                                
                                                 
   
static void
ExecSetupChildParentMapForSubplan(ModifyTableState *mtstate)
{
  ResultRelInfo *targetRelInfo = getTargetResultRelInfo(mtstate);
  ResultRelInfo *resultRelInfos = mtstate->resultRelInfo;
  TupleDesc outdesc;
  int numResultRelInfos = mtstate->mt_nplans;
  int i;

     
                                                                           
                                                                        
                                                                
     

                                               
  outdesc = RelationGetDescr(targetRelInfo->ri_RelationDesc);

  mtstate->mt_per_subplan_tupconv_maps = (TupleConversionMap **)palloc(sizeof(TupleConversionMap *) * numResultRelInfos);

  for (i = 0; i < numResultRelInfos; ++i)
  {
    mtstate->mt_per_subplan_tupconv_maps[i] = convert_tuples_by_name(RelationGetDescr(resultRelInfos[i].ri_RelationDesc), outdesc, gettext_noop("could not convert row type"));
  }
}

   
                                                            
   
static TupleConversionMap *
tupconv_map_for_subplan(ModifyTableState *mtstate, int whichplan)
{
                                                                          
  if (mtstate->mt_per_subplan_tupconv_maps == NULL)
  {
    ExecSetupChildParentMapForSubplan(mtstate);
  }

  Assert(whichplan >= 0 && whichplan < mtstate->mt_nplans);
  return mtstate->mt_per_subplan_tupconv_maps[whichplan];
}

                                                                    
                      
   
                                                                          
               
                                                                    
   
static TupleTableSlot *
ExecModifyTable(PlanState *pstate)
{
  ModifyTableState *node = castNode(ModifyTableState, pstate);
  PartitionTupleRouting *proute = node->mt_partition_tuple_routing;
  EState *estate = node->ps.state;
  CmdType operation = node->operation;
  ResultRelInfo *saved_resultRelInfo;
  ResultRelInfo *resultRelInfo;
  PlanState *subplanstate;
  JunkFilter *junkfilter;
  TupleTableSlot *slot;
  TupleTableSlot *planSlot;
  ItemPointer tupleid;
  ItemPointerData tuple_ctid;
  HeapTupleData oldtupdata;
  HeapTuple oldtuple;

  CHECK_FOR_INTERRUPTS();

     
                                                                             
                                                                         
                                                                       
                                                                         
                                                                          
                                                                            
                           
     
  if (estate->es_epq_active != NULL)
  {
    elog(ERROR, "ModifyTable should not be called during EvalPlanQual");
  }

     
                                                                           
                                                                            
                                                                        
                  
     
  if (node->mt_done)
  {
    return NULL;
  }

     
                                                                      
     
  if (node->fireBSTriggers)
  {
    fireBSTriggers(node);
    node->fireBSTriggers = false;
  }

                               
  resultRelInfo = node->resultRelInfo + node->mt_whichplan;
  subplanstate = node->mt_plans[node->mt_whichplan];
  junkfilter = resultRelInfo->ri_junkFilter;

     
                                                                       
                                                                      
                                                                      
                                                                             
                                                               
     
  saved_resultRelInfo = estate->es_result_relation_info;

  estate->es_result_relation_info = resultRelInfo;

     
                                                                             
                   
     
  for (;;)
  {
       
                                                                       
                                                                          
                                                                           
                              
       
    ResetPerTupleExprContext(estate);

       
                                                                          
                                                                    
                                        
       
    if (pstate->ps_ExprContext)
    {
      ResetExprContext(pstate->ps_ExprContext);
    }

    planSlot = ExecProcNode(subplanstate);

    if (TupIsNull(planSlot))
    {
                                          
      node->mt_whichplan++;
      if (node->mt_whichplan < node->mt_nplans)
      {
        resultRelInfo++;
        subplanstate = node->mt_plans[node->mt_whichplan];
        junkfilter = resultRelInfo->ri_junkFilter;
        estate->es_result_relation_info = resultRelInfo;
        EvalPlanQualSetPlan(&node->mt_epqstate, subplanstate->plan, node->mt_arowmarks[node->mt_whichplan]);
                                                                   
        if (node->mt_transition_capture != NULL)
        {
          node->mt_transition_capture->tcs_map = tupconv_map_for_subplan(node, node->mt_whichplan);
        }
        if (node->mt_oc_transition_capture != NULL)
        {
          node->mt_oc_transition_capture->tcs_map = tupconv_map_for_subplan(node, node->mt_whichplan);
        }
        continue;
      }
      else
      {
        break;
      }
    }

       
                                                                       
       
    if (node->mt_scans[node->mt_whichplan]->tts_ops != planSlot->tts_ops)
    {
      ExecCopySlot(node->mt_scans[node->mt_whichplan], planSlot);
      planSlot = node->mt_scans[node->mt_whichplan];
    }

       
                                                                           
                                                  
       
    if (resultRelInfo->ri_usesFdwDirectModify)
    {
      Assert(resultRelInfo->ri_projectReturning);

         
                                                                     
                                                               
                                                                    
                          
         
      slot = ExecProcessReturning(resultRelInfo->ri_projectReturning, RelationGetRelid(resultRelInfo->ri_RelationDesc), NULL, planSlot);

      estate->es_result_relation_info = saved_resultRelInfo;
      return slot;
    }

    EvalPlanQualSetSlot(&node->mt_epqstate, planSlot);
    slot = planSlot;

    tupleid = NULL;
    oldtuple = NULL;
    if (junkfilter != NULL)
    {
         
                                                          
         
      if (operation == CMD_UPDATE || operation == CMD_DELETE)
      {
        char relkind;
        Datum datum;
        bool isNull;

        relkind = resultRelInfo->ri_RelationDesc->rd_rel->relkind;
        if (relkind == RELKIND_RELATION || relkind == RELKIND_MATVIEW)
        {
          datum = ExecGetJunkAttribute(slot, junkfilter->jf_junkAttNo, &isNull);
                                                   
          if (isNull)
          {
            elog(ERROR, "ctid is NULL");
          }

          tupleid = (ItemPointer)DatumGetPointer(datum);
          tuple_ctid = *tupleid;                                   
          tupleid = &tuple_ctid;
        }

           
                                                                      
                                   
           
                                                                    
                                                                     
                                                                   
                                                                       
                                                                       
                                                         
           
                                                                       
                                      
           
        else if (AttributeNumberIsValid(junkfilter->jf_junkAttNo))
        {
          datum = ExecGetJunkAttribute(slot, junkfilter->jf_junkAttNo, &isNull);
                                                   
          if (isNull)
          {
            elog(ERROR, "wholerow is NULL");
          }

          oldtupdata.t_data = DatumGetHeapTupleHeader(datum);
          oldtupdata.t_len = HeapTupleHeaderGetDatumLength(oldtupdata.t_data);
          ItemPointerSetInvalid(&(oldtupdata.t_self));
                                                                   
          oldtupdata.t_tableOid = (relkind == RELKIND_VIEW) ? InvalidOid : RelationGetRelid(resultRelInfo->ri_RelationDesc);

          oldtuple = &oldtupdata;
        }
        else
        {
          Assert(relkind == RELKIND_FOREIGN_TABLE);
        }
      }

         
                                         
         
      if (operation != CMD_DELETE)
      {
        slot = ExecFilterJunk(junkfilter, slot);
      }
    }

    switch (operation)
    {
    case CMD_INSERT:
                                                
      if (proute)
      {
        slot = ExecPrepareTupleRouting(node, estate, proute, resultRelInfo, slot);
      }
      slot = ExecInsert(node, slot, planSlot, NULL, estate->es_result_relation_info, estate, node->canSetTag);
                                                          
      if (proute)
      {
        estate->es_result_relation_info = resultRelInfo;
      }
      break;
    case CMD_UPDATE:
      slot = ExecUpdate(node, tupleid, oldtuple, slot, planSlot, &node->mt_epqstate, estate, node->canSetTag);
      break;
    case CMD_DELETE:
      slot = ExecDelete(node, tupleid, oldtuple, planSlot, &node->mt_epqstate, estate, true, node->canSetTag, false                   , NULL, NULL);
      break;
    default:
      elog(ERROR, "unknown operation");
      break;
    }

       
                                                                          
                              
       
    if (slot)
    {
      estate->es_result_relation_info = saved_resultRelInfo;
      return slot;
    }
  }

                                                      
  estate->es_result_relation_info = saved_resultRelInfo;

     
                                                                   
     
  fireASTriggers(node);

  node->mt_done = true;

  return NULL;
}

                                                                    
                        
                                                                    
   
ModifyTableState *
ExecInitModifyTable(ModifyTable *node, EState *estate, int eflags)
{
  ModifyTableState *mtstate;
  CmdType operation = node->operation;
  int nplans = list_length(node->plans);
  ResultRelInfo *saved_resultRelInfo;
  ResultRelInfo *resultRelInfo;
  Plan *subplan;
  ListCell *l;
  int i;
  Relation rel;
  bool update_tuple_routing_needed = node->partColsUpdated;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  mtstate = makeNode(ModifyTableState);
  mtstate->ps.plan = (Plan *)node;
  mtstate->ps.state = estate;
  mtstate->ps.ExecProcNode = ExecModifyTable;

  mtstate->operation = operation;
  mtstate->canSetTag = node->canSetTag;
  mtstate->mt_done = false;

  mtstate->mt_plans = (PlanState **)palloc0(sizeof(PlanState *) * nplans);
  mtstate->resultRelInfo = estate->es_result_relations + node->resultRelIndex;
  mtstate->mt_scans = (TupleTableSlot **)palloc0(sizeof(TupleTableSlot *) * nplans);

                                                                        
  if (node->rootResultRelIndex >= 0)
  {
    mtstate->rootResultRelInfo = estate->es_root_result_relations + node->rootResultRelIndex;
  }

  mtstate->mt_arowmarks = (List **)palloc0(sizeof(List *) * nplans);
  mtstate->mt_nplans = nplans;

                                                              
  EvalPlanQualInit(&mtstate->mt_epqstate, estate, NULL, NIL, node->epqParam);
  mtstate->fireBSTriggers = true;

     
                                                                        
                                                                            
                                                                        
                                                                     
                                                                        
                                                                     
                                                                             
               
     
  saved_resultRelInfo = estate->es_result_relation_info;

  resultRelInfo = mtstate->resultRelInfo;
  i = 0;
  foreach (l, node->plans)
  {
    subplan = (Plan *)lfirst(l);

                                                 
    resultRelInfo->ri_usesFdwDirectModify = bms_is_member(i, node->fdwDirectModifyPlans);

       
                                                                          
       
    CheckValidResultRel(resultRelInfo, operation);

       
                                                                       
                                                                       
                                                                        
                                                                           
                                                                   
                                                                     
              
       
    if (resultRelInfo->ri_RelationDesc->rd_rel->relhasindex && operation != CMD_DELETE && resultRelInfo->ri_IndexRelationDescs == NULL)
    {
      ExecOpenIndices(resultRelInfo, node->onConflictAction != ONCONFLICT_NONE);
    }

       
                                                                        
                                                                        
                          
       
    if (resultRelInfo->ri_TrigDesc && resultRelInfo->ri_TrigDesc->trig_update_before_row && operation == CMD_UPDATE)
    {
      update_tuple_routing_needed = true;
    }

                                               
    estate->es_result_relation_info = resultRelInfo;
    mtstate->mt_plans[i] = ExecInitNode(subplan, estate, eflags);
    mtstate->mt_scans[i] = ExecInitExtraTupleSlot(mtstate->ps.state, ExecGetResultType(mtstate->mt_plans[i]), table_slot_callbacks(resultRelInfo->ri_RelationDesc));

                                                                     
    if (!resultRelInfo->ri_usesFdwDirectModify && resultRelInfo->ri_FdwRoutine != NULL && resultRelInfo->ri_FdwRoutine->BeginForeignModify != NULL)
    {
      List *fdw_private = (List *)list_nth(node->fdwPrivLists, i);

      resultRelInfo->ri_FdwRoutine->BeginForeignModify(mtstate, resultRelInfo, fdw_private, i, eflags);
    }

    resultRelInfo++;
    i++;
  }

  estate->es_result_relation_info = saved_resultRelInfo;

                               
  rel = (getTargetResultRelInfo(mtstate))->ri_RelationDesc;

     
                                                                            
                       
     
  if (rel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    update_tuple_routing_needed = false;
  }

     
                                                                             
                    
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE && (operation == CMD_INSERT || update_tuple_routing_needed))
  {
    mtstate->mt_partition_tuple_routing = ExecSetupPartitionTupleRouting(estate, mtstate, rel);
  }

     
                                                                           
                                                                   
     
  if (!(eflags & EXEC_FLAG_EXPLAIN_ONLY))
  {
    ExecSetupTransitionCaptureState(mtstate, estate);
  }

     
                                                                            
                                                                             
                                                                          
                                                                             
                                                                           
                                                                             
                                                                
     
  if (update_tuple_routing_needed)
  {
    ExecSetupChildParentMapForSubplan(mtstate);
    mtstate->mt_root_tuple_slot = table_slot_create(rel, NULL);
  }

     
                                                             
     
  resultRelInfo = mtstate->resultRelInfo;
  i = 0;
  foreach (l, node->withCheckOptionLists)
  {
    List *wcoList = (List *)lfirst(l);
    List *wcoExprs = NIL;
    ListCell *ll;

    foreach (ll, wcoList)
    {
      WithCheckOption *wco = (WithCheckOption *)lfirst(ll);
      ExprState *wcoExpr = ExecInitQual((List *)wco->qual, &mtstate->ps);

      wcoExprs = lappend(wcoExprs, wcoExpr);
    }

    resultRelInfo->ri_WithCheckOptions = wcoList;
    resultRelInfo->ri_WithCheckOptionExprs = wcoExprs;
    resultRelInfo++;
    i++;
  }

     
                                                 
     
  if (node->returningLists)
  {
    TupleTableSlot *slot;
    ExprContext *econtext;

       
                                                                           
                                                               
       
    mtstate->ps.plan->targetlist = (List *)linitial(node->returningLists);

                                                                     
    ExecInitResultTupleSlotTL(&mtstate->ps, &TTSOpsVirtual);
    slot = mtstate->ps.ps_ResultTupleSlot;

                              
    if (mtstate->ps.ps_ExprContext == NULL)
    {
      ExecAssignExprContext(estate, &mtstate->ps);
    }
    econtext = mtstate->ps.ps_ExprContext;

       
                                               
       
    resultRelInfo = mtstate->resultRelInfo;
    foreach (l, node->returningLists)
    {
      List *rlist = (List *)lfirst(l);

      resultRelInfo->ri_returningList = rlist;
      resultRelInfo->ri_projectReturning = ExecBuildProjectionInfo(rlist, econtext, slot, &mtstate->ps, resultRelInfo->ri_RelationDesc->rd_att);
      resultRelInfo++;
    }
  }
  else
  {
       
                                                                           
                                                
       
    mtstate->ps.plan->targetlist = NIL;
    ExecInitResultTypeTL(&mtstate->ps);

    mtstate->ps.ps_ExprContext = NULL;
  }

                                                                 
  resultRelInfo = mtstate->resultRelInfo;
  if (node->onConflictAction != ONCONFLICT_NONE)
  {
    resultRelInfo->ri_onConflictArbiterIndexes = node->arbiterIndexes;
  }

     
                                                                            
                
     
  if (node->onConflictAction == ONCONFLICT_UPDATE)
  {
    OnConflictSetState *onconfl = makeNode(OnConflictSetState);
    ExprContext *econtext;
    TupleDesc relationDesc;

                                                                    
    Assert(nplans == 1);

                                                                 
    if (mtstate->ps.ps_ExprContext == NULL)
    {
      ExecAssignExprContext(estate, &mtstate->ps);
    }

    econtext = mtstate->ps.ps_ExprContext;
    relationDesc = resultRelInfo->ri_RelationDesc->rd_att;

                                                           
    mtstate->mt_excludedtlist = node->exclRelTlist;

                                                  
    resultRelInfo->ri_onConflict = onconfl;

                                                
    onconfl->oc_Existing = table_slot_create(resultRelInfo->ri_RelationDesc, &mtstate->ps.state->es_tupleTable);

       
                                                                           
                                                                         
                                                                       
                          
       
    onconfl->oc_ProjSlot = table_slot_create(resultRelInfo->ri_RelationDesc, &mtstate->ps.state->es_tupleTable);

       
                                                                         
                                                                     
                                                                  
                          
       
    ExecCheckPlanOutput(resultRelInfo->ri_RelationDesc, node->onConflictSet);

                                           
    onconfl->oc_ProjInfo = ExecBuildProjectionInfoExt(node->onConflictSet, econtext, onconfl->oc_ProjSlot, false, &mtstate->ps, relationDesc);

                                                               
    if (node->onConflictWhere)
    {
      ExprState *qualexpr;

      qualexpr = ExecInitQual((List *)node->onConflictWhere, &mtstate->ps);
      onconfl->oc_WhereClause = qualexpr;
    }
  }

     
                                                                             
                                                                        
                                                                     
                            
     
  foreach (l, node->rowMarks)
  {
    PlanRowMark *rc = lfirst_node(PlanRowMark, l);
    ExecRowMark *erm;

                                                                  
    if (rc->isParent)
    {
      continue;
    }

                                                  
    erm = ExecFindRowMark(estate, rc->rti, false);

                                               
    for (i = 0; i < nplans; i++)
    {
      ExecAuxRowMark *aerm;

      subplan = mtstate->mt_plans[i]->plan;
      aerm = ExecBuildAuxRowMark(erm, subplan->targetlist);
      mtstate->mt_arowmarks[i] = lappend(mtstate->mt_arowmarks[i], aerm);
    }
  }

                            
  mtstate->mt_whichplan = 0;
  subplan = (Plan *)linitial(node->plans);
  EvalPlanQualSetPlan(&mtstate->mt_epqstate, subplan, mtstate->mt_arowmarks[0]);

     
                                                                            
                                                                         
                                                                             
                                                                     
                                                                        
                                                                         
          
     
                                                                         
                                                                            
                                                            
     
                                                                        
                                                                
     
  {
    bool junk_filter_needed = false;

    switch (operation)
    {
    case CMD_INSERT:
      foreach (l, subplan->targetlist)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(l);

        if (tle->resjunk)
        {
          junk_filter_needed = true;
          break;
        }
      }
      break;
    case CMD_UPDATE:
    case CMD_DELETE:
      junk_filter_needed = true;
      break;
    default:
      elog(ERROR, "unknown operation");
      break;
    }

    if (junk_filter_needed)
    {
      resultRelInfo = mtstate->resultRelInfo;
      for (i = 0; i < nplans; i++)
      {
        JunkFilter *j;
        TupleTableSlot *junkresslot;

        subplan = mtstate->mt_plans[i]->plan;

        junkresslot = ExecInitExtraTupleSlot(estate, NULL, table_slot_callbacks(resultRelInfo->ri_RelationDesc));

           
                                                                       
                                                                  
                                                                  
           
        if (operation == CMD_INSERT || operation == CMD_UPDATE)
        {
          ExecCheckPlanOutput(resultRelInfo->ri_RelationDesc, subplan->targetlist);
          j = ExecInitJunkFilterInsertion(subplan->targetlist, RelationGetDescr(resultRelInfo->ri_RelationDesc), junkresslot);
        }
        else
        {
          j = ExecInitJunkFilter(subplan->targetlist, junkresslot);
        }

        if (operation == CMD_UPDATE || operation == CMD_DELETE)
        {
                                                                     
          char relkind;

          relkind = resultRelInfo->ri_RelationDesc->rd_rel->relkind;
          if (relkind == RELKIND_RELATION || relkind == RELKIND_MATVIEW || relkind == RELKIND_PARTITIONED_TABLE)
          {
            j->jf_junkAttNo = ExecFindJunkAttribute(j, "ctid");
            if (!AttributeNumberIsValid(j->jf_junkAttNo))
            {
              elog(ERROR, "could not find junk ctid column");
            }
          }
          else if (relkind == RELKIND_FOREIGN_TABLE)
          {
               
                                                                  
                                     
               
            j->jf_junkAttNo = ExecFindJunkAttribute(j, "wholerow");
          }
          else
          {
            j->jf_junkAttNo = ExecFindJunkAttribute(j, "wholerow");
            if (!AttributeNumberIsValid(j->jf_junkAttNo))
            {
              elog(ERROR, "could not find junk wholerow column");
            }
          }
        }

        resultRelInfo->ri_junkFilter = j;
        resultRelInfo++;
      }
    }
    else
    {
      if (operation == CMD_INSERT)
      {
        ExecCheckPlanOutput(mtstate->resultRelInfo->ri_RelationDesc, subplan->targetlist);
      }
    }
  }

     
                                                                             
                                                                           
                                                                       
                                                                            
                                                                          
                                                                           
                                                       
     
  if (!mtstate->canSetTag)
  {
    estate->es_auxmodifytables = lcons(mtstate, estate->es_auxmodifytables);
  }

  return mtstate;
}

                                                                    
                       
   
                         
   
                                 
                                                                    
   
void
ExecEndModifyTable(ModifyTableState *node)
{
  int i;

     
                                 
     
  for (i = 0; i < node->mt_nplans; i++)
  {
    ResultRelInfo *resultRelInfo = node->resultRelInfo + i;

    if (!resultRelInfo->ri_usesFdwDirectModify && resultRelInfo->ri_FdwRoutine != NULL && resultRelInfo->ri_FdwRoutine->EndForeignModify != NULL)
    {
      resultRelInfo->ri_FdwRoutine->EndForeignModify(node->ps.state, resultRelInfo);
    }
  }

     
                                                                          
                                                          
     
  if (node->mt_partition_tuple_routing)
  {
    ExecCleanupTupleRouting(node, node->mt_partition_tuple_routing);

    if (node->mt_root_tuple_slot)
    {
      ExecDropSingleTupleTableSlot(node->mt_root_tuple_slot);
    }
  }

     
                          
     
  ExecFreeExprContext(&node->ps);

     
                               
     
  if (node->ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ps.ps_ResultTupleSlot);
  }

     
                                       
     
  EvalPlanQualEnd(&node->mt_epqstate);

     
                        
     
  for (i = 0; i < node->mt_nplans; i++)
  {
    ExecEndNode(node->mt_plans[i]);
  }
}

void
ExecReScanModifyTable(ModifyTableState *node)
{
     
                                                                          
                                                        
     
  elog(ERROR, "ExecReScanModifyTable is not implemented");
}
