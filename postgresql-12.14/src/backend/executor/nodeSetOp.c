                                                                            
   
               
                                                       
   
                                                                    
                                                                          
                                                                          
                                                                       
                                                                         
                                                                        
                                                                            
                                                                 
   
                                                                        
                                                                               
                                                                             
                                                                           
                                                                             
                                                                              
                                                                           
                                                                          
                                                                              
                                                                  
   
                                                                         
                                                                       
                                  
   
                                                                        
                                                                      
                
   
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "executor/executor.h"
#include "executor/nodeSetOp.h"
#include "miscadmin.h"
#include "utils/memutils.h"

   
                                                    
   
                                                                      
                                                          
   
                                                                             
                                                                           
                                  
   
typedef struct SetOpStatePerGroupData
{
  long numLeft;                                          
  long numRight;                                          
} SetOpStatePerGroupData;

static TupleTableSlot *
setop_retrieve_direct(SetOpState *setopstate);
static void
setop_fill_hash_table(SetOpState *setopstate);
static TupleTableSlot *
setop_retrieve_hash_table(SetOpState *setopstate);

   
                                                     
   
static inline void
initialize_counts(SetOpStatePerGroup pergroup)
{
  pergroup->numLeft = pergroup->numRight = 0;
}

   
                                                        
   
static inline void
advance_counts(SetOpStatePerGroup pergroup, int flag)
{
  if (flag)
  {
    pergroup->numRight++;
  }
  else
  {
    pergroup->numLeft++;
  }
}

   
                                                
                                                                           
   
static int
fetch_tuple_flag(SetOpState *setopstate, TupleTableSlot *inputslot)
{
  SetOp *node = (SetOp *)setopstate->ps.plan;
  int flag;
  bool isNull;

  flag = DatumGetInt32(slot_getattr(inputslot, node->flagColIdx, &isNull));
  Assert(!isNull);
  Assert(flag == 0 || flag == 1);
  return flag;
}

   
                                       
   
static void
build_hash_table(SetOpState *setopstate)
{
  SetOp *node = (SetOp *)setopstate->ps.plan;
  ExprContext *econtext = setopstate->ps.ps_ExprContext;
  TupleDesc desc = ExecGetResultType(outerPlanState(setopstate));

  Assert(node->strategy == SETOP_HASHED);
  Assert(node->numGroups > 0);

  setopstate->hashtable = BuildTupleHashTableExt(&setopstate->ps, desc, node->numCols, node->dupColIdx, setopstate->eqfuncoids, setopstate->hashfunctions, node->dupCollations, node->numGroups, 0, setopstate->ps.state->es_query_cxt, setopstate->tableContext, econtext->ecxt_per_tuple_memory, false);
}

   
                                                                              
                                                                          
                                                        
   
static void
set_output_count(SetOpState *setopstate, SetOpStatePerGroup pergroup)
{
  SetOp *plannode = (SetOp *)setopstate->ps.plan;

  switch (plannode->cmd)
  {
  case SETOPCMD_INTERSECT:
    if (pergroup->numLeft > 0 && pergroup->numRight > 0)
    {
      setopstate->numOutput = 1;
    }
    else
    {
      setopstate->numOutput = 0;
    }
    break;
  case SETOPCMD_INTERSECT_ALL:
    setopstate->numOutput = (pergroup->numLeft < pergroup->numRight) ? pergroup->numLeft : pergroup->numRight;
    break;
  case SETOPCMD_EXCEPT:
    if (pergroup->numLeft > 0 && pergroup->numRight == 0)
    {
      setopstate->numOutput = 1;
    }
    else
    {
      setopstate->numOutput = 0;
    }
    break;
  case SETOPCMD_EXCEPT_ALL:
    setopstate->numOutput = (pergroup->numLeft < pergroup->numRight) ? 0 : (pergroup->numLeft - pergroup->numRight);
    break;
  default:
    elog(ERROR, "unrecognized set op: %d", (int)plannode->cmd);
    break;
  }
}

                                                                    
              
                                                                    
   
static TupleTableSlot *                              
ExecSetOp(PlanState *pstate)
{
  SetOpState *node = castNode(SetOpState, pstate);
  SetOp *plannode = (SetOp *)node->ps.plan;
  TupleTableSlot *resultTupleSlot = node->ps.ps_ResultTupleSlot;

  CHECK_FOR_INTERRUPTS();

     
                                                                           
                        
     
  if (node->numOutput > 0)
  {
    node->numOutput--;
    return resultTupleSlot;
  }

                                                     
  if (node->setop_done)
  {
    return NULL;
  }

                                                                    
  if (plannode->strategy == SETOP_HASHED)
  {
    if (!node->table_filled)
    {
      setop_fill_hash_table(node);
    }
    return setop_retrieve_hash_table(node);
  }
  else
  {
    return setop_retrieve_direct(node);
  }
}

   
                                 
   
static TupleTableSlot *
setop_retrieve_direct(SetOpState *setopstate)
{
  PlanState *outerPlan;
  SetOpStatePerGroup pergroup;
  TupleTableSlot *outerslot;
  TupleTableSlot *resultTupleSlot;
  ExprContext *econtext = setopstate->ps.ps_ExprContext;

     
                              
     
  outerPlan = outerPlanState(setopstate);
  pergroup = (SetOpStatePerGroup)setopstate->pergroup;
  resultTupleSlot = setopstate->ps.ps_ResultTupleSlot;

     
                                                                  
     
  while (!setopstate->setop_done)
  {
       
                                                                           
                            
       
    if (setopstate->grp_firstTuple == NULL)
    {
      outerslot = ExecProcNode(outerPlan);
      if (!TupIsNull(outerslot))
      {
                                                  
        setopstate->grp_firstTuple = ExecCopySlotHeapTuple(outerslot);
      }
      else
      {
                                                  
        setopstate->setop_done = true;
        return NULL;
      }
    }

       
                                                                           
                                                                      
             
       
    ExecStoreHeapTuple(setopstate->grp_firstTuple, resultTupleSlot, true);
    setopstate->grp_firstTuple = NULL;                              

                                                              
    initialize_counts(pergroup);

                                     
    advance_counts(pergroup, fetch_tuple_flag(setopstate, resultTupleSlot));

       
                                                                          
       
    for (;;)
    {
      outerslot = ExecProcNode(outerPlan);
      if (TupIsNull(outerslot))
      {
                                                 
        setopstate->setop_done = true;
        break;
      }

         
                                                       
         
      econtext->ecxt_outertuple = resultTupleSlot;
      econtext->ecxt_innertuple = outerslot;

      if (!ExecQualAndReset(setopstate->eqfunction, econtext))
      {
           
                                                         
           
        setopstate->grp_firstTuple = ExecCopySlotHeapTuple(outerslot);
        break;
      }

                                                    
      advance_counts(pergroup, fetch_tuple_flag(setopstate, outerslot));
    }

       
                                                                          
                                                         
       
    set_output_count(setopstate, pergroup);

    if (setopstate->numOutput > 0)
    {
      setopstate->numOutput--;
      return resultTupleSlot;
    }
  }

                      
  ExecClearTuple(resultTupleSlot);
  return NULL;
}

   
                                                                       
   
static void
setop_fill_hash_table(SetOpState *setopstate)
{
  SetOp *node = (SetOp *)setopstate->ps.plan;
  PlanState *outerPlan;
  int firstFlag;
  bool in_first_rel PG_USED_FOR_ASSERTS_ONLY;
  ExprContext *econtext = setopstate->ps.ps_ExprContext;

     
                              
     
  outerPlan = outerPlanState(setopstate);
  firstFlag = node->firstFlag;
                                     
  Assert(firstFlag == 0 || (firstFlag == 1 && (node->cmd == SETOPCMD_INTERSECT || node->cmd == SETOPCMD_INTERSECT_ALL)));

     
                                                                          
                             
     
  in_first_rel = true;
  for (;;)
  {
    TupleTableSlot *outerslot;
    int flag;
    TupleHashEntryData *entry;
    bool isnew;

    outerslot = ExecProcNode(outerPlan);
    if (TupIsNull(outerslot))
    {
      break;
    }

                                                   
    flag = fetch_tuple_flag(setopstate, outerslot);

    if (flag == firstFlag)
    {
                                           
      Assert(in_first_rel);

                                                                
      entry = LookupTupleHashEntry(setopstate->hashtable, outerslot, &isnew);

                                                 
      if (isnew)
      {
        entry->additional = (SetOpStatePerGroup)MemoryContextAlloc(setopstate->hashtable->tablecxt, sizeof(SetOpStatePerGroupData));
        initialize_counts((SetOpStatePerGroup)entry->additional);
      }

                              
      advance_counts((SetOpStatePerGroup)entry->additional, flag);
    }
    else
    {
                                   
      in_first_rel = false;

                                                                       
      entry = LookupTupleHashEntry(setopstate->hashtable, outerslot, NULL);

                                                          
      if (entry)
      {
        advance_counts((SetOpStatePerGroup)entry->additional, flag);
      }
    }

                                                                   
    ResetExprContext(econtext);
  }

  setopstate->table_filled = true;
                                         
  ResetTupleHashIterator(setopstate->hashtable, &setopstate->hashiter);
}

   
                                                                         
   
static TupleTableSlot *
setop_retrieve_hash_table(SetOpState *setopstate)
{
  TupleHashEntryData *entry;
  TupleTableSlot *resultTupleSlot;

     
                              
     
  resultTupleSlot = setopstate->ps.ps_ResultTupleSlot;

     
                                                                  
     
  while (!setopstate->setop_done)
  {
    CHECK_FOR_INTERRUPTS();

       
                                             
       
    entry = ScanTupleHashTable(setopstate->hashtable, &setopstate->hashiter);
    if (entry == NULL)
    {
                                                 
      setopstate->setop_done = true;
      return NULL;
    }

       
                                                                        
                       
       
    set_output_count(setopstate, (SetOpStatePerGroup)entry->additional);

    if (setopstate->numOutput > 0)
    {
      setopstate->numOutput--;
      return ExecStoreMinimalTuple(entry->firstTuple, resultTupleSlot, false);
    }
  }

                      
  ExecClearTuple(resultTupleSlot);
  return NULL;
}

                                                                    
                  
   
                                                         
                        
                                                                    
   
SetOpState *
ExecInitSetOp(SetOp *node, EState *estate, int eflags)
{
  SetOpState *setopstate;
  TupleDesc outerDesc;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  setopstate = makeNode(SetOpState);
  setopstate->ps.plan = (Plan *)node;
  setopstate->ps.state = estate;
  setopstate->ps.ExecProcNode = ExecSetOp;

  setopstate->eqfuncoids = NULL;
  setopstate->hashfunctions = NULL;
  setopstate->setop_done = false;
  setopstate->numOutput = 0;
  setopstate->pergroup = NULL;
  setopstate->grp_firstTuple = NULL;
  setopstate->hashtable = NULL;
  setopstate->tableContext = NULL;

     
                               
     
  ExecAssignExprContext(estate, &setopstate->ps);

     
                                                                       
                                                                           
                                                             
     
  if (node->strategy == SETOP_HASHED)
  {
    setopstate->tableContext = AllocSetContextCreate(CurrentMemoryContext, "SetOp hash table", ALLOCSET_DEFAULT_SIZES);
  }

     
                            
     
                                                                          
                                       
     
  if (node->strategy == SETOP_HASHED)
  {
    eflags &= ~EXEC_FLAG_REWIND;
  }
  outerPlanState(setopstate) = ExecInitNode(outerPlan(node), estate, eflags);
  outerDesc = ExecGetResultType(outerPlanState(setopstate));

     
                                                                        
                                                             
     
  ExecInitResultTupleSlotTL(&setopstate->ps, node->strategy == SETOP_HASHED ? &TTSOpsMinimalTuple : &TTSOpsHeapTuple);
  setopstate->ps.ps_ProjInfo = NULL;

     
                                                                           
                                                                     
              
     
  if (node->strategy == SETOP_HASHED)
  {
    execTuplesHashPrepare(node->numCols, node->dupOperators, &setopstate->eqfuncoids, &setopstate->hashfunctions);
  }
  else
  {
    setopstate->eqfunction = execTuplesMatchPrepare(outerDesc, node->numCols, node->dupColIdx, node->dupOperators, node->dupCollations, &setopstate->ps);
  }

  if (node->strategy == SETOP_HASHED)
  {
    build_hash_table(setopstate);
    setopstate->table_filled = false;
  }
  else
  {
    setopstate->pergroup = (SetOpStatePerGroup)palloc0(sizeof(SetOpStatePerGroupData));
  }

  return setopstate;
}

                                                                    
                 
   
                                                              
                  
                                                                    
   
void
ExecEndSetOp(SetOpState *node)
{
                            
  ExecClearTuple(node->ps.ps_ResultTupleSlot);

                                                 
  if (node->tableContext)
  {
    MemoryContextDelete(node->tableContext);
  }
  ExecFreeExprContext(&node->ps);

  ExecEndNode(outerPlanState(node));
}

void
ExecReScanSetOp(SetOpState *node)
{
  ExecClearTuple(node->ps.ps_ResultTupleSlot);
  node->setop_done = false;
  node->numOutput = 0;

  if (((SetOp *)node->ps.plan)->strategy == SETOP_HASHED)
  {
       
                                                                          
                                                                           
                                                                        
                                            
       
    if (!node->table_filled)
    {
      return;
    }

       
                                                                      
                                                                           
                                  
       
    if (node->ps.lefttree->chgParam == NULL)
    {
      ResetTupleHashIterator(node->hashtable, &node->hashiter);
      return;
    }
  }

                                                            
  if (node->grp_firstTuple != NULL)
  {
    heap_freetuple(node->grp_firstTuple);
    node->grp_firstTuple = NULL;
  }

                                     
  if (node->tableContext)
  {
    MemoryContextResetAndDeleteChildren(node->tableContext);
  }

                                             
  if (((SetOp *)node->ps.plan)->strategy == SETOP_HASHED)
  {
    ResetTupleHashTable(node->hashtable);
    node->table_filled = false;
  }

     
                                                                        
                         
     
  if (node->ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->ps.lefttree);
  }
}
