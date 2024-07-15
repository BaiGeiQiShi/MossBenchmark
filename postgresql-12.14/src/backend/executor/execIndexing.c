                                                                            
   
                  
                                                                  
                            
   
                                                                       
                                                                            
                                                                   
                          
   
                  
                  
   
                                                                        
                                                                    
                                                                            
                                                                          
                                                                         
                                                                       
   
                                                                      
                                                                            
                                                                       
                                                                           
                                          
   
                         
                         
   
                                                                            
                                                                   
                                                                       
                                                                          
                                                                       
                                                                          
                    
   
                                                                        
                                                                           
                                                                      
                                                                          
                                                                    
                                                                    
            
   
                                                                         
                                                                           
                                                                           
                                                                  
                                 
   
                         
                         
   
                                                                    
                                                                          
                                                                       
                                                                            
                                                                          
                                                                            
                     
   
                                                                       
                                                                          
                                                                           
        
   
                                                                      
                                                                          
                                                                          
                                                                            
                                                                           
                                                                         
                
   
                                                                        
                                                  
   
                                                                         
                                 
                                                                            
                     
   
                                                                          
                                                                        
                                                                     
                                                                        
                                                                           
                  
   
                                                                            
                                                                           
                                                                            
                                  
   
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/index.h"
#include "executor/executor.h"
#include "nodes/nodeFuncs.h"
#include "storage/lmgr.h"
#include "utils/snapmgr.h"

                                                                 
typedef enum
{
  CEOUC_WAIT,
  CEOUC_NOWAIT,
  CEOUC_LIVELOCK_PREVENTING_WAIT
} CEOUC_WAIT_MODE;

static bool
check_exclusion_or_unique_constraint(Relation heap, Relation index, IndexInfo *indexInfo, ItemPointer tupleid, Datum *values, bool *isnull, EState *estate, bool newIndex, CEOUC_WAIT_MODE waitMode, bool errorOK, ItemPointer conflictTid);

static bool
index_recheck_constraint(Relation index, Oid *constr_procs, Datum *existing_values, bool *existing_isnull, Datum *new_values);

                                                                    
                    
   
                                                                   
                                                                 
   
                                                   
                                    
                                                                    
   
void
ExecOpenIndices(ResultRelInfo *resultRelInfo, bool speculative)
{
  Relation resultRelation = resultRelInfo->ri_RelationDesc;
  List *indexoidlist;
  ListCell *l;
  int len, i;
  RelationPtr relationDescs;
  IndexInfo **indexInfoArray;

  resultRelInfo->ri_NumIndices = 0;

                               
  if (!RelationGetForm(resultRelation)->relhasindex)
  {
    return;
  }

     
                                   
     
  indexoidlist = RelationGetIndexList(resultRelation);
  len = list_length(indexoidlist);
  if (len == 0)
  {
    return;
  }

     
                                      
     
  relationDescs = (RelationPtr)palloc(len * sizeof(Relation));
  indexInfoArray = (IndexInfo **)palloc(len * sizeof(IndexInfo *));

  resultRelInfo->ri_NumIndices = len;
  resultRelInfo->ri_IndexRelationDescs = relationDescs;
  resultRelInfo->ri_IndexRelationInfo = indexInfoArray;

     
                                                                        
                                                                    
     
                                                                          
                                                          
     
  i = 0;
  foreach (l, indexoidlist)
  {
    Oid indexOid = lfirst_oid(l);
    Relation indexDesc;
    IndexInfo *ii;

    indexDesc = index_open(indexOid, RowExclusiveLock);

                                                                      
    ii = BuildIndexInfo(indexDesc);

       
                                                                          
                                                     
       
    if (speculative && ii->ii_Unique)
    {
      BuildSpeculativeIndexInfo(indexDesc, ii);
    }

    relationDescs[i] = indexDesc;
    indexInfoArray[i] = ii;
    i++;
  }

  list_free(indexoidlist);
}

                                                                    
                     
   
                                                      
                                                                    
   
void
ExecCloseIndices(ResultRelInfo *resultRelInfo)
{
  int i;
  int numIndices;
  RelationPtr indexDescs;

  numIndices = resultRelInfo->ri_NumIndices;
  indexDescs = resultRelInfo->ri_IndexRelationDescs;

  for (i = 0; i < numIndices; i++)
  {
    if (indexDescs[i] == NULL)
    {
      continue;                        
    }

                                               
    index_close(indexDescs[i], RowExclusiveLock);
  }

     
                                                                         
                                                                       
     
}

                                                                    
                          
   
                                                      
                                                        
                                                            
   
                                                              
                                                               
                                                         
                                                              
                                                              
                                                                
   
                                                              
                                                               
   
                                                       
                                                        
                                               
                                                                    
   
List *
ExecInsertIndexTuples(TupleTableSlot *slot, EState *estate, bool noDupErr, bool *specConflict, List *arbiterIndexes)
{
  ItemPointer tupleid = &slot->tts_tid;
  List *result = NIL;
  ResultRelInfo *resultRelInfo;
  int i;
  int numIndices;
  RelationPtr relationDescs;
  Relation heapRelation;
  IndexInfo **indexInfoArray;
  ExprContext *econtext;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];

  Assert(ItemPointerIsValid(tupleid));

     
                                                              
     
  resultRelInfo = estate->es_result_relation_info;
  numIndices = resultRelInfo->ri_NumIndices;
  relationDescs = resultRelInfo->ri_IndexRelationDescs;
  indexInfoArray = resultRelInfo->ri_IndexRelationInfo;
  heapRelation = resultRelInfo->ri_RelationDesc;

                                                                            
  Assert(slot->tts_tableOid == RelationGetRelid(heapRelation));

     
                                                                          
                                                                    
     
  econtext = GetPerTupleExprContext(estate);

                                                                    
  econtext->ecxt_scantuple = slot;

     
                                                     
     
  for (i = 0; i < numIndices; i++)
  {
    Relation indexRelation = relationDescs[i];
    IndexInfo *indexInfo;
    bool applyNoDupErr;
    IndexUniqueCheck checkUnique;
    bool satisfiesConstraint;

    if (indexRelation == NULL)
    {
      continue;
    }

    indexInfo = indexInfoArray[i];

                                                        
    if (!indexInfo->ii_ReadyForInserts)
    {
      continue;
    }

                                 
    if (indexInfo->ii_Predicate != NIL)
    {
      ExprState *predicate;

         
                                                                       
                            
         
      predicate = indexInfo->ii_PredicateState;
      if (predicate == NULL)
      {
        predicate = ExecPrepareQual(indexInfo->ii_Predicate, estate);
        indexInfo->ii_PredicateState = predicate;
      }

                                                                   
      if (!ExecQual(predicate, econtext))
      {
        continue;
      }
    }

       
                                                                         
                                                          
       
    FormIndexDatum(indexInfo, slot, estate, values, isnull);

                                                       
    applyNoDupErr = noDupErr && (arbiterIndexes == NIL || list_member_oid(arbiterIndexes, indexRelation->rd_index->indexrelid));

       
                                                                         
       
                                                                        
                                  
       
                                                                          
                                                                       
                                           
       
                                                                        
                                                  
       
    if (!indexRelation->rd_index->indisunique)
    {
      checkUnique = UNIQUE_CHECK_NO;
    }
    else if (applyNoDupErr)
    {
      checkUnique = UNIQUE_CHECK_PARTIAL;
    }
    else if (indexRelation->rd_index->indimmediate)
    {
      checkUnique = UNIQUE_CHECK_YES;
    }
    else
    {
      checkUnique = UNIQUE_CHECK_PARTIAL;
    }

    satisfiesConstraint = index_insert(indexRelation,                     
        values,                                                                  
        isnull,                                                       
        tupleid,                                                             
        heapRelation,                                                    
        checkUnique,                                                                      
        indexInfo);                                                               

       
                                                                        
                                                                       
                                                                           
                                                                           
                                                                       
                                                                        
                                                                        
             
       
                                                                         
                                                                         
                                                                
       
    if (indexInfo->ii_ExclusionOps != NULL)
    {
      bool violationOK;
      CEOUC_WAIT_MODE waitMode;

      if (applyNoDupErr)
      {
        violationOK = true;
        waitMode = CEOUC_LIVELOCK_PREVENTING_WAIT;
      }
      else if (!indexRelation->rd_index->indimmediate)
      {
        violationOK = true;
        waitMode = CEOUC_NOWAIT;
      }
      else
      {
        violationOK = false;
        waitMode = CEOUC_WAIT;
      }

      satisfiesConstraint = check_exclusion_or_unique_constraint(heapRelation, indexRelation, indexInfo, tupleid, values, isnull, estate, false, waitMode, violationOK, NULL);
    }

    if ((checkUnique == UNIQUE_CHECK_PARTIAL || indexInfo->ii_ExclusionOps != NULL) && !satisfiesConstraint)
    {
         
                                                                    
                                                                         
                                                                  
                                                                     
         
      result = lappend_oid(result, RelationGetRelid(indexRelation));
      if (indexRelation->rd_index->indimmediate && specConflict)
      {
        *specConflict = true;
      }
    }
  }

  return result;
}

                                                                    
                              
   
                                                          
                                                                  
                                                            
                                       
   
                                                                  
                           
   
                                                               
                                                              
                                                              
                      
                                                                    
   
bool
ExecCheckIndexConstraints(TupleTableSlot *slot, EState *estate, ItemPointer conflictTid, List *arbiterIndexes)
{
  ResultRelInfo *resultRelInfo;
  int i;
  int numIndices;
  RelationPtr relationDescs;
  Relation heapRelation;
  IndexInfo **indexInfoArray;
  ExprContext *econtext;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  ItemPointerData invalidItemPtr;
  bool checkedIndex = false;

  ItemPointerSetInvalid(conflictTid);
  ItemPointerSetInvalid(&invalidItemPtr);

     
                                                              
     
  resultRelInfo = estate->es_result_relation_info;
  numIndices = resultRelInfo->ri_NumIndices;
  relationDescs = resultRelInfo->ri_IndexRelationDescs;
  indexInfoArray = resultRelInfo->ri_IndexRelationInfo;
  heapRelation = resultRelInfo->ri_RelationDesc;

     
                                                                          
                                                                    
     
  econtext = GetPerTupleExprContext(estate);

                                                                    
  econtext->ecxt_scantuple = slot;

     
                                                                    
                 
     
  for (i = 0; i < numIndices; i++)
  {
    Relation indexRelation = relationDescs[i];
    IndexInfo *indexInfo;
    bool satisfiesConstraint;

    if (indexRelation == NULL)
    {
      continue;
    }

    indexInfo = indexInfoArray[i];

    if (!indexInfo->ii_Unique && !indexInfo->ii_ExclusionOps)
    {
      continue;
    }

                                                        
    if (!indexInfo->ii_ReadyForInserts)
    {
      continue;
    }

                                                                    
    if (arbiterIndexes != NIL && !list_member_oid(arbiterIndexes, indexRelation->rd_index->indexrelid))
    {
      continue;
    }

    if (!indexRelation->rd_index->indimmediate)
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("ON CONFLICT does not support deferrable unique constraints/exclusion constraints as arbiters"), errtableconstraint(heapRelation, RelationGetRelationName(indexRelation))));
    }

    checkedIndex = true;

                                 
    if (indexInfo->ii_Predicate != NIL)
    {
      ExprState *predicate;

         
                                                                       
                            
         
      predicate = indexInfo->ii_PredicateState;
      if (predicate == NULL)
      {
        predicate = ExecPrepareQual(indexInfo->ii_Predicate, estate);
        indexInfo->ii_PredicateState = predicate;
      }

                                                                   
      if (!ExecQual(predicate, econtext))
      {
        continue;
      }
    }

       
                                                                         
                                                          
       
    FormIndexDatum(indexInfo, slot, estate, values, isnull);

    satisfiesConstraint = check_exclusion_or_unique_constraint(heapRelation, indexRelation, indexInfo, &invalidItemPtr, values, isnull, estate, false, CEOUC_WAIT, true, conflictTid);
    if (!satisfiesConstraint)
    {
      return false;
    }
  }

  if (arbiterIndexes != NIL && !checkedIndex)
  {
    elog(ERROR, "unexpected failure to find arbiter index");
  }

  return true;
}

   
                                                            
   
                                            
                                              
                                                                       
                                                                           
                                      
                                                                        
                                             
                                                                       
                                        
                                                               
                                                         
                                                                               
   
                                                              
   
                                                                             
                                                                       
                                                                       
                                                                          
                                                                             
                                                                          
                                                                             
                                             
   
                                                                              
                                                                            
                                                                           
                                                   
   
                                                                               
                                                                            
                                                                           
   
                                                                              
                                                                               
                                                                              
                                                                              
                                       
   
static bool
check_exclusion_or_unique_constraint(Relation heap, Relation index, IndexInfo *indexInfo, ItemPointer tupleid, Datum *values, bool *isnull, EState *estate, bool newIndex, CEOUC_WAIT_MODE waitMode, bool violationOK, ItemPointer conflictTid)
{
  Oid *constr_procs;
  uint16 *constr_strats;
  Oid *index_collations = index->rd_indcollation;
  int indnkeyatts = IndexRelationGetNumberOfKeyAttributes(index);
  IndexScanDesc index_scan;
  ScanKeyData scankeys[INDEX_MAX_KEYS];
  SnapshotData DirtySnapshot;
  int i;
  bool conflict;
  bool found_self;
  ExprContext *econtext;
  TupleTableSlot *existing_slot;
  TupleTableSlot *save_scantuple;

  if (indexInfo->ii_ExclusionOps)
  {
    constr_procs = indexInfo->ii_ExclusionProcs;
    constr_strats = indexInfo->ii_ExclusionStrats;
  }
  else
  {
    constr_procs = indexInfo->ii_UniqueProcs;
    constr_strats = indexInfo->ii_UniqueStrats;
  }

     
                                                                             
                                                      
     
  for (i = 0; i < indnkeyatts; i++)
  {
    if (isnull[i])
    {
      return true;
    }
  }

     
                                                                           
                                     
     
  InitDirtySnapshot(DirtySnapshot);

  for (i = 0; i < indnkeyatts; i++)
  {
    ScanKeyEntryInitialize(&scankeys[i], 0, i + 1, constr_strats[i], InvalidOid, index_collations[i], constr_procs[i], values[i]);
  }

     
                                                      
     
                                                                           
                                                                   
                
     
  existing_slot = table_slot_create(heap, NULL);

  econtext = GetPerTupleExprContext(estate);
  save_scantuple = econtext->ecxt_scantuple;
  econtext->ecxt_scantuple = existing_slot;

     
                                                                         
            
     
retry:
  conflict = false;
  found_self = false;
  index_scan = index_beginscan(heap, index, &DirtySnapshot, indnkeyatts, 0);
  index_rescan(index_scan, scankeys, indnkeyatts, NULL, 0);

  while (index_getnext_slot(index_scan, ForwardScanDirection, existing_slot))
  {
    TransactionId xwait;
    XLTW_Oper reason_wait;
    Datum existing_values[INDEX_MAX_KEYS];
    bool existing_isnull[INDEX_MAX_KEYS];
    char *error_new;
    char *error_existing;

       
                                                             
       
    if (ItemPointerIsValid(tupleid) && ItemPointerEquals(tupleid, &existing_slot->tts_tid))
    {
      if (found_self)                        
      {
        elog(ERROR, "found self tuple multiple times in index \"%s\"", RelationGetRelationName(index));
      }
      found_self = true;
      continue;
    }

       
                                                                          
              
       
    FormIndexDatum(indexInfo, existing_slot, estate, existing_values, existing_isnull);

                                                        
    if (index_scan->xs_recheck)
    {
      if (!index_recheck_constraint(index, constr_procs, existing_values, existing_isnull, values))
      {
        continue;                                        
                                
      }
    }

       
                                                                        
       
                                                                         
                                                                          
                                                                         
                                                                     
                                                                          
                                                            
       
    xwait = TransactionIdIsValid(DirtySnapshot.xmin) ? DirtySnapshot.xmin : DirtySnapshot.xmax;

    if (TransactionIdIsValid(xwait) && (waitMode == CEOUC_WAIT || (waitMode == CEOUC_LIVELOCK_PREVENTING_WAIT && DirtySnapshot.speculativeToken && TransactionIdPrecedes(GetCurrentTransactionId(), xwait))))
    {
      reason_wait = indexInfo->ii_ExclusionOps ? XLTW_RecheckExclusionConstr : XLTW_InsertIndex;
      index_endscan(index_scan);
      if (DirtySnapshot.speculativeToken)
      {
        SpeculativeInsertionWait(DirtySnapshot.xmin, DirtySnapshot.speculativeToken);
      }
      else
      {
        XactLockTableWait(xwait, heap, &existing_slot->tts_tid, reason_wait);
      }
      goto retry;
    }

       
                                                                       
                                                                 
       
    if (violationOK)
    {
      conflict = true;
      if (conflictTid)
      {
        *conflictTid = existing_slot->tts_tid;
      }
      break;
    }

    error_new = BuildIndexValueDescription(index, values, isnull);
    error_existing = BuildIndexValueDescription(index, existing_values, existing_isnull);
    if (newIndex)
    {
      ereport(ERROR, (errcode(ERRCODE_EXCLUSION_VIOLATION), errmsg("could not create exclusion constraint \"%s\"", RelationGetRelationName(index)), error_new && error_existing ? errdetail("Key %s conflicts with key %s.", error_new, error_existing) : errdetail("Key conflicts exist."), errtableconstraint(heap, RelationGetRelationName(index))));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_EXCLUSION_VIOLATION), errmsg("conflicting key value violates exclusion constraint \"%s\"", RelationGetRelationName(index)), error_new && error_existing ? errdetail("Key %s conflicts with existing key %s.", error_new, error_existing) : errdetail("Key conflicts with existing key."), errtableconstraint(heap, RelationGetRelationName(index))));
    }
  }

  index_endscan(index_scan);

     
                                                                           
                                                                         
                                                                            
                                                                             
                                                         
     

  econtext->ecxt_scantuple = save_scantuple;

  ExecDropSingleTupleTableSlot(existing_slot);

  return !conflict;
}

   
                                                  
   
                                                                         
                                                                
   
void
check_exclusion_constraint(Relation heap, Relation index, IndexInfo *indexInfo, ItemPointer tupleid, Datum *values, bool *isnull, EState *estate, bool newIndex)
{
  (void)check_exclusion_or_unique_constraint(heap, index, indexInfo, tupleid, values, isnull, estate, newIndex, CEOUC_WAIT, false, NULL);
}

   
                                                                       
                                                                          
   
static bool
index_recheck_constraint(Relation index, Oid *constr_procs, Datum *existing_values, bool *existing_isnull, Datum *new_values)
{
  int indnkeyatts = IndexRelationGetNumberOfKeyAttributes(index);
  int i;

  for (i = 0; i < indnkeyatts; i++)
  {
                                                   
    if (existing_isnull[i])
    {
      return false;
    }

    if (!DatumGetBool(OidFunctionCall2Coll(constr_procs[i], index->rd_indcollation[i], existing_values[i], new_values[i])))
    {
      return false;
    }
  }

  return true;
}
