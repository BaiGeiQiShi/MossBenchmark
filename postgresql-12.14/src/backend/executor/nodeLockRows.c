                                                                            
   
                  
                                                         
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
   
                      
                                      
                                                      
                                                  
   

#include "postgres.h"

#include "access/tableam.h"
#include "access/xact.h"
#include "executor/executor.h"
#include "executor/nodeLockRows.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "utils/rel.h"

                                                                    
                 
                                                                    
   
static TupleTableSlot *                              
ExecLockRows(PlanState *pstate)
{
  LockRowsState *node = castNode(LockRowsState, pstate);
  TupleTableSlot *slot;
  EState *estate;
  PlanState *outerPlan;
  bool epq_needed;
  ListCell *lc;

  CHECK_FOR_INTERRUPTS();

     
                                   
     
  estate = node->ps.state;
  outerPlan = outerPlanState(node);

     
                                          
     
lnext:
  slot = ExecProcNode(outerPlan);

  if (TupIsNull(slot))
  {
    return NULL;
  }

                                                                         
  epq_needed = false;

     
                                                                      
                                
     
  foreach (lc, node->lr_arowMarks)
  {
    ExecAuxRowMark *aerm = (ExecAuxRowMark *)lfirst(lc);
    ExecRowMark *erm = aerm->rowmark;
    Datum datum;
    bool isNull;
    ItemPointerData tid;
    TM_FailureData tmfd;
    LockTupleMode lockmode;
    int lockflags = 0;
    TM_Result test;
    TupleTableSlot *markSlot;

                                                    
    markSlot = EvalPlanQualSlot(&node->lr_epqstate, erm->relation, erm->rti);
    ExecClearTuple(markSlot);

                                                               
    if (erm->rti != erm->prti)
    {
      Oid tableoid;

      datum = ExecGetJunkAttribute(slot, aerm->toidAttNo, &isNull);
                                               
      if (isNull)
      {
        elog(ERROR, "tableoid is NULL");
      }
      tableoid = DatumGetObjectId(datum);

      Assert(OidIsValid(erm->relid));
      if (tableoid != erm->relid)
      {
                                              
        erm->ermActive = false;
        ItemPointerSetInvalid(&(erm->curCtid));
        ExecClearTuple(markSlot);
        continue;
      }
    }
    erm->ermActive = true;

                                
    datum = ExecGetJunkAttribute(slot, aerm->ctidAttNo, &isNull);
                                             
    if (isNull)
    {
      elog(ERROR, "ctid is NULL");
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

      fdwroutine->RefetchForeignRow(estate, erm, datum, markSlot, &updated);
      if (TupIsNull(markSlot))
      {
                                                     
        goto lnext;
      }

         
                                                                         
                                                                 
         
      if (updated)
      {
        epq_needed = true;
      }

      continue;
    }

                                                 
    tid = *((ItemPointer)DatumGetPointer(datum));
    switch (erm->markType)
    {
    case ROW_MARK_EXCLUSIVE:
      lockmode = LockTupleExclusive;
      break;
    case ROW_MARK_NOKEYEXCLUSIVE:
      lockmode = LockTupleNoKeyExclusive;
      break;
    case ROW_MARK_SHARE:
      lockmode = LockTupleShare;
      break;
    case ROW_MARK_KEYSHARE:
      lockmode = LockTupleKeyShare;
      break;
    default:
      elog(ERROR, "unsupported rowmark type");
      lockmode = LockTupleNoKeyExclusive;                          
      break;
    }

    lockflags = TUPLE_LOCK_FLAG_LOCK_UPDATE_IN_PROGRESS;
    if (!IsolationUsesXactSnapshot())
    {
      lockflags |= TUPLE_LOCK_FLAG_FIND_LAST_VERSION;
    }

    test = table_tuple_lock(erm->relation, &tid, estate->es_snapshot, markSlot, estate->es_output_cid, lockmode, erm->waitPolicy, lockflags, &tmfd);

    switch (test)
    {
    case TM_WouldBlock:
                                                   
      goto lnext;

    case TM_SelfModified:

         
                                                                
                                                               
                                                                
                                                                  
                                                                   
                                                                
                                                             
                                                                 
                                                                   
                                                                     
                                                                 
         
      goto lnext;

    case TM_Ok:

         
                                                              
                                                              
         
      if (tmfd.traversed)
      {
        epq_needed = true;
      }
      break;

    case TM_Updated:
      if (IsolationUsesXactSnapshot())
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent update")));
      }
      elog(ERROR, "unexpected table_tuple_lock status: %u", test);
      break;

    case TM_Deleted:
      if (IsolationUsesXactSnapshot())
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to concurrent update")));
      }
                                                
      goto lnext;

    case TM_Invisible:
      elog(ERROR, "attempted to lock invisible tuple");
      break;

    default:
      elog(ERROR, "unrecognized table_tuple_lock status: %u", test);
    }

                                                                          
    erm->curCtid = tid;
  }

     
                                                   
     
  if (epq_needed)
  {
                                  
    EvalPlanQualBegin(&node->lr_epqstate);

       
                                                                          
                                            
       
    EvalPlanQualSetSlot(&node->lr_epqstate, slot);

       
                                                 
       
    slot = EvalPlanQualNext(&node->lr_epqstate);
    if (TupIsNull(slot))
    {
                                                            
      goto lnext;
    }
  }

                                                  
  return slot;
}

                                                                    
                     
   
                                                            
                        
                                                                    
   
LockRowsState *
ExecInitLockRows(LockRows *node, EState *estate, int eflags)
{
  LockRowsState *lrstate;
  Plan *outerPlan = outerPlan(node);
  List *epq_arowmarks;
  ListCell *lc;

                                   
  Assert(!(eflags & EXEC_FLAG_MARK));

     
                            
     
  lrstate = makeNode(LockRowsState);
  lrstate->ps.plan = (Plan *)node;
  lrstate->ps.state = estate;
  lrstate->ps.ExecProcNode = ExecLockRows;

     
                                  
     
                                                                     
                            
     

     
                             
     
  ExecInitResultTypeTL(&lrstate->ps);

     
                                
     
  outerPlanState(lrstate) = ExecInitNode(outerPlan, estate, eflags);

                                                         
  lrstate->ps.resultopsset = true;
  lrstate->ps.resultops = ExecGetResultSlotOps(outerPlanState(lrstate), &lrstate->ps.resultopsfixed);

     
                                                                         
                             
     
  lrstate->ps.ps_ProjInfo = NULL;

     
                                                                      
                                                                        
                                             
     
  lrstate->lr_arowMarks = NIL;
  epq_arowmarks = NIL;
  foreach (lc, node->rowMarks)
  {
    PlanRowMark *rc = lfirst_node(PlanRowMark, lc);
    ExecRowMark *erm;
    ExecAuxRowMark *aerm;

                                                                  
    if (rc->isParent)
    {
      continue;
    }

                                                   
    erm = ExecFindRowMark(estate, rc->rti, false);
    aerm = ExecBuildAuxRowMark(erm, outerPlan->targetlist);

       
                                                                          
                                                                           
                                                                          
                          
       
    if (RowMarkRequiresRowShareLock(erm->markType))
    {
      lrstate->lr_arowMarks = lappend(lrstate->lr_arowMarks, aerm);
    }
    else
    {
      epq_arowmarks = lappend(epq_arowmarks, aerm);
    }
  }

                                                       
  EvalPlanQualInit(&lrstate->lr_epqstate, estate, outerPlan, epq_arowmarks, node->epqParam);

  return lrstate;
}

                                                                    
                    
   
                                                              
                  
                                                                    
   
void
ExecEndLockRows(LockRowsState *node)
{
  EvalPlanQualEnd(&node->lr_epqstate);
  ExecEndNode(outerPlanState(node));
}

void
ExecReScanLockRows(LockRowsState *node)
{
     
                                                                        
                         
     
  if (node->ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->ps.lefttree);
  }
}
