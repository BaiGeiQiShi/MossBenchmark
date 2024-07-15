                                                                            
   
                  
                                        
   
                                                                         
                                                                        
   
   
                  
                                         
   
               
   
                                                                              
                                                                            
                                                                               
                                                                             
                                                                               
                                                                               
                                                                        
                                                                              
                                                       
   
                                                                             
                                                                              
                                                                            
                                                                           
                                                                             
                                                                             
                                                       
   
                                                                            
                                                                            
                                                                         
                                                                               
                                                                          
                                         
   
                                                                              
                                                                              
                                                                               
   
                                                      
                                                                            
                                                               
                                                                              
                                                                         
   
                                                                              
                                                                            
                                                     
   
                                                      
                                                                  
                                                        
                                                                    
   
                                                      
                                                                  
                                                          
   
                                                                              
                                                                             
                                                                              
                                                                            
                                                    
   
                                                                             
                                                                               
                                                                           
                                                                          
                                                                            
                                                        
   
                                                                      
                                                                               
                                                                           
                              
   
                                              
                                                      
                                                                  
                                          
                                    
   
                                                             
                                                                     
                                                   
   
                                                                            
                                                                              
                                                                           
                                                    
   
                                                                             
                                                                             
                                                                          
                                                                         
                                                                            
                                                                         
                                                                             
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "access/parallel.h"
#include "executor/executor.h"
#include "executor/hashjoin.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/sharedtuplestore.h"

   
                                            
   
#define HJ_BUILD_HASHTABLE 1
#define HJ_NEED_NEW_OUTER 2
#define HJ_SCAN_BUCKET 3
#define HJ_FILL_OUTER_TUPLE 4
#define HJ_FILL_INNER_TUPLES 5
#define HJ_NEED_NEW_BATCH 6

                                                       
#define HJ_FILL_OUTER(hjstate) ((hjstate)->hj_NullInnerTupleSlot != NULL)
                                                       
#define HJ_FILL_INNER(hjstate) ((hjstate)->hj_NullOuterTupleSlot != NULL)

static TupleTableSlot *
ExecHashJoinOuterGetTuple(PlanState *outerNode, HashJoinState *hjstate, uint32 *hashvalue);
static TupleTableSlot *
ExecParallelHashJoinOuterGetTuple(PlanState *outerNode, HashJoinState *hjstate, uint32 *hashvalue);
static TupleTableSlot *
ExecHashJoinGetSavedTuple(HashJoinState *hjstate, BufFile *file, uint32 *hashvalue, TupleTableSlot *tupleSlot);
static bool
ExecHashJoinNewBatch(HashJoinState *hjstate);
static bool
ExecParallelHashJoinNewBatch(HashJoinState *hjstate);
static void
ExecParallelHashJoinPartitionOuter(HashJoinState *node);

                                                                    
                     
   
                                                                          
                                                               
                                                                      
                                                                          
                                                         
   
                                                             
                                 
                                                                    
   
static pg_attribute_always_inline TupleTableSlot *
ExecHashJoinImpl(PlanState *pstate, bool parallel)
{
  HashJoinState *node = castNode(HashJoinState, pstate);
  PlanState *outerNode;
  HashState *hashNode;
  ExprState *joinqual;
  ExprState *otherqual;
  ExprContext *econtext;
  HashJoinTable hashtable;
  TupleTableSlot *outerTupleSlot;
  uint32 hashvalue;
  int batchno;
  ParallelHashJoinState *parallel_state;

     
                                        
     
  joinqual = node->js.joinqual;
  otherqual = node->js.ps.qual;
  hashNode = (HashState *)innerPlanState(node);
  outerNode = outerPlanState(node);
  hashtable = node->hj_HashTable;
  econtext = node->js.ps.ps_ExprContext;
  parallel_state = hashNode->parallel_state;

     
                                                                      
                                                    
     
  ResetExprContext(econtext);

     
                                     
     
  for (;;)
  {
       
                                                                        
                                                                         
                                                                          
                          
       
    CHECK_FOR_INTERRUPTS();

    switch (node->hj_JoinState)
    {
    case HJ_BUILD_HASHTABLE:

         
                                                                  
         
      Assert(hashtable == NULL);

         
                                                                 
                                                                
                                                                
                                                                   
                                                             
                                                                   
                                                                     
                                                                    
                                                                 
                           
         
                                                                   
                                                              
                                                                
                                                            
                                                             
                                             
         
                                                                   
                                                                    
                                                                     
         
      if (HJ_FILL_INNER(node))
      {
                                                   
        node->hj_FirstOuterTupleSlot = NULL;
      }
      else if (parallel)
      {
           
                                                               
                                                              
                                                                  
                                                                  
                                                                 
                                    
           
        node->hj_FirstOuterTupleSlot = NULL;
      }
      else if (HJ_FILL_OUTER(node) || (outerNode->plan->startup_cost < hashNode->ps.plan->total_cost && !node->hj_OuterNotEmpty))
      {
        node->hj_FirstOuterTupleSlot = ExecProcNode(outerNode);
        if (TupIsNull(node->hj_FirstOuterTupleSlot))
        {
          node->hj_OuterNotEmpty = false;
          return NULL;
        }
        else
        {
          node->hj_OuterNotEmpty = true;
        }
      }
      else
      {
        node->hj_FirstOuterTupleSlot = NULL;
      }

         
                                                              
                                                                    
                                                  
         
      hashtable = ExecHashTableCreate(hashNode, node->hj_HashOperators, node->hj_Collations, HJ_FILL_INNER(node));
      node->hj_HashTable = hashtable;

         
                                                                   
                                                                 
                           
         
      hashNode->hashtable = hashtable;
      (void)MultiExecProcNode((PlanState *)hashNode);

         
                                                                  
                                                                   
                         
         
      if (hashtable->totalTuples == 0 && !HJ_FILL_OUTER(node))
      {
        return NULL;
      }

         
                                                                
                                           
         
      hashtable->nbatch_outstart = hashtable->nbatch;

         
                                                                 
                                                             
                                    
         
      node->hj_OuterNotEmpty = false;

      if (parallel)
      {
        Barrier *build_barrier;

        build_barrier = &parallel_state->build_barrier;
        Assert(BarrierPhase(build_barrier) == PHJ_BUILD_HASHING_OUTER || BarrierPhase(build_barrier) == PHJ_BUILD_DONE);
        if (BarrierPhase(build_barrier) == PHJ_BUILD_HASHING_OUTER)
        {
             
                                                                
                       
             
          if (hashtable->nbatch > 1)
          {
            ExecParallelHashJoinPartitionOuter(node);
          }
          BarrierArriveAndWait(build_barrier, WAIT_EVENT_HASH_BUILD_HASHING_OUTER);
        }
        Assert(BarrierPhase(build_barrier) == PHJ_BUILD_DONE);

                                                                
        hashtable->curbatch = -1;
        node->hj_JoinState = HJ_NEED_NEW_BATCH;

        continue;
      }
      else
      {
        node->hj_JoinState = HJ_NEED_NEW_OUTER;
      }

                     

    case HJ_NEED_NEW_OUTER:

         
                                                               
         
      if (parallel)
      {
        outerTupleSlot = ExecParallelHashJoinOuterGetTuple(outerNode, node, &hashvalue);
      }
      else
      {
        outerTupleSlot = ExecHashJoinOuterGetTuple(outerNode, node, &hashvalue);
      }

      if (TupIsNull(outerTupleSlot))
      {
                                               
        if (HJ_FILL_INNER(node))
        {
                                                         
          ExecPrepHashTableForUnmatched(node);
          node->hj_JoinState = HJ_FILL_INNER_TUPLES;
        }
        else
        {
          node->hj_JoinState = HJ_NEED_NEW_BATCH;
        }
        continue;
      }

      econtext->ecxt_outertuple = outerTupleSlot;
      node->hj_MatchedOuter = false;

         
                                                                  
                                        
         
      node->hj_CurHashValue = hashvalue;
      ExecHashGetBucketAndBatch(hashtable, hashvalue, &node->hj_CurBucketNo, &batchno);
      node->hj_CurSkewBucketNo = ExecHashGetSkewBucket(hashtable, hashvalue);
      node->hj_CurTuple = NULL;

         
                                                                
                                                            
         
      if (batchno != hashtable->curbatch && node->hj_CurSkewBucketNo == INVALID_SKEW_BUCKET_NO)
      {
        bool shouldFree;
        MinimalTuple mintuple = ExecFetchSlotMinimalTuple(outerTupleSlot, &shouldFree);

           
                                                               
                                                          
           
        Assert(parallel_state == NULL);
        Assert(batchno > hashtable->curbatch);
        ExecHashJoinSaveTuple(mintuple, hashvalue, &hashtable->outerBatchFile[batchno]);

        if (shouldFree)
        {
          heap_free_minimal_tuple(mintuple);
        }

                                                             
        continue;
      }

                                                 
      node->hj_JoinState = HJ_SCAN_BUCKET;

                     

    case HJ_SCAN_BUCKET:

         
                                                                    
         
      if (parallel)
      {
        if (!ExecParallelScanHashBucket(node, econtext))
        {
                                                                  
          node->hj_JoinState = HJ_FILL_OUTER_TUPLE;
          continue;
        }
      }
      else
      {
        if (!ExecScanHashBucket(node, econtext))
        {
                                                                  
          node->hj_JoinState = HJ_FILL_OUTER_TUPLE;
          continue;
        }
      }

         
                                                                     
                                                                   
                        
         
                                                                     
                                                                
                                     
         
                                                                  
                                                       
         
      if (joinqual == NULL || ExecQual(joinqual, econtext))
      {
        node->hj_MatchedOuter = true;

        if (parallel)
        {
             
                                                                
                                                             
                                                          
                                                         
                      
             
          Assert(!HJ_FILL_INNER(node));
        }
        else
        {
             
                                                                
                                                                
             
          HeapTupleHeaderSetMatch(HJTUPLE_MINTUPLE(node->hj_CurTuple));
        }

                                                             
        if (node->js.jointype == JOIN_ANTI)
        {
          node->hj_JoinState = HJ_NEED_NEW_OUTER;
          continue;
        }

           
                                                               
                                                                   
                                           
           
        if (node->js.single_match)
        {
          node->hj_JoinState = HJ_NEED_NEW_OUTER;
        }

        if (otherqual == NULL || ExecQual(otherqual, econtext))
        {
          return ExecProject(node->js.ps.ps_ProjInfo);
        }
        else
        {
          InstrCountFiltered2(node, 1);
        }
      }
      else
      {
        InstrCountFiltered1(node, 1);
      }
      break;

    case HJ_FILL_OUTER_TUPLE:

         
                                                                  
                                                                    
                                                       
         
      node->hj_JoinState = HJ_NEED_NEW_OUTER;

      if (!node->hj_MatchedOuter && HJ_FILL_OUTER(node))
      {
           
                                                               
                                                                 
           
        econtext->ecxt_innertuple = node->hj_NullInnerTupleSlot;

        if (otherqual == NULL || ExecQual(otherqual, econtext))
        {
          return ExecProject(node->js.ps.ps_ProjInfo);
        }
        else
        {
          InstrCountFiltered2(node, 1);
        }
      }
      break;

    case HJ_FILL_INNER_TUPLES:

         
                                                                     
                                                                   
                                                       
         
      if (!ExecScanHashTableForUnmatched(node, econtext))
      {
                                      
        node->hj_JoinState = HJ_NEED_NEW_BATCH;
        continue;
      }

         
                                                                    
                                                        
         
      econtext->ecxt_outertuple = node->hj_NullOuterTupleSlot;

      if (otherqual == NULL || ExecQual(otherqual, econtext))
      {
        return ExecProject(node->js.ps.ps_ProjInfo);
      }
      else
      {
        InstrCountFiltered2(node, 1);
      }
      break;

    case HJ_NEED_NEW_BATCH:

         
                                                                   
         
      if (parallel)
      {
        if (!ExecParallelHashJoinNewBatch(node))
        {
          return NULL;                                 
        }
      }
      else
      {
        if (!ExecHashJoinNewBatch(node))
        {
          return NULL;                                     
        }
      }
      node->hj_JoinState = HJ_NEED_NEW_OUTER;
      break;

    default:
      elog(ERROR, "unrecognized hashjoin state: %d", (int)node->hj_JoinState);
    }
  }
}

                                                                    
                 
   
                                
                                                                    
   
static TupleTableSlot *                              
ExecHashJoin(PlanState *pstate)
{
     
                                                                     
                                      
     
  return ExecHashJoinImpl(pstate, false);
}

                                                                    
                         
   
                            
                                                                    
   
static TupleTableSlot *                              
ExecParallelHashJoin(PlanState *pstate)
{
     
                                                                     
                                          
     
  return ExecHashJoinImpl(pstate, true);
}

                                                                    
                     
   
                                    
                                                                    
   
HashJoinState *
ExecInitHashJoin(HashJoin *node, EState *estate, int eflags)
{
  HashJoinState *hjstate;
  Plan *outerNode;
  Hash *hashNode;
  TupleDesc outerDesc, innerDesc;
  const TupleTableSlotOps *ops;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  hjstate = makeNode(HashJoinState);
  hjstate->js.ps.plan = (Plan *)node;
  hjstate->js.ps.state = estate;

     
                                                                        
                                                                        
                                         
     
  hjstate->js.ps.ExecProcNode = ExecHashJoin;
  hjstate->js.jointype = node->join.jointype;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &hjstate->js.ps);

     
                            
     
                                                                        
                                                                        
                                          
     
  outerNode = outerPlan(node);
  hashNode = (Hash *)innerPlan(node);

  outerPlanState(hjstate) = ExecInitNode(outerNode, estate, eflags);
  outerDesc = ExecGetResultType(outerPlanState(hjstate));
  innerPlanState(hjstate) = ExecInitNode((Plan *)hashNode, estate, eflags);
  innerDesc = ExecGetResultType(innerPlanState(hjstate));

     
                                                  
     
  ExecInitResultTupleSlotTL(&hjstate->js.ps, &TTSOpsVirtual);
  ExecAssignProjectionInfo(&hjstate->js.ps, NULL);

     
                                
     
  ops = ExecGetResultSlotOps(outerPlanState(hjstate), NULL);
  hjstate->hj_OuterTupleSlot = ExecInitExtraTupleSlot(estate, outerDesc, ops);

     
                                                                         
     
  hjstate->js.single_match = (node->join.inner_unique || node->join.jointype == JOIN_SEMI);

                                                     
  switch (node->join.jointype)
  {
  case JOIN_INNER:
  case JOIN_SEMI:
    break;
  case JOIN_LEFT:
  case JOIN_ANTI:
    hjstate->hj_NullInnerTupleSlot = ExecInitNullTupleSlot(estate, innerDesc, &TTSOpsVirtual);
    break;
  case JOIN_RIGHT:
    hjstate->hj_NullOuterTupleSlot = ExecInitNullTupleSlot(estate, outerDesc, &TTSOpsVirtual);
    break;
  case JOIN_FULL:
    hjstate->hj_NullOuterTupleSlot = ExecInitNullTupleSlot(estate, outerDesc, &TTSOpsVirtual);
    hjstate->hj_NullInnerTupleSlot = ExecInitNullTupleSlot(estate, innerDesc, &TTSOpsVirtual);
    break;
  default:
    elog(ERROR, "unrecognized join type: %d", (int)node->join.jointype);
  }

     
                                                                           
                                                                            
                                                                          
                                                                            
                                  
     
  {
    HashState *hashstate = (HashState *)innerPlanState(hjstate);
    TupleTableSlot *slot = hashstate->ps.ps_ResultTupleSlot;

    hjstate->hj_HashTupleSlot = slot;
  }

     
                                  
     
  hjstate->js.ps.qual = ExecInitQual(node->join.plan.qual, (PlanState *)hjstate);
  hjstate->js.joinqual = ExecInitQual(node->join.joinqual, (PlanState *)hjstate);
  hjstate->hashclauses = ExecInitQual(node->hashclauses, (PlanState *)hjstate);

     
                                   
     
  hjstate->hj_HashTable = NULL;
  hjstate->hj_FirstOuterTupleSlot = NULL;

  hjstate->hj_CurHashValue = 0;
  hjstate->hj_CurBucketNo = 0;
  hjstate->hj_CurSkewBucketNo = INVALID_SKEW_BUCKET_NO;
  hjstate->hj_CurTuple = NULL;

  hjstate->hj_OuterHashKeys = ExecInitExprList(node->hashkeys, (PlanState *)hjstate);
  hjstate->hj_HashOperators = node->hashoperators;
  hjstate->hj_Collations = node->hashcollations;

  hjstate->hj_JoinState = HJ_BUILD_HASHTABLE;
  hjstate->hj_MatchedOuter = false;
  hjstate->hj_OuterNotEmpty = false;

  return hjstate;
}

                                                                    
                    
   
                                       
                                                                    
   
void
ExecEndHashJoin(HashJoinState *node)
{
     
                     
     
  if (node->hj_HashTable)
  {
    ExecHashTableDestroy(node->hj_HashTable);
    node->hj_HashTable = NULL;
  }

     
                          
     
  ExecFreeExprContext(&node->js.ps);

     
                               
     
  ExecClearTuple(node->js.ps.ps_ResultTupleSlot);
  ExecClearTuple(node->hj_OuterTupleSlot);
  ExecClearTuple(node->hj_HashTupleSlot);

     
                       
     
  ExecEndNode(outerPlanState(node));
  ExecEndNode(innerPlanState(node));
}

   
                             
   
                                                                          
                                                                      
                                    
   
                                                                           
   
                                                                          
                                                              
   
static TupleTableSlot *
ExecHashJoinOuterGetTuple(PlanState *outerNode, HashJoinState *hjstate, uint32 *hashvalue)
{
  HashJoinTable hashtable = hjstate->hj_HashTable;
  int curbatch = hashtable->curbatch;
  TupleTableSlot *slot;

  if (curbatch == 0)                              
  {
       
                                                                
                                        
       
    slot = hjstate->hj_FirstOuterTupleSlot;
    if (!TupIsNull(slot))
    {
      hjstate->hj_FirstOuterTupleSlot = NULL;
    }
    else
    {
      slot = ExecProcNode(outerNode);
    }

    while (!TupIsNull(slot))
    {
         
                                                    
         
      ExprContext *econtext = hjstate->js.ps.ps_ExprContext;

      econtext->ecxt_outertuple = slot;
      if (ExecHashGetHashValue(hashtable, econtext, hjstate->hj_OuterHashKeys, true,                  
              HJ_FILL_OUTER(hjstate), hashvalue))
      {
                                                                      
        hjstate->hj_OuterNotEmpty = true;

        return slot;
      }

         
                                                                        
                                     
         
      slot = ExecProcNode(outerNode);
    }
  }
  else if (curbatch < hashtable->nbatch)
  {
    BufFile *file = hashtable->outerBatchFile[curbatch];

       
                                                                         
                 
       
    if (file == NULL)
    {
      return NULL;
    }

    slot = ExecHashJoinGetSavedTuple(hjstate, file, hashvalue, hjstate->hj_OuterTupleSlot);
    if (!TupIsNull(slot))
    {
      return slot;
    }
  }

                         
  return NULL;
}

   
                                                            
   
static TupleTableSlot *
ExecParallelHashJoinOuterGetTuple(PlanState *outerNode, HashJoinState *hjstate, uint32 *hashvalue)
{
  HashJoinTable hashtable = hjstate->hj_HashTable;
  int curbatch = hashtable->curbatch;
  TupleTableSlot *slot;

     
                                                                       
                                                                            
                  
     
  if (curbatch == 0 && hashtable->nbatch == 1)
  {
    slot = ExecProcNode(outerNode);

    while (!TupIsNull(slot))
    {
      ExprContext *econtext = hjstate->js.ps.ps_ExprContext;

      econtext->ecxt_outertuple = slot;
      if (ExecHashGetHashValue(hashtable, econtext, hjstate->hj_OuterHashKeys, true,                  
              HJ_FILL_OUTER(hjstate), hashvalue))
      {
        return slot;
      }

         
                                                                        
                                     
         
      slot = ExecProcNode(outerNode);
    }
  }
  else if (curbatch < hashtable->nbatch)
  {
    MinimalTuple tuple;

    tuple = sts_parallel_scan_next(hashtable->batches[curbatch].outer_tuples, hashvalue);
    if (tuple != NULL)
    {
      ExecForceStoreMinimalTuple(tuple, hjstate->hj_OuterTupleSlot, false);
      slot = hjstate->hj_OuterTupleSlot;
      return slot;
    }
    else
    {
      ExecClearTuple(hjstate->hj_OuterTupleSlot);
    }
  }

                         
  return NULL;
}

   
                        
                                   
   
                                                                   
   
static bool
ExecHashJoinNewBatch(HashJoinState *hjstate)
{
  HashJoinTable hashtable = hjstate->hj_HashTable;
  int nbatch;
  int curbatch;
  BufFile *innerFile;
  TupleTableSlot *slot;
  uint32 hashvalue;

  nbatch = hashtable->nbatch;
  curbatch = hashtable->curbatch;

  if (curbatch > 0)
  {
       
                                                                       
                                
       
    if (hashtable->outerBatchFile[curbatch])
    {
      BufFileClose(hashtable->outerBatchFile[curbatch]);
    }
    hashtable->outerBatchFile[curbatch] = NULL;
  }
  else                                       
  {
       
                                                                        
                                                                      
                                                                     
                         
       
    hashtable->skewEnabled = false;
    hashtable->skewBucket = NULL;
    hashtable->skewBucketNums = NULL;
    hashtable->nSkewBuckets = 0;
    hashtable->spaceUsedSkew = 0;
  }

     
                                                                           
                                                                           
                                     
     
                                                                            
                                                                          
                                                                     
     
                                                                           
                                                                        
                                        
     
                                                                        
                                                                            
                            
     
  curbatch++;
  while (curbatch < nbatch && (hashtable->outerBatchFile[curbatch] == NULL || hashtable->innerBatchFile[curbatch] == NULL))
  {
    if (hashtable->outerBatchFile[curbatch] && HJ_FILL_OUTER(hjstate))
    {
      break;                                 
    }
    if (hashtable->innerBatchFile[curbatch] && HJ_FILL_INNER(hjstate))
    {
      break;                                 
    }
    if (hashtable->innerBatchFile[curbatch] && nbatch != hashtable->nbatch_original)
    {
      break;                                 
    }
    if (hashtable->outerBatchFile[curbatch] && nbatch != hashtable->nbatch_outstart)
    {
      break;                                 
    }
                                   
                                                   
    if (hashtable->innerBatchFile[curbatch])
    {
      BufFileClose(hashtable->innerBatchFile[curbatch]);
    }
    hashtable->innerBatchFile[curbatch] = NULL;
    if (hashtable->outerBatchFile[curbatch])
    {
      BufFileClose(hashtable->outerBatchFile[curbatch]);
    }
    hashtable->outerBatchFile[curbatch] = NULL;
    curbatch++;
  }

  if (curbatch >= nbatch)
  {
    return false;                      
  }

  hashtable->curbatch = curbatch;

     
                                                                           
     
  ExecHashTableReset(hashtable);

  innerFile = hashtable->innerBatchFile[curbatch];

  if (innerFile != NULL)
  {
    if (BufFileSeek(innerFile, 0, 0L, SEEK_SET))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not rewind hash-join temporary file")));
    }

    while ((slot = ExecHashJoinGetSavedTuple(hjstate, innerFile, &hashvalue, hjstate->hj_HashTupleSlot)))
    {
         
                                                                       
                                                              
         
      ExecHashTableInsert(hashtable, slot, hashvalue);
    }

       
                                                                        
              
       
    BufFileClose(innerFile);
    hashtable->innerBatchFile[curbatch] = NULL;
  }

     
                                                                            
     
  if (hashtable->outerBatchFile[curbatch] != NULL)
  {
    if (BufFileSeek(hashtable->outerBatchFile[curbatch], 0, 0L, SEEK_SET))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not rewind hash-join temporary file")));
    }
  }

  return true;
}

   
                                                                             
                                       
   
static bool
ExecParallelHashJoinNewBatch(HashJoinState *hjstate)
{
  HashJoinTable hashtable = hjstate->hj_HashTable;
  int start_batchno;
  int batchno;

     
                                                                           
                                                                       
                                             
     
  if (hashtable->batches == NULL)
  {
    return false;
  }

     
                                                                             
                                                                             
                      
     
  if (hashtable->curbatch >= 0)
  {
    hashtable->batches[hashtable->curbatch].done = true;
    ExecHashTableDetachBatch(hashtable);
  }

     
                                                                            
                                                                         
                                     
     
  batchno = start_batchno = pg_atomic_fetch_add_u32(&hashtable->parallel_state->distributor, 1) % hashtable->nbatch;
  do
  {
    uint32 hashvalue;
    MinimalTuple tuple;
    TupleTableSlot *slot;

    if (!hashtable->batches[batchno].done)
    {
      SharedTuplestoreAccessor *inner_tuples;
      Barrier *batch_barrier = &hashtable->batches[batchno].shared->batch_barrier;

      switch (BarrierAttach(batch_barrier))
      {
      case PHJ_BATCH_ELECTING:

                                                   
        if (BarrierArriveAndWait(batch_barrier, WAIT_EVENT_HASH_BATCH_ELECTING))
        {
          ExecParallelHashTableAlloc(hashtable, batchno);
        }
                           

      case PHJ_BATCH_ALLOCATING:
                                              
        BarrierArriveAndWait(batch_barrier, WAIT_EVENT_HASH_BATCH_ALLOCATING);
                           

      case PHJ_BATCH_LOADING:
                                                
        ExecParallelHashTableSetCurrentBatch(hashtable, batchno);
        inner_tuples = hashtable->batches[batchno].inner_tuples;
        sts_begin_parallel_scan(inner_tuples);
        while ((tuple = sts_parallel_scan_next(inner_tuples, &hashvalue)))
        {
          ExecForceStoreMinimalTuple(tuple, hjstate->hj_HashTupleSlot, false);
          slot = hjstate->hj_HashTupleSlot;
          ExecParallelHashTableInsertCurrentBatch(hashtable, slot, hashvalue);
        }
        sts_end_parallel_scan(inner_tuples);
        BarrierArriveAndWait(batch_barrier, WAIT_EVENT_HASH_BATCH_LOADING);
                           

      case PHJ_BATCH_PROBING:

           
                                                            
                                                                 
                                                            
                                                                
                                                                
                                                          
                                                            
                                          
           
        ExecParallelHashTableSetCurrentBatch(hashtable, batchno);
        sts_begin_parallel_scan(hashtable->batches[batchno].outer_tuples);
        return true;

      case PHJ_BATCH_DONE:

           
                                                             
                    
           
        BarrierDetach(batch_barrier);
        hashtable->batches[batchno].done = true;
        hashtable->curbatch = -1;
        break;

      default:
        elog(ERROR, "unexpected batch phase %d", BarrierPhase(batch_barrier));
      }
    }
    batchno = (batchno + 1) % hashtable->nbatch;
  } while (batchno != start_batchno);

  return false;
}

   
                         
                                  
   
                                                                   
                                          
   
                                                                     
                                                                       
                       
   
void
ExecHashJoinSaveTuple(MinimalTuple tuple, uint32 hashvalue, BufFile **fileptr)
{
  BufFile *file = *fileptr;

  if (file == NULL)
  {
                                                     
    file = BufFileCreateTemp(false);
    *fileptr = file;
  }

  BufFileWrite(file, (void *)&hashvalue, sizeof(uint32));
  BufFileWrite(file, (void *)tuple, tuple->t_len);
}

   
                             
                                                                    
   
                                                                          
                                       
   
static TupleTableSlot *
ExecHashJoinGetSavedTuple(HashJoinState *hjstate, BufFile *file, uint32 *hashvalue, TupleTableSlot *tupleSlot)
{
  uint32 header[2];
  size_t nread;
  MinimalTuple tuple;

     
                                                                        
                                                                          
                   
     
  CHECK_FOR_INTERRUPTS();

     
                                                                            
                                                                      
               
     
  nread = BufFileRead(file, (void *)header, sizeof(header));
  if (nread == 0)                  
  {
    ExecClearTuple(tupleSlot);
    return NULL;
  }
  if (nread != sizeof(header))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from hash-join temporary file: read only %zu of %zu bytes", nread, sizeof(header))));
  }
  *hashvalue = header[0];
  tuple = (MinimalTuple)palloc(header[1]);
  tuple->t_len = header[1];
  nread = BufFileRead(file, (void *)((char *)tuple + sizeof(uint32)), header[1] - sizeof(uint32));
  if (nread != header[1] - sizeof(uint32))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from hash-join temporary file: read only %zu of %zu bytes", nread, header[1] - sizeof(uint32))));
  }
  ExecForceStoreMinimalTuple(tuple, tupleSlot, true);
  return tupleSlot;
}

void
ExecReScanHashJoin(HashJoinState *node)
{
     
                                                                          
                                                                            
                                                                           
                                                                            
                    
     
  if (node->hj_HashTable != NULL)
  {
    if (node->hj_HashTable->nbatch == 1 && node->js.ps.righttree->chgParam == NULL)
    {
         
                                                                     
         
                                                                   
                                                         
         
      if (HJ_FILL_INNER(node))
      {
        ExecHashTableResetMatchFlags(node->hj_HashTable);
      }

         
                                                                     
                                                                       
                                                                         
                                                                     
                                                                       
                                                                    
                                                            
         
      node->hj_OuterNotEmpty = false;

                                                          
      node->hj_JoinState = HJ_NEED_NEW_OUTER;
    }
    else
    {
                                               
      HashState *hashNode = castNode(HashState, innerPlanState(node));

                                                                      
      Assert(hashNode->hashtable == node->hj_HashTable);
      hashNode->hashtable = NULL;

      ExecHashTableDestroy(node->hj_HashTable);
      node->hj_HashTable = NULL;
      node->hj_JoinState = HJ_BUILD_HASHTABLE;

         
                                                                         
                                
         
      if (node->js.ps.righttree->chgParam == NULL)
      {
        ExecReScan(node->js.ps.righttree);
      }
    }
  }

                                      
  node->hj_CurHashValue = 0;
  node->hj_CurBucketNo = 0;
  node->hj_CurSkewBucketNo = INVALID_SKEW_BUCKET_NO;
  node->hj_CurTuple = NULL;

  node->hj_MatchedOuter = false;
  node->hj_FirstOuterTupleSlot = NULL;

     
                                                                        
                         
     
  if (node->js.ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->js.ps.lefttree);
  }
}

void
ExecShutdownHashJoin(HashJoinState *node)
{
  if (node->hj_HashTable)
  {
       
                                                                         
                                                                        
                             
       
    ExecHashTableDetachBatch(node->hj_HashTable);
    ExecHashTableDetach(node->hj_HashTable);
  }
}

static void
ExecParallelHashJoinPartitionOuter(HashJoinState *hjstate)
{
  PlanState *outerState = outerPlanState(hjstate);
  ExprContext *econtext = hjstate->js.ps.ps_ExprContext;
  HashJoinTable hashtable = hjstate->hj_HashTable;
  TupleTableSlot *slot;
  uint32 hashvalue;
  int i;

  Assert(hjstate->hj_FirstOuterTupleSlot == NULL);

                                                                     
  for (;;)
  {
    slot = ExecProcNode(outerState);
    if (TupIsNull(slot))
    {
      break;
    }
    econtext->ecxt_outertuple = slot;
    if (ExecHashGetHashValue(hashtable, econtext, hjstate->hj_OuterHashKeys, true,                  
            HJ_FILL_OUTER(hjstate), &hashvalue))
    {
      int batchno;
      int bucketno;
      bool shouldFree;
      MinimalTuple mintup = ExecFetchSlotMinimalTuple(slot, &shouldFree);

      ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno, &batchno);
      sts_puttuple(hashtable->batches[batchno].outer_tuples, &hashvalue, mintup);

      if (shouldFree)
      {
        heap_free_minimal_tuple(mintup);
      }
    }
    CHECK_FOR_INTERRUPTS();
  }

                                                                   
  for (i = 0; i < hashtable->nbatch; ++i)
  {
    sts_end_write(hashtable->batches[i].outer_tuples);
  }
}

void
ExecHashJoinEstimate(HashJoinState *state, ParallelContext *pcxt)
{
  shm_toc_estimate_chunk(&pcxt->estimator, sizeof(ParallelHashJoinState));
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

void
ExecHashJoinInitializeDSM(HashJoinState *state, ParallelContext *pcxt)
{
  int plan_node_id = state->js.ps.plan->plan_node_id;
  HashState *hashNode;
  ParallelHashJoinState *pstate;

     
                                                                      
                                                                             
     
  if (pcxt->seg == NULL)
  {
    return;
  }

  ExecSetExecProcNode(&state->js.ps, ExecParallelHashJoin);

     
                                                                     
                                                      
     
  pstate = shm_toc_allocate(pcxt->toc, sizeof(ParallelHashJoinState));
  shm_toc_insert(pcxt->toc, plan_node_id, pstate);

     
                                                                  
                                                                          
                        
     
  pstate->nbatch = 0;
  pstate->space_allowed = 0;
  pstate->batches = InvalidDsaPointer;
  pstate->old_batches = InvalidDsaPointer;
  pstate->nbuckets = 0;
  pstate->growth = PHJ_GROWTH_OK;
  pstate->chunk_work_queue = InvalidDsaPointer;
  pg_atomic_init_u32(&pstate->distributor, 0);
  pstate->nparticipants = pcxt->nworkers + 1;
  pstate->total_tuples = 0;
  LWLockInitialize(&pstate->lock, LWTRANCHE_PARALLEL_HASH_JOIN);
  BarrierInit(&pstate->build_barrier, 0);
  BarrierInit(&pstate->grow_batches_barrier, 0);
  BarrierInit(&pstate->grow_buckets_barrier, 0);

                                                              
  SharedFileSetInit(&pstate->fileset, pcxt->seg);

                                                     
  hashNode = (HashState *)innerPlanState(state);
  hashNode->parallel_state = pstate;
}

                                                                    
                                
   
                                                      
                                                                    
   
void
ExecHashJoinReInitializeDSM(HashJoinState *state, ParallelContext *cxt)
{
  int plan_node_id = state->js.ps.plan->plan_node_id;
  ParallelHashJoinState *pstate = shm_toc_lookup(cxt->toc, plan_node_id, false);

     
                                                                         
                                                                  
                                                                          
                                                                        
                                                                          
                                                              
                                                                      
                                                                          
                
     

                                                    
  if (state->hj_HashTable != NULL)
  {
    ExecHashTableDetachBatch(state->hj_HashTable);
    ExecHashTableDetach(state->hj_HashTable);
  }

                                     
  SharedFileSetDeleteAll(&pstate->fileset);

                                                                            
  BarrierInit(&pstate->build_barrier, 0);
}

void
ExecHashJoinInitializeWorker(HashJoinState *state, ParallelWorkerContext *pwcxt)
{
  HashState *hashNode;
  int plan_node_id = state->js.ps.plan->plan_node_id;
  ParallelHashJoinState *pstate = shm_toc_lookup(pwcxt->toc, plan_node_id, false);

                                                       
  SharedFileSetAttach(&pstate->fileset, pwcxt->seg);

                                                    
  hashNode = (HashState *)innerPlanState(state);
  hashNode->parallel_state = pstate;

  ExecSetExecProcNode(&state->js.ps, ExecParallelHashJoin);
}
