                                                                            
   
              
                                             
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                              
   
                                                                            
   
   
                      
                                                                     
                                                
                                              
   

#include "postgres.h"

#include <math.h>
#include <limits.h>

#include "access/htup_details.h"
#include "access/parallel.h"
#include "catalog/pg_statistic.h"
#include "commands/tablespace.h"
#include "executor/execdebug.h"
#include "executor/hashjoin.h"
#include "executor/nodeHash.h"
#include "executor/nodeHashjoin.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "port/atomics.h"
#include "port/pg_bitutils.h"
#include "utils/dynahash.h"
#include "utils/memutils.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static void
ExecHashIncreaseNumBatches(HashJoinTable hashtable);
static void
ExecHashIncreaseNumBuckets(HashJoinTable hashtable);
static void
ExecParallelHashIncreaseNumBatches(HashJoinTable hashtable);
static void
ExecParallelHashIncreaseNumBuckets(HashJoinTable hashtable);
static void
ExecHashBuildSkewHash(HashJoinTable hashtable, Hash *node, int mcvsToUse);
static void
ExecHashSkewTableInsert(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue, int bucketNumber);
static void
ExecHashRemoveNextSkewBucket(HashJoinTable hashtable);

static void *
dense_alloc(HashJoinTable hashtable, Size size);
static HashJoinTuple
ExecParallelHashTupleAlloc(HashJoinTable hashtable, size_t size, dsa_pointer *shared);
static void
MultiExecPrivateHash(HashState *node);
static void
MultiExecParallelHash(HashState *node);
static inline HashJoinTuple
ExecParallelHashFirstTuple(HashJoinTable table, int bucketno);
static inline HashJoinTuple
ExecParallelHashNextTuple(HashJoinTable table, HashJoinTuple tuple);
static inline void
ExecParallelHashPushTuple(dsa_pointer_atomic *head, HashJoinTuple tuple, dsa_pointer tuple_shared);
static void
ExecParallelHashJoinSetUpBatches(HashJoinTable hashtable, int nbatch);
static void
ExecParallelHashEnsureBatchAccessors(HashJoinTable hashtable);
static void
ExecParallelHashRepartitionFirst(HashJoinTable hashtable);
static void
ExecParallelHashRepartitionRest(HashJoinTable hashtable);
static HashMemoryChunk
ExecParallelHashPopChunkQueue(HashJoinTable table, dsa_pointer *shared);
static bool
ExecParallelHashTuplePrealloc(HashJoinTable hashtable, int batchno, size_t size);
static void
ExecParallelHashMergeCounters(HashJoinTable hashtable);
static void
ExecParallelHashCloseBatchAccessors(HashJoinTable hashtable);

                                                                    
             
   
                                  
                                                                    
   
static TupleTableSlot *
ExecHash(PlanState *pstate)
{
  elog(ERROR, "Hash node does not support ExecProcNode call convention");
  return NULL;
}

                                                                    
                  
   
                                                              
                                
                                                                    
   
Node *
MultiExecHash(HashState *node)
{
                                                    
  if (node->ps.instrument)
  {
    InstrStartNode(node->ps.instrument);
  }

  if (node->parallel_state != NULL)
  {
    MultiExecParallelHash(node);
  }
  else
  {
    MultiExecPrivateHash(node);
  }

                                                    
  if (node->ps.instrument)
  {
    InstrStopNode(node->ps.instrument, node->hashtable->partialTuples);
  }

     
                                                                            
                                                                         
                                                                             
                                                                         
                                               
     
  return NULL;
}

                                                                    
                         
   
                                                           
                                               
                                                                    
   
static void
MultiExecPrivateHash(HashState *node)
{
  PlanState *outerNode;
  List *hashkeys;
  HashJoinTable hashtable;
  TupleTableSlot *slot;
  ExprContext *econtext;
  uint32 hashvalue;

     
                              
     
  outerNode = outerPlanState(node);
  hashtable = node->hashtable;

     
                            
     
  hashkeys = node->hashkeys;
  econtext = node->ps.ps_ExprContext;

     
                                                                          
                                 
     
  for (;;)
  {
    slot = ExecProcNode(outerNode);
    if (TupIsNull(slot))
    {
      break;
    }
                                           
    econtext->ecxt_outertuple = slot;
    if (ExecHashGetHashValue(hashtable, econtext, hashkeys, false, hashtable->keepNulls, &hashvalue))
    {
      int bucketNumber;

      bucketNumber = ExecHashGetSkewBucket(hashtable, hashvalue);
      if (bucketNumber != INVALID_SKEW_BUCKET_NO)
      {
                                                               
        ExecHashSkewTableInsert(hashtable, slot, hashvalue, bucketNumber);
        hashtable->skewTuples += 1;
      }
      else
      {
                                                                  
        ExecHashTableInsert(hashtable, slot, hashvalue);
      }
      hashtable->totalTuples += 1;
    }
  }

                                                                  
  if (hashtable->nbuckets != hashtable->nbuckets_optimal)
  {
    ExecHashIncreaseNumBuckets(hashtable);
  }

                                                                          
  hashtable->spaceUsed += hashtable->nbuckets * sizeof(HashJoinTuple);
  if (hashtable->spaceUsed > hashtable->spacePeak)
  {
    hashtable->spacePeak = hashtable->spaceUsed;
  }

  hashtable->partialTuples = hashtable->totalTuples;
}

                                                                    
                          
   
                                                             
                                                            
                                    
                                                                    
   
static void
MultiExecParallelHash(HashState *node)
{
  ParallelHashJoinState *pstate;
  PlanState *outerNode;
  List *hashkeys;
  HashJoinTable hashtable;
  TupleTableSlot *slot;
  ExprContext *econtext;
  uint32 hashvalue;
  Barrier *build_barrier;
  int i;

     
                              
     
  outerNode = outerPlanState(node);
  hashtable = node->hashtable;

     
                            
     
  hashkeys = node->hashkeys;
  econtext = node->ps.ps_ExprContext;

     
                                                                            
                                                          
                                                                         
                                                                          
                                                                           
                                                        
     
  pstate = hashtable->parallel_state;
  build_barrier = &pstate->build_barrier;
  Assert(BarrierPhase(build_barrier) >= PHJ_BUILD_ALLOCATING);
  switch (BarrierPhase(build_barrier))
  {
  case PHJ_BUILD_ALLOCATING:

       
                                                         
                                                                     
                                                                
       
    BarrierArriveAndWait(build_barrier, WAIT_EVENT_HASH_BUILD_ALLOCATING);
                       

  case PHJ_BUILD_HASHING_INNER:

       
                                                                   
                                                                   
                                                                     
                                                                   
                                                                       
                                                                   
              
       
    if (PHJ_GROW_BATCHES_PHASE(BarrierAttach(&pstate->grow_batches_barrier)) != PHJ_GROW_BATCHES_ELECTING)
    {
      ExecParallelHashIncreaseNumBatches(hashtable);
    }
    if (PHJ_GROW_BUCKETS_PHASE(BarrierAttach(&pstate->grow_buckets_barrier)) != PHJ_GROW_BUCKETS_ELECTING)
    {
      ExecParallelHashIncreaseNumBuckets(hashtable);
    }
    ExecParallelHashEnsureBatchAccessors(hashtable);
    ExecParallelHashTableSetCurrentBatch(hashtable, 0);
    for (;;)
    {
      slot = ExecProcNode(outerNode);
      if (TupIsNull(slot))
      {
        break;
      }
      econtext->ecxt_outertuple = slot;
      if (ExecHashGetHashValue(hashtable, econtext, hashkeys, false, hashtable->keepNulls, &hashvalue))
      {
        ExecParallelHashTableInsert(hashtable, slot, hashvalue);
      }
      hashtable->partialTuples++;
    }

       
                                                                 
                                                
       
    for (i = 0; i < hashtable->nbatch; ++i)
    {
      sts_end_write(hashtable->batches[i].inner_tuples);
    }

       
                                                                      
                                                
       
    ExecParallelHashMergeCounters(hashtable);

    BarrierDetach(&pstate->grow_buckets_barrier);
    BarrierDetach(&pstate->grow_batches_barrier);

       
                                                                   
                 
       
    if (BarrierArriveAndWait(build_barrier, WAIT_EVENT_HASH_BUILD_HASHING_INNER))
    {
         
                                                                   
                                                                     
                                                                     
                                                                  
                
         
      pstate->growth = PHJ_GROWTH_DISABLED;
    }
  }

     
                                                                            
                                                                
     
  hashtable->curbatch = -1;
  hashtable->nbuckets = pstate->nbuckets;
  hashtable->log2_nbuckets = my_log2(hashtable->nbuckets);
  hashtable->totalTuples = pstate->total_tuples;
  ExecParallelHashEnsureBatchAccessors(hashtable);

     
                                                                            
                                                                           
                     
     
  Assert(BarrierPhase(build_barrier) == PHJ_BUILD_HASHING_OUTER || BarrierPhase(build_barrier) == PHJ_BUILD_DONE);
}

                                                                    
                 
   
                               
                                                                    
   
HashState *
ExecInitHash(Hash *node, EState *estate, int eflags)
{
  HashState *hashstate;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

     
                            
     
  hashstate = makeNode(HashState);
  hashstate->ps.plan = (Plan *)node;
  hashstate->ps.state = estate;
  hashstate->ps.ExecProcNode = ExecHash;
  hashstate->hashtable = NULL;
  hashstate->hashkeys = NIL;                                     

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &hashstate->ps);

     
                            
     
  outerPlanState(hashstate) = ExecInitNode(outerPlan(node), estate, eflags);

     
                                                                      
                                               
     
  ExecInitResultTupleSlotTL(&hashstate->ps, &TTSOpsMinimalTuple);
  hashstate->ps.ps_ProjInfo = NULL;

     
                                  
     
  Assert(node->plan.qual == NIL);
  hashstate->hashkeys = ExecInitExprList(node->hashkeys, (PlanState *)hashstate);

  return hashstate;
}

                                                                   
                
   
                                   
                                                                    
   
void
ExecEndHash(HashState *node)
{
  PlanState *outerPlan;

     
                      
     
  ExecFreeExprContext(&node->ps);

     
                           
     
  outerPlan = outerPlanState(node);
  ExecEndNode(outerPlan);
}

                                                                    
                        
   
                                                           
                                                                    
   
HashJoinTable
ExecHashTableCreate(HashState *state, List *hashOperators, List *hashCollations, bool keepNulls)
{
  Hash *node;
  HashJoinTable hashtable;
  Plan *outerNode;
  size_t space_allowed;
  int nbuckets;
  int nbatch;
  double rows;
  int num_skew_mcvs;
  int log2_nbuckets;
  int nkeys;
  int i;
  ListCell *ho;
  ListCell *hc;
  MemoryContext oldcxt;

     
                                                                           
                                                                            
                                                     
     
  node = (Hash *)state->ps.plan;
  outerNode = outerPlan(node);

     
                                                                         
                                                                            
                                                                 
     
  rows = node->plan.parallel_aware ? node->rows_total : outerNode->plan_rows;

  ExecChooseHashTableSize(rows, outerNode->plan_width, OidIsValid(node->skewTable), state->parallel_state != NULL, state->parallel_state != NULL ? state->parallel_state->nparticipants - 1 : 0, &space_allowed, &nbuckets, &nbatch, &num_skew_mcvs);

                                     
  log2_nbuckets = my_log2(nbuckets);
  Assert(nbuckets == (1 << log2_nbuckets));

     
                                              
     
                                                                      
                                                                          
                                     
     
  hashtable = (HashJoinTable)palloc(sizeof(HashJoinTableData));
  hashtable->nbuckets = nbuckets;
  hashtable->nbuckets_original = nbuckets;
  hashtable->nbuckets_optimal = nbuckets;
  hashtable->log2_nbuckets = log2_nbuckets;
  hashtable->log2_nbuckets_optimal = log2_nbuckets;
  hashtable->buckets.unshared = NULL;
  hashtable->keepNulls = keepNulls;
  hashtable->skewEnabled = false;
  hashtable->skewBucket = NULL;
  hashtable->skewBucketLen = 0;
  hashtable->nSkewBuckets = 0;
  hashtable->skewBucketNums = NULL;
  hashtable->nbatch = nbatch;
  hashtable->curbatch = 0;
  hashtable->nbatch_original = nbatch;
  hashtable->nbatch_outstart = nbatch;
  hashtable->growEnabled = true;
  hashtable->totalTuples = 0;
  hashtable->partialTuples = 0;
  hashtable->skewTuples = 0;
  hashtable->innerBatchFile = NULL;
  hashtable->outerBatchFile = NULL;
  hashtable->spaceUsed = 0;
  hashtable->spacePeak = 0;
  hashtable->spaceAllowed = space_allowed;
  hashtable->spaceUsedSkew = 0;
  hashtable->spaceAllowedSkew = hashtable->spaceAllowed * SKEW_WORK_MEM_PERCENT / 100;
  hashtable->chunks = NULL;
  hashtable->current_chunk = NULL;
  hashtable->parallel_state = state->parallel_state;
  hashtable->area = state->ps.state->es_query_dsa;
  hashtable->batches = NULL;

#ifdef HJDEBUG
  printf("Hashjoin %p: initial nbatch = %d, nbuckets = %d\n", hashtable, nbatch, nbuckets);
#endif

     
                                                                             
                                                 
     
  hashtable->hashCxt = AllocSetContextCreate(CurrentMemoryContext, "HashTableContext", ALLOCSET_DEFAULT_SIZES);

  hashtable->batchCxt = AllocSetContextCreate(hashtable->hashCxt, "HashBatchContext", ALLOCSET_DEFAULT_SIZES);

                                                                 

  oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

     
                                                                          
                                                     
     
  nkeys = list_length(hashOperators);
  hashtable->outer_hashfunctions = (FmgrInfo *)palloc(nkeys * sizeof(FmgrInfo));
  hashtable->inner_hashfunctions = (FmgrInfo *)palloc(nkeys * sizeof(FmgrInfo));
  hashtable->hashStrict = (bool *)palloc(nkeys * sizeof(bool));
  hashtable->collations = (Oid *)palloc(nkeys * sizeof(Oid));
  i = 0;
  forboth(ho, hashOperators, hc, hashCollations)
  {
    Oid hashop = lfirst_oid(ho);
    Oid left_hashfn;
    Oid right_hashfn;

    if (!get_op_hash_functions(hashop, &left_hashfn, &right_hashfn))
    {
      elog(ERROR, "could not find hash function for hash operator %u", hashop);
    }
    fmgr_info(left_hashfn, &hashtable->outer_hashfunctions[i]);
    fmgr_info(right_hashfn, &hashtable->inner_hashfunctions[i]);
    hashtable->hashStrict[i] = op_strict(hashop);
    hashtable->collations[i] = lfirst_oid(hc);
    i++;
  }

  if (nbatch > 1 && hashtable->parallel_state == NULL)
  {
       
                                                                          
                                                                         
       
    hashtable->innerBatchFile = (BufFile **)palloc0(nbatch * sizeof(BufFile *));
    hashtable->outerBatchFile = (BufFile **)palloc0(nbatch * sizeof(BufFile *));
                                                      
                                                                         
    PrepareTempTablespaces();
  }

  MemoryContextSwitchTo(oldcxt);

  if (hashtable->parallel_state)
  {
    ParallelHashJoinState *pstate = hashtable->parallel_state;
    Barrier *build_barrier;

       
                                                                           
                                                                 
                                                                           
                                                                         
                                                                    
                                                                  
       
    build_barrier = &pstate->build_barrier;
    BarrierAttach(build_barrier);

       
                                                                        
                                                                          
                                                                      
                                                                        
                                                            
       
    if (BarrierPhase(build_barrier) == PHJ_BUILD_ELECTING && BarrierArriveAndWait(build_barrier, WAIT_EVENT_HASH_BUILD_ELECTING))
    {
      pstate->nbatch = nbatch;
      pstate->space_allowed = space_allowed;
      pstate->growth = PHJ_GROWTH_OK;

                                                             
      ExecParallelHashJoinSetUpBatches(hashtable, nbatch);

         
                                                                  
                                 
         
      pstate->nbuckets = nbuckets;
      ExecParallelHashTableAlloc(hashtable, 0);
    }

       
                                                          
                                                                      
                                                                     
                                           
       
  }
  else
  {
       
                                                                          
                                                              
       
    MemoryContextSwitchTo(hashtable->batchCxt);

    hashtable->buckets.unshared = (HashJoinTuple *)palloc0(nbuckets * sizeof(HashJoinTuple));

       
                                                                        
                                                                       
            
       
    if (nbatch > 1)
    {
      ExecHashBuildSkewHash(hashtable, node, num_skew_mcvs);
    }

    MemoryContextSwitchTo(oldcxt);
  }

  return hashtable;
}

   
                                                                          
                                                                 
   
                                                                 
   

                                               
#define NTUP_PER_BUCKET 1

void
ExecChooseHashTableSize(double ntuples, int tupwidth, bool useskew, bool try_combined_work_mem, int parallel_workers, size_t *space_allowed, int *numbuckets, int *numbatches, int *num_skew_mcvs)
{
  int tupsize;
  double inner_rel_bytes;
  long bucket_bytes;
  long hash_table_bytes;
  long skew_table_bytes;
  long max_pointers;
  long mppow2;
  int nbatch = 1;
  int nbuckets;
  double dbuckets;

                                                  
  if (ntuples <= 0.0)
  {
    ntuples = 1000.0;
  }

     
                                                                            
                                                                             
                                         
     
  tupsize = HJTUPLE_OVERHEAD + MAXALIGN(SizeofMinimalTupleHeader) + MAXALIGN(tupwidth);
  inner_rel_bytes = ntuples * tupsize;

     
                                                            
     
  hash_table_bytes = work_mem * 1024L;

     
                                                                        
                                                                             
                                                          
     
  if (try_combined_work_mem)
  {
    hash_table_bytes += hash_table_bytes * parallel_workers;
  }

  *space_allowed = hash_table_bytes;

     
                                                                           
                                                                          
                                                    
     
                                                                          
                                                                             
                                                        
     
                                                                          
                                                                          
                                                                           
                 
     
  if (useskew)
  {
    skew_table_bytes = hash_table_bytes * SKEW_WORK_MEM_PERCENT / 100;

                 
                   
                              
                                                 
                                        
                                         
                 
       
    *num_skew_mcvs = skew_table_bytes / (tupsize + (8 * sizeof(HashSkewBucket *)) + sizeof(int) + SKEW_BUCKET_OVERHEAD);
    if (*num_skew_mcvs > 0)
    {
      hash_table_bytes -= skew_table_bytes;
    }
  }
  else
  {
    *num_skew_mcvs = 0;
  }

     
                                                                            
                                                                            
                                                                         
                   
     
                                                                    
                                     
     
  max_pointers = *space_allowed / sizeof(HashJoinTuple);
  max_pointers = Min(max_pointers, MaxAllocSize / sizeof(HashJoinTuple));
                                                                     
  mppow2 = 1L << my_log2(max_pointers);
  if (max_pointers != mppow2)
  {
    max_pointers = mppow2 / 2;
  }

                                                                    
                                                                        
  max_pointers = Min(max_pointers, INT_MAX / 2);

  dbuckets = ceil(ntuples / NTUP_PER_BUCKET);
  dbuckets = Min(dbuckets, max_pointers);
  nbuckets = (int)dbuckets;
                                                      
  nbuckets = Max(nbuckets, 1024);
                                            
  nbuckets = 1 << my_log2(nbuckets);

     
                                                                             
                                                                 
     
  bucket_bytes = sizeof(HashJoinTuple) * nbuckets;
  if (inner_rel_bytes + bucket_bytes > hash_table_bytes)
  {
                                     
    long lbuckets;
    double dbatch;
    int minbatch;
    long bucket_size;

       
                                                                         
                                                                    
       
    if (try_combined_work_mem)
    {
      ExecChooseHashTableSize(ntuples, tupwidth, useskew, false, parallel_workers, space_allowed, numbuckets, numbatches, num_skew_mcvs);
      return;
    }

       
                                                                          
                                                                      
                                                                     
                                                                   
       
    bucket_size = (tupsize * NTUP_PER_BUCKET + sizeof(HashJoinTuple));
    lbuckets = 1L << my_log2(hash_table_bytes / bucket_size);
    lbuckets = Min(lbuckets, max_pointers);
    nbuckets = (int)lbuckets;
    nbuckets = 1 << my_log2(nbuckets);
    bucket_bytes = nbuckets * sizeof(HashJoinTuple);

       
                                                                     
                                                                          
                                                            
                                                                         
                                                                        
                          
       
    Assert(bucket_bytes <= hash_table_bytes / 2);

                                               
    dbatch = ceil(inner_rel_bytes / (hash_table_bytes - bucket_bytes));
    dbatch = Min(dbatch, max_pointers);
    minbatch = (int)dbatch;
    nbatch = 2;
    while (nbatch < minbatch)
    {
      nbatch <<= 1;
    }
  }

  Assert(nbuckets > 0);
  Assert(nbatch > 0);

  *numbuckets = nbuckets;
  *numbatches = nbatch;
}

                                                                    
                         
   
                         
                                                                    
   
void
ExecHashTableDestroy(HashJoinTable hashtable)
{
  int i;

     
                                                                         
                                                                       
                                                                    
     
  if (hashtable->innerBatchFile != NULL)
  {
    for (i = 1; i < hashtable->nbatch; i++)
    {
      if (hashtable->innerBatchFile[i])
      {
        BufFileClose(hashtable->innerBatchFile[i]);
      }
      if (hashtable->outerBatchFile[i])
      {
        BufFileClose(hashtable->outerBatchFile[i]);
      }
    }
  }

                                                                         
  MemoryContextDelete(hashtable->hashCxt);

                                  
  pfree(hashtable);
}

   
                              
                                                               
                               
   
static void
ExecHashIncreaseNumBatches(HashJoinTable hashtable)
{
  int oldnbatch = hashtable->nbatch;
  int curbatch = hashtable->curbatch;
  int nbatch;
  MemoryContext oldcxt;
  long ninmemory;
  long nfreed;
  HashMemoryChunk oldchunks;

                                                      
  if (!hashtable->growEnabled)
  {
    return;
  }

                                      
  if (oldnbatch > Min(INT_MAX / 2, MaxAllocSize / (sizeof(void *) * 2)))
  {
    return;
  }

  nbatch = oldnbatch * 2;
  Assert(nbatch > 1);

#ifdef HJDEBUG
  printf("Hashjoin %p: increasing nbatch to %d because space = %zu\n", hashtable, nbatch, hashtable->spaceUsed);
#endif

  oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

  if (hashtable->innerBatchFile == NULL)
  {
                                      
    hashtable->innerBatchFile = (BufFile **)palloc0(nbatch * sizeof(BufFile *));
    hashtable->outerBatchFile = (BufFile **)palloc0(nbatch * sizeof(BufFile *));
                                                     
    PrepareTempTablespaces();
  }
  else
  {
                                                   
    hashtable->innerBatchFile = (BufFile **)repalloc(hashtable->innerBatchFile, nbatch * sizeof(BufFile *));
    hashtable->outerBatchFile = (BufFile **)repalloc(hashtable->outerBatchFile, nbatch * sizeof(BufFile *));
    MemSet(hashtable->innerBatchFile + oldnbatch, 0, (nbatch - oldnbatch) * sizeof(BufFile *));
    MemSet(hashtable->outerBatchFile + oldnbatch, 0, (nbatch - oldnbatch) * sizeof(BufFile *));
  }

  MemoryContextSwitchTo(oldcxt);

  hashtable->nbatch = nbatch;

     
                                                                            
                                     
     
  ninmemory = nfreed = 0;

                                                                          
  if (hashtable->nbuckets_optimal != hashtable->nbuckets)
  {
                                                 
    Assert(hashtable->nbuckets_optimal > hashtable->nbuckets);

    hashtable->nbuckets = hashtable->nbuckets_optimal;
    hashtable->log2_nbuckets = hashtable->log2_nbuckets_optimal;

    hashtable->buckets.unshared = repalloc(hashtable->buckets.unshared, sizeof(HashJoinTuple) * hashtable->nbuckets);
  }

     
                                                                        
                                                                             
                                                                   
     
  memset(hashtable->buckets.unshared, 0, sizeof(HashJoinTuple) * hashtable->nbuckets);
  oldchunks = hashtable->chunks;
  hashtable->chunks = NULL;

                                                                           
  while (oldchunks != NULL)
  {
    HashMemoryChunk nextchunk = oldchunks->next.unshared;

                                                            
    size_t idx = 0;

                                                                    
    while (idx < oldchunks->used)
    {
      HashJoinTuple hashTuple = (HashJoinTuple)(HASH_CHUNK_DATA(oldchunks) + idx);
      MinimalTuple tuple = HJTUPLE_MINTUPLE(hashTuple);
      int hashTupleSize = (HJTUPLE_OVERHEAD + tuple->t_len);
      int bucketno;
      int batchno;

      ninmemory++;
      ExecHashGetBucketAndBatch(hashtable, hashTuple->hashvalue, &bucketno, &batchno);

      if (batchno == curbatch)
      {
                                                               
        HashJoinTuple copyTuple;

        copyTuple = (HashJoinTuple)dense_alloc(hashtable, hashTupleSize);
        memcpy(copyTuple, hashTuple, hashTupleSize);

                                                       
        copyTuple->next.unshared = hashtable->buckets.unshared[bucketno];
        hashtable->buckets.unshared[bucketno] = copyTuple;
      }
      else
      {
                         
        Assert(batchno > curbatch);
        ExecHashJoinSaveTuple(HJTUPLE_MINTUPLE(hashTuple), hashTuple->hashvalue, &hashtable->innerBatchFile[batchno]);

        hashtable->spaceUsed -= hashTupleSize;
        nfreed++;
      }

                                    
      idx += MAXALIGN(hashTupleSize);

                                             
      CHECK_FOR_INTERRUPTS();
    }

                                                                          
    pfree(oldchunks);
    oldchunks = nextchunk;
  }

#ifdef HJDEBUG
  printf("Hashjoin %p: freed %ld of %ld tuples, space now %zu\n", hashtable, nfreed, ninmemory, hashtable->spaceUsed);
#endif

     
                                                                             
                                                                       
                                                                     
                                                                             
                                                                           
                     
     
  if (nfreed == 0 || nfreed == ninmemory)
  {
    hashtable->growEnabled = false;
#ifdef HJDEBUG
    printf("Hashjoin %p: disabling further increase of nbatch\n", hashtable);
#endif
  }
}

   
                                      
                                                                     
                                                                      
   
static void
ExecParallelHashIncreaseNumBatches(HashJoinTable hashtable)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  int i;

  Assert(BarrierPhase(&pstate->build_barrier) == PHJ_BUILD_HASHING_INNER);

     
                                                                            
                                                                            
                         
     
  switch (PHJ_GROW_BATCHES_PHASE(BarrierPhase(&pstate->grow_batches_barrier)))
  {
  case PHJ_GROW_BATCHES_ELECTING:

       
                                                                       
                                                                      
                                                                       
               
       
    if (BarrierArriveAndWait(&pstate->grow_batches_barrier, WAIT_EVENT_HASH_GROW_BATCHES_ELECTING))
    {
      dsa_pointer_atomic *buckets;
      ParallelHashJoinBatch *old_batch0;
      int new_nbatch;
      int i;

                                              
      old_batch0 = hashtable->batches[0].shared;
      pstate->old_batches = pstate->batches;
      pstate->old_nbatch = hashtable->nbatch;
      pstate->batches = InvalidDsaPointer;

                                              
      ExecParallelHashCloseBatchAccessors(hashtable);

                                               
      if (hashtable->nbatch == 1)
      {
           
                                                                   
                                                                  
                                    
           
        pstate->space_allowed = work_mem * 1024L;

           
                                                            
                                                                
                                                               
                                                              
                                         
           
        new_nbatch = 1 << my_log2(pstate->nparticipants * 2);
      }
      else
      {
           
                                                                   
                       
           
        new_nbatch = hashtable->nbatch * 2;
      }

                                                      
      Assert(hashtable->nbatch == pstate->nbatch);
      ExecParallelHashJoinSetUpBatches(hashtable, new_nbatch);
      Assert(hashtable->nbatch == pstate->nbatch);

                                                      
      if (pstate->old_nbatch == 1)
      {
        double dtuples;
        double dbuckets;
        int new_nbuckets;

           
                                                                   
                                                                
                                                                  
                                                                   
                                                              
                                                                   
                                                              
                  
           
        dtuples = (old_batch0->ntuples * 2.0) / new_nbatch;
        dbuckets = ceil(dtuples / NTUP_PER_BUCKET);
        dbuckets = Min(dbuckets, MaxAllocSize / sizeof(dsa_pointer_atomic));
        new_nbuckets = (int)dbuckets;
        new_nbuckets = Max(new_nbuckets, 1024);
        new_nbuckets = 1 << my_log2(new_nbuckets);
        dsa_free(hashtable->area, old_batch0->buckets);
        hashtable->batches[0].shared->buckets = dsa_allocate(hashtable->area, sizeof(dsa_pointer_atomic) * new_nbuckets);
        buckets = (dsa_pointer_atomic *)dsa_get_address(hashtable->area, hashtable->batches[0].shared->buckets);
        for (i = 0; i < new_nbuckets; ++i)
        {
          dsa_pointer_atomic_init(&buckets[i], InvalidDsaPointer);
        }
        pstate->nbuckets = new_nbuckets;
      }
      else
      {
                                                
        hashtable->batches[0].shared->buckets = old_batch0->buckets;
        buckets = (dsa_pointer_atomic *)dsa_get_address(hashtable->area, old_batch0->buckets);
        for (i = 0; i < hashtable->nbuckets; ++i)
        {
          dsa_pointer_atomic_write(&buckets[i], InvalidDsaPointer);
        }
      }

                                                                      
      pstate->chunk_work_queue = old_batch0->chunks;

                                                                   
      pstate->growth = PHJ_GROWTH_DISABLED;
    }
    else
    {
                                                                   
      ExecParallelHashCloseBatchAccessors(hashtable);
    }
                       

  case PHJ_GROW_BATCHES_ALLOCATING:
                                            
    BarrierArriveAndWait(&pstate->grow_batches_barrier, WAIT_EVENT_HASH_GROW_BATCHES_ALLOCATING);
                       

  case PHJ_GROW_BATCHES_REPARTITIONING:
                                                                    
    ExecParallelHashEnsureBatchAccessors(hashtable);
    ExecParallelHashTableSetCurrentBatch(hashtable, 0);
                                         
    ExecParallelHashRepartitionFirst(hashtable);
    ExecParallelHashRepartitionRest(hashtable);
    ExecParallelHashMergeCounters(hashtable);
                                            
    BarrierArriveAndWait(&pstate->grow_batches_barrier, WAIT_EVENT_HASH_GROW_BATCHES_REPARTITIONING);
                       

  case PHJ_GROW_BATCHES_DECIDING:

       
                                                                    
                                                                    
                    
       
    if (BarrierArriveAndWait(&pstate->grow_batches_barrier, WAIT_EVENT_HASH_GROW_BATCHES_DECIDING))
    {
      bool space_exhausted = false;
      bool extreme_skew_detected = false;

                                                                      
      ExecParallelHashEnsureBatchAccessors(hashtable);
      ExecParallelHashTableSetCurrentBatch(hashtable, 0);

                                                               
      for (i = 0; i < hashtable->nbatch; ++i)
      {
        ParallelHashJoinBatch *batch = hashtable->batches[i].shared;

        if (batch->space_exhausted || batch->estimated_size > pstate->space_allowed)
        {
          int parent;

          space_exhausted = true;

             
                                                               
                                                             
                                                                 
                                         
             
          parent = i % pstate->old_nbatch;
          if (batch->ntuples == hashtable->batches[parent].shared->old_ntuples)
          {
            extreme_skew_detected = true;
          }
        }
      }

                                                                    
      if (extreme_skew_detected || hashtable->nbatch >= INT_MAX / 2)
      {
        pstate->growth = PHJ_GROWTH_DISABLED;
      }
      else if (space_exhausted)
      {
        pstate->growth = PHJ_GROWTH_NEED_MORE_BATCHES;
      }
      else
      {
        pstate->growth = PHJ_GROWTH_OK;
      }

                                                  
      dsa_free(hashtable->area, pstate->old_batches);
      pstate->old_batches = InvalidDsaPointer;
    }
                       

  case PHJ_GROW_BATCHES_FINISHING:
                                         
    BarrierArriveAndWait(&pstate->grow_batches_barrier, WAIT_EVENT_HASH_GROW_BATCHES_FINISHING);
  }
}

   
                                                                         
                                                                               
                                                        
   
static void
ExecParallelHashRepartitionFirst(HashJoinTable hashtable)
{
  dsa_pointer chunk_shared;
  HashMemoryChunk chunk;

  Assert(hashtable->nbatch == hashtable->parallel_state->nbatch);

  while ((chunk = ExecParallelHashPopChunkQueue(hashtable, &chunk_shared)))
  {
    size_t idx = 0;

                                               
    while (idx < chunk->used)
    {
      HashJoinTuple hashTuple = (HashJoinTuple)(HASH_CHUNK_DATA(chunk) + idx);
      MinimalTuple tuple = HJTUPLE_MINTUPLE(hashTuple);
      HashJoinTuple copyTuple;
      dsa_pointer shared;
      int bucketno;
      int batchno;

      ExecHashGetBucketAndBatch(hashtable, hashTuple->hashvalue, &bucketno, &batchno);

      Assert(batchno < hashtable->nbatch);
      if (batchno == 0)
      {
                                                                
        copyTuple = ExecParallelHashTupleAlloc(hashtable, HJTUPLE_OVERHEAD + tuple->t_len, &shared);
        copyTuple->hashvalue = hashTuple->hashvalue;
        memcpy(HJTUPLE_MINTUPLE(copyTuple), tuple, tuple->t_len);
        ExecParallelHashPushTuple(&hashtable->buckets.shared[bucketno], copyTuple, shared);
      }
      else
      {
        size_t tuple_size = MAXALIGN(HJTUPLE_OVERHEAD + tuple->t_len);

                                          
        hashtable->batches[batchno].estimated_size += tuple_size;
        sts_puttuple(hashtable->batches[batchno].inner_tuples, &hashTuple->hashvalue, tuple);
      }

                             
      ++hashtable->batches[0].old_ntuples;
      ++hashtable->batches[batchno].ntuples;

      idx += MAXALIGN(HJTUPLE_OVERHEAD + HJTUPLE_MINTUPLE(hashTuple)->t_len);
    }

                          
    dsa_free(hashtable->area, chunk_shared);

    CHECK_FOR_INTERRUPTS();
  }
}

   
                                        
   
static void
ExecParallelHashRepartitionRest(HashJoinTable hashtable)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  int old_nbatch = pstate->old_nbatch;
  SharedTuplestoreAccessor **old_inner_tuples;
  ParallelHashJoinBatch *old_batches;
  int i;

                                                            
  old_batches = (ParallelHashJoinBatch *)dsa_get_address(hashtable->area, pstate->old_batches);
  old_inner_tuples = palloc0(sizeof(SharedTuplestoreAccessor *) * old_nbatch);
  for (i = 1; i < old_nbatch; ++i)
  {
    ParallelHashJoinBatch *shared = NthParallelHashJoinBatch(old_batches, i);

    old_inner_tuples[i] = sts_attach(ParallelHashJoinBatchInner(shared), ParallelWorkerNumber + 1, &pstate->fileset);
  }

                                               
  for (i = 1; i < old_nbatch; ++i)
  {
    MinimalTuple tuple;
    uint32 hashvalue;

                                                          
    sts_begin_parallel_scan(old_inner_tuples[i]);
    while ((tuple = sts_parallel_scan_next(old_inner_tuples[i], &hashvalue)))
    {
      size_t tuple_size = MAXALIGN(HJTUPLE_OVERHEAD + tuple->t_len);
      int bucketno;
      int batchno;

                                                                    
      ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno, &batchno);

      hashtable->batches[batchno].estimated_size += tuple_size;
      ++hashtable->batches[batchno].ntuples;
      ++hashtable->batches[i].old_ntuples;

                                          
      sts_puttuple(hashtable->batches[batchno].inner_tuples, &hashvalue, tuple);

      CHECK_FOR_INTERRUPTS();
    }
    sts_end_parallel_scan(old_inner_tuples[i]);
  }

  pfree(old_inner_tuples);
}

   
                                                                       
   
static void
ExecParallelHashMergeCounters(HashJoinTable hashtable)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  int i;

  LWLockAcquire(&pstate->lock, LW_EXCLUSIVE);
  pstate->total_tuples = 0;
  for (i = 0; i < hashtable->nbatch; ++i)
  {
    ParallelHashJoinBatchAccessor *batch = &hashtable->batches[i];

    batch->shared->size += batch->size;
    batch->shared->estimated_size += batch->estimated_size;
    batch->shared->ntuples += batch->ntuples;
    batch->shared->old_ntuples += batch->old_ntuples;
    batch->size = 0;
    batch->estimated_size = 0;
    batch->ntuples = 0;
    batch->old_ntuples = 0;
    pstate->total_tuples += batch->shared->ntuples;
  }
  LWLockRelease(&pstate->lock);
}

   
                              
                                                               
                                
   
static void
ExecHashIncreaseNumBuckets(HashJoinTable hashtable)
{
  HashMemoryChunk chunk;

                                                                         
  if (hashtable->nbuckets >= hashtable->nbuckets_optimal)
  {
    return;
  }

#ifdef HJDEBUG
  printf("Hashjoin %p: increasing nbuckets %d => %d\n", hashtable, hashtable->nbuckets, hashtable->nbuckets_optimal);
#endif

  hashtable->nbuckets = hashtable->nbuckets_optimal;
  hashtable->log2_nbuckets = hashtable->log2_nbuckets_optimal;

  Assert(hashtable->nbuckets > 1);
  Assert(hashtable->nbuckets <= (INT_MAX / 2));
  Assert(hashtable->nbuckets == (1 << hashtable->log2_nbuckets));

     
                                                                          
                                                                         
                                                                      
             
     
  hashtable->buckets.unshared = (HashJoinTuple *)repalloc(hashtable->buckets.unshared, hashtable->nbuckets * sizeof(HashJoinTuple));

  memset(hashtable->buckets.unshared, 0, hashtable->nbuckets * sizeof(HashJoinTuple));

                                                                       
  for (chunk = hashtable->chunks; chunk != NULL; chunk = chunk->next.unshared)
  {
                                                 
    size_t idx = 0;

    while (idx < chunk->used)
    {
      HashJoinTuple hashTuple = (HashJoinTuple)(HASH_CHUNK_DATA(chunk) + idx);
      int bucketno;
      int batchno;

      ExecHashGetBucketAndBatch(hashtable, hashTuple->hashvalue, &bucketno, &batchno);

                                              
      hashTuple->next.unshared = hashtable->buckets.unshared[bucketno];
      hashtable->buckets.unshared[bucketno] = hashTuple;

                                        
      idx += MAXALIGN(HJTUPLE_OVERHEAD + HJTUPLE_MINTUPLE(hashTuple)->t_len);
    }

                                           
    CHECK_FOR_INTERRUPTS();
  }
}

static void
ExecParallelHashIncreaseNumBuckets(HashJoinTable hashtable)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  int i;
  HashMemoryChunk chunk;
  dsa_pointer chunk_s;

  Assert(BarrierPhase(&pstate->build_barrier) == PHJ_BUILD_HASHING_INNER);

     
                                                                            
                                                                            
                         
     
  switch (PHJ_GROW_BUCKETS_PHASE(BarrierPhase(&pstate->grow_buckets_barrier)))
  {
  case PHJ_GROW_BUCKETS_ELECTING:
                                                                
    if (BarrierArriveAndWait(&pstate->grow_buckets_barrier, WAIT_EVENT_HASH_GROW_BUCKETS_ELECTING))
    {
      size_t size;
      dsa_pointer_atomic *buckets;

                                                
      pstate->nbuckets *= 2;
      size = pstate->nbuckets * sizeof(dsa_pointer_atomic);
      hashtable->batches[0].shared->size += size / 2;
      dsa_free(hashtable->area, hashtable->batches[0].shared->buckets);
      hashtable->batches[0].shared->buckets = dsa_allocate(hashtable->area, size);
      buckets = (dsa_pointer_atomic *)dsa_get_address(hashtable->area, hashtable->batches[0].shared->buckets);
      for (i = 0; i < pstate->nbuckets; ++i)
      {
        dsa_pointer_atomic_init(&buckets[i], InvalidDsaPointer);
      }

                                                   
      pstate->chunk_work_queue = hashtable->batches[0].shared->chunks;

                           
      pstate->growth = PHJ_GROWTH_OK;
    }
                       

  case PHJ_GROW_BUCKETS_ALLOCATING:
                                         
    BarrierArriveAndWait(&pstate->grow_buckets_barrier, WAIT_EVENT_HASH_GROW_BUCKETS_ALLOCATING);
                       

  case PHJ_GROW_BUCKETS_REINSERTING:
                                                  
    ExecParallelHashEnsureBatchAccessors(hashtable);
    ExecParallelHashTableSetCurrentBatch(hashtable, 0);
    while ((chunk = ExecParallelHashPopChunkQueue(hashtable, &chunk_s)))
    {
      size_t idx = 0;

      while (idx < chunk->used)
      {
        HashJoinTuple hashTuple = (HashJoinTuple)(HASH_CHUNK_DATA(chunk) + idx);
        dsa_pointer shared = chunk_s + HASH_CHUNK_HEADER_SIZE + idx;
        int bucketno;
        int batchno;

        ExecHashGetBucketAndBatch(hashtable, hashTuple->hashvalue, &bucketno, &batchno);
        Assert(batchno == 0);

                                                
        ExecParallelHashPushTuple(&hashtable->buckets.shared[bucketno], hashTuple, shared);

                                          
        idx += MAXALIGN(HJTUPLE_OVERHEAD + HJTUPLE_MINTUPLE(hashTuple)->t_len);
      }

                                             
      CHECK_FOR_INTERRUPTS();
    }
    BarrierArriveAndWait(&pstate->grow_buckets_barrier, WAIT_EVENT_HASH_GROW_BUCKETS_REINSERTING);
  }
}

   
                       
                                                                   
                                                    
   
                                                                              
                                                                              
                                                                            
                                                                              
                                 
   
void
ExecHashTableInsert(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue)
{
  bool shouldFree;
  MinimalTuple tuple = ExecFetchSlotMinimalTuple(slot, &shouldFree);
  int bucketno;
  int batchno;

  ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno, &batchno);

     
                                                                      
     
  if (batchno == hashtable->curbatch)
  {
       
                                   
       
    HashJoinTuple hashTuple;
    int hashTupleSize;
    double ntuples = (hashtable->totalTuples - hashtable->skewTuples);

                                  
    hashTupleSize = HJTUPLE_OVERHEAD + tuple->t_len;
    hashTuple = (HashJoinTuple)dense_alloc(hashtable, hashTupleSize);

    hashTuple->hashvalue = hashvalue;
    memcpy(HJTUPLE_MINTUPLE(hashTuple), tuple, tuple->t_len);

       
                                                                          
                                                                      
                                                                        
                                 
       
    HeapTupleHeaderClearMatch(HJTUPLE_MINTUPLE(hashTuple));

                                                     
    hashTuple->next.unshared = hashtable->buckets.unshared[bucketno];
    hashtable->buckets.unshared[bucketno] = hashTuple;

       
                                                                        
                                                                       
              
       
    if (hashtable->nbatch == 1 && ntuples > (hashtable->nbuckets_optimal * NTUP_PER_BUCKET))
    {
                                                                  
      if (hashtable->nbuckets_optimal <= INT_MAX / 2 && hashtable->nbuckets_optimal * 2 <= MaxAllocSize / sizeof(HashJoinTuple))
      {
        hashtable->nbuckets_optimal *= 2;
        hashtable->log2_nbuckets_optimal += 1;
      }
    }

                                                                     
    hashtable->spaceUsed += hashTupleSize;
    if (hashtable->spaceUsed > hashtable->spacePeak)
    {
      hashtable->spacePeak = hashtable->spaceUsed;
    }
    if (hashtable->spaceUsed + hashtable->nbuckets_optimal * sizeof(HashJoinTuple) > hashtable->spaceAllowed)
    {
      ExecHashIncreaseNumBatches(hashtable);
    }
  }
  else
  {
       
                                                        
       
    Assert(batchno > hashtable->curbatch);
    ExecHashJoinSaveTuple(tuple, hashvalue, &hashtable->innerBatchFile[batchno]);
  }

  if (shouldFree)
  {
    heap_free_minimal_tuple(tuple);
  }
}

   
                               
                                                                       
   
void
ExecParallelHashTableInsert(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue)
{
  bool shouldFree;
  MinimalTuple tuple = ExecFetchSlotMinimalTuple(slot, &shouldFree);
  dsa_pointer shared;
  int bucketno;
  int batchno;

retry:
  ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno, &batchno);

  if (batchno == 0)
  {
    HashJoinTuple hashTuple;

                                     
    Assert(BarrierPhase(&hashtable->parallel_state->build_barrier) == PHJ_BUILD_HASHING_INNER);
    hashTuple = ExecParallelHashTupleAlloc(hashtable, HJTUPLE_OVERHEAD + tuple->t_len, &shared);
    if (hashTuple == NULL)
    {
      goto retry;
    }

                                                           
    hashTuple->hashvalue = hashvalue;
    memcpy(HJTUPLE_MINTUPLE(hashTuple), tuple, tuple->t_len);

                                                     
    ExecParallelHashPushTuple(&hashtable->buckets.shared[bucketno], hashTuple, shared);
  }
  else
  {
    size_t tuple_size = MAXALIGN(HJTUPLE_OVERHEAD + tuple->t_len);

    Assert(batchno > 0);

                                                             
    if (hashtable->batches[batchno].preallocated < tuple_size)
    {
      if (!ExecParallelHashTuplePrealloc(hashtable, batchno, tuple_size))
      {
        goto retry;
      }
    }

    Assert(hashtable->batches[batchno].preallocated >= tuple_size);
    hashtable->batches[batchno].preallocated -= tuple_size;
    sts_puttuple(hashtable->batches[batchno].inner_tuples, &hashvalue, tuple);
  }
  ++hashtable->batches[batchno].ntuples;

  if (shouldFree)
  {
    heap_free_minimal_tuple(tuple);
  }
}

   
                                                       
                                                                               
                                                                            
                                                                          
   
void
ExecParallelHashTableInsertCurrentBatch(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue)
{
  bool shouldFree;
  MinimalTuple tuple = ExecFetchSlotMinimalTuple(slot, &shouldFree);
  HashJoinTuple hashTuple;
  dsa_pointer shared;
  int batchno;
  int bucketno;

  ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno, &batchno);
  Assert(batchno == hashtable->curbatch);
  hashTuple = ExecParallelHashTupleAlloc(hashtable, HJTUPLE_OVERHEAD + tuple->t_len, &shared);
  hashTuple->hashvalue = hashvalue;
  memcpy(HJTUPLE_MINTUPLE(hashTuple), tuple, tuple->t_len);
  HeapTupleHeaderClearMatch(HJTUPLE_MINTUPLE(hashTuple));
  ExecParallelHashPushTuple(&hashtable->buckets.shared[bucketno], hashTuple, shared);

  if (shouldFree)
  {
    heap_free_minimal_tuple(tuple);
  }
}

   
                        
                                       
   
                                                                             
                                                                             
                                                                      
                                                                               
                                                                          
                                                                             
                                                         
   
                                                                             
                                                                          
                                                                          
                                                                       
   
bool
ExecHashGetHashValue(HashJoinTable hashtable, ExprContext *econtext, List *hashkeys, bool outer_tuple, bool keep_nulls, uint32 *hashvalue)
{
  uint32 hashkey = 0;
  FmgrInfo *hashfunctions;
  ListCell *hk;
  int i = 0;
  MemoryContext oldContext;

     
                                                                             
                          
     
  ResetExprContext(econtext);

  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

  if (outer_tuple)
  {
    hashfunctions = hashtable->outer_hashfunctions;
  }
  else
  {
    hashfunctions = hashtable->inner_hashfunctions;
  }

  foreach (hk, hashkeys)
  {
    ExprState *keyexpr = (ExprState *)lfirst(hk);
    Datum keyval;
    bool isNull;

                                                
    hashkey = (hashkey << 1) | ((hashkey & 0x80000000) ? 1 : 0);

       
                                                 
       
    keyval = ExecEvalExpr(keyexpr, econtext, &isNull);

       
                                                                       
                                                                
                                                                           
                                                                     
                                                                           
                                                                        
                                                                        
       
                                                                        
                                                                          
                                                                
       
    if (isNull)
    {
      if (hashtable->hashStrict[i] && !keep_nulls)
      {
        MemoryContextSwitchTo(oldContext);
        return false;                   
      }
                                                                    
    }
    else
    {
                                     
      uint32 hkey;

      hkey = DatumGetUInt32(FunctionCall1Coll(&hashfunctions[i], hashtable->collations[i], keyval));
      hashkey ^= hkey;
    }

    i++;
  }

  MemoryContextSwitchTo(oldContext);

  *hashvalue = hashkey;
  return true;
}

   
                             
                                                                  
   
                                                                          
                                                                       
                                                                       
                               
                                      
                                                       
                                                                            
                                                                             
                                                                           
                                                          
   
                                                                              
                                                                             
                                                                               
                               
   
                                                                            
                                                                           
                                                                              
                                                                              
                                                                           
                                               
   
void
ExecHashGetBucketAndBatch(HashJoinTable hashtable, uint32 hashvalue, int *bucketno, int *batchno)
{
  uint32 nbuckets = (uint32)hashtable->nbuckets;
  uint32 nbatch = (uint32)hashtable->nbatch;

  if (nbatch > 1)
  {
    *bucketno = hashvalue & (nbuckets - 1);
    *batchno = pg_rotate_right32(hashvalue, hashtable->log2_nbuckets) & (nbatch - 1);
  }
  else
  {
    *bucketno = hashvalue & (nbuckets - 1);
    *batchno = 0;
  }
}

   
                      
                                                              
   
                                                                        
   
                                                                       
                                                                          
                   
   
bool
ExecScanHashBucket(HashJoinState *hjstate, ExprContext *econtext)
{
  ExprState *hjclauses = hjstate->hashclauses;
  HashJoinTable hashtable = hjstate->hj_HashTable;
  HashJoinTuple hashTuple = hjstate->hj_CurTuple;
  uint32 hashvalue = hjstate->hj_CurHashValue;

     
                                                                            
                                                                  
     
                                                                    
                                                   
     
  if (hashTuple != NULL)
  {
    hashTuple = hashTuple->next.unshared;
  }
  else if (hjstate->hj_CurSkewBucketNo != INVALID_SKEW_BUCKET_NO)
  {
    hashTuple = hashtable->skewBucket[hjstate->hj_CurSkewBucketNo]->tuples;
  }
  else
  {
    hashTuple = hashtable->buckets.unshared[hjstate->hj_CurBucketNo];
  }

  while (hashTuple != NULL)
  {
    if (hashTuple->hashvalue == hashvalue)
    {
      TupleTableSlot *inntuple;

                                                                       
      inntuple = ExecStoreMinimalTuple(HJTUPLE_MINTUPLE(hashTuple), hjstate->hj_HashTupleSlot, false);                   
      econtext->ecxt_innertuple = inntuple;

      if (ExecQualAndReset(hjclauses, econtext))
      {
        hjstate->hj_CurTuple = hashTuple;
        return true;
      }
    }

    hashTuple = hashTuple->next.unshared;
  }

     
              
     
  return false;
}

   
                              
                                                              
   
                                                                        
   
                                                                       
                                                                          
                   
   
bool
ExecParallelScanHashBucket(HashJoinState *hjstate, ExprContext *econtext)
{
  ExprState *hjclauses = hjstate->hashclauses;
  HashJoinTable hashtable = hjstate->hj_HashTable;
  HashJoinTuple hashTuple = hjstate->hj_CurTuple;
  uint32 hashvalue = hjstate->hj_CurHashValue;

     
                                                                            
                                                                  
     
  if (hashTuple != NULL)
  {
    hashTuple = ExecParallelHashNextTuple(hashtable, hashTuple);
  }
  else
  {
    hashTuple = ExecParallelHashFirstTuple(hashtable, hjstate->hj_CurBucketNo);
  }

  while (hashTuple != NULL)
  {
    if (hashTuple->hashvalue == hashvalue)
    {
      TupleTableSlot *inntuple;

                                                                       
      inntuple = ExecStoreMinimalTuple(HJTUPLE_MINTUPLE(hashTuple), hjstate->hj_HashTupleSlot, false);                   
      econtext->ecxt_innertuple = inntuple;

      if (ExecQualAndReset(hjclauses, econtext))
      {
        hjstate->hj_CurTuple = hashTuple;
        return true;
      }
    }

    hashTuple = ExecParallelHashNextTuple(hashtable, hashTuple);
  }

     
              
     
  return false;
}

   
                                 
                                                               
   
void
ExecPrepHashTableForUnmatched(HashJoinState *hjstate)
{
               
                                                                  
     
                                                 
                                                                         
                                                                    
               
     
  hjstate->hj_CurBucketNo = 0;
  hjstate->hj_CurSkewBucketNo = 0;
  hjstate->hj_CurTuple = NULL;
}

   
                                 
                                                   
   
                                                                       
                                                                          
                   
   
bool
ExecScanHashTableForUnmatched(HashJoinState *hjstate, ExprContext *econtext)
{
  HashJoinTable hashtable = hjstate->hj_HashTable;
  HashJoinTuple hashTuple = hjstate->hj_CurTuple;

  for (;;)
  {
       
                                                                      
                                                                    
               
       
    if (hashTuple != NULL)
    {
      hashTuple = hashTuple->next.unshared;
    }
    else if (hjstate->hj_CurBucketNo < hashtable->nbuckets)
    {
      hashTuple = hashtable->buckets.unshared[hjstate->hj_CurBucketNo];
      hjstate->hj_CurBucketNo++;
    }
    else if (hjstate->hj_CurSkewBucketNo < hashtable->nSkewBuckets)
    {
      int j = hashtable->skewBucketNums[hjstate->hj_CurSkewBucketNo];

      hashTuple = hashtable->skewBucket[j]->tuples;
      hjstate->hj_CurSkewBucketNo++;
    }
    else
    {
      break;                           
    }

    while (hashTuple != NULL)
    {
      if (!HeapTupleHeaderHasMatch(HJTUPLE_MINTUPLE(hashTuple)))
      {
        TupleTableSlot *inntuple;

                                                     
        inntuple = ExecStoreMinimalTuple(HJTUPLE_MINTUPLE(hashTuple), hjstate->hj_HashTupleSlot, false);                   
        econtext->ecxt_innertuple = inntuple;

           
                                                                       
                                                               
                                           
           
        ResetExprContext(econtext);

        hjstate->hj_CurTuple = hashTuple;
        return true;
      }

      hashTuple = hashTuple->next.unshared;
    }

                                           
    CHECK_FOR_INTERRUPTS();
  }

     
                              
     
  return false;
}

   
                      
   
                                          
   
void
ExecHashTableReset(HashJoinTable hashtable)
{
  MemoryContext oldcxt;
  int nbuckets = hashtable->nbuckets;

     
                                                                             
                                              
     
  MemoryContextReset(hashtable->batchCxt);
  oldcxt = MemoryContextSwitchTo(hashtable->batchCxt);

                                                            
  hashtable->buckets.unshared = (HashJoinTuple *)palloc0(nbuckets * sizeof(HashJoinTuple));

  hashtable->spaceUsed = 0;

  MemoryContextSwitchTo(oldcxt);

                                                                            
  hashtable->chunks = NULL;
}

   
                                
                                                             
   
void
ExecHashTableResetMatchFlags(HashJoinTable hashtable)
{
  HashJoinTuple tuple;
  int i;

                                             
  for (i = 0; i < hashtable->nbuckets; i++)
  {
    for (tuple = hashtable->buckets.unshared[i]; tuple != NULL; tuple = tuple->next.unshared)
    {
      HeapTupleHeaderClearMatch(HJTUPLE_MINTUPLE(tuple));
    }
  }

                                                     
  for (i = 0; i < hashtable->nSkewBuckets; i++)
  {
    int j = hashtable->skewBucketNums[i];
    HashSkewBucket *skewBucket = hashtable->skewBucket[j];

    for (tuple = skewBucket->tuples; tuple != NULL; tuple = tuple->next.unshared)
    {
      HeapTupleHeaderClearMatch(HJTUPLE_MINTUPLE(tuple));
    }
  }
}

void
ExecReScanHash(HashState *node)
{
     
                                                                        
                         
     
  if (node->ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->ps.lefttree);
  }
}

   
                         
   
                                                                           
                                                                         
                                                                      
                               
   
static void
ExecHashBuildSkewHash(HashJoinTable hashtable, Hash *node, int mcvsToUse)
{
  HeapTupleData *statsTuple;
  AttStatsSlot sslot;

                                                                           
  if (!OidIsValid(node->skewTable))
  {
    return;
  }
                                                                           
  if (mcvsToUse <= 0)
  {
    return;
  }

     
                                                                       
     
  statsTuple = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(node->skewTable), Int16GetDatum(node->skewColumn), BoolGetDatum(node->skewInherit));
  if (!HeapTupleIsValid(statsTuple))
  {
    return;
  }

  if (get_attstatsslot(&sslot, statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS))
  {
    double frac;
    int nbuckets;
    FmgrInfo *hashfunctions;
    int i;

    if (mcvsToUse > sslot.nvalues)
    {
      mcvsToUse = sslot.nvalues;
    }

       
                                                                   
                                                                     
                                                             
       
    frac = 0;
    for (i = 0; i < mcvsToUse; i++)
    {
      frac += sslot.numbers[i];
    }
    if (frac < SKEW_MIN_OUTER_FRACTION)
    {
      free_attstatsslot(&sslot);
      ReleaseSysCache(statsTuple);
      return;
    }

       
                                        
       
                                                                           
                                                                           
                                                                
                   
       
                                                                    
                                                                         
                                                                   
       
    nbuckets = 2;
    while (nbuckets <= mcvsToUse)
    {
      nbuckets <<= 1;
    }
                                                         
    nbuckets <<= 2;

    hashtable->skewEnabled = true;
    hashtable->skewBucketLen = nbuckets;

       
                                                                          
                                                                          
                                                           
       
    hashtable->skewBucket = (HashSkewBucket **)MemoryContextAllocZero(hashtable->batchCxt, nbuckets * sizeof(HashSkewBucket *));
    hashtable->skewBucketNums = (int *)MemoryContextAllocZero(hashtable->batchCxt, mcvsToUse * sizeof(int));

    hashtable->spaceUsed += nbuckets * sizeof(HashSkewBucket *) + mcvsToUse * sizeof(int);
    hashtable->spaceUsedSkew += nbuckets * sizeof(HashSkewBucket *) + mcvsToUse * sizeof(int);
    if (hashtable->spaceUsed > hashtable->spacePeak)
    {
      hashtable->spacePeak = hashtable->spaceUsed;
    }

       
                                                     
       
                                                                         
                                                                          
                                                                  
                                                                          
                         
       
    hashfunctions = hashtable->outer_hashfunctions;

    for (i = 0; i < mcvsToUse; i++)
    {
      uint32 hashvalue;
      int bucket;

      hashvalue = DatumGetUInt32(FunctionCall1Coll(&hashfunctions[0], hashtable->collations[0], sslot.values[i]));

         
                                                                        
                                                                      
                                                                     
                                      
         
      bucket = hashvalue & (nbuckets - 1);
      while (hashtable->skewBucket[bucket] != NULL && hashtable->skewBucket[bucket]->hashvalue != hashvalue)
      {
        bucket = (bucket + 1) & (nbuckets - 1);
      }

         
                                                                       
                                                                 
         
      if (hashtable->skewBucket[bucket] != NULL)
      {
        continue;
      }

                                                              
      hashtable->skewBucket[bucket] = (HashSkewBucket *)MemoryContextAlloc(hashtable->batchCxt, sizeof(HashSkewBucket));
      hashtable->skewBucket[bucket]->hashvalue = hashvalue;
      hashtable->skewBucket[bucket]->tuples = NULL;
      hashtable->skewBucketNums[hashtable->nSkewBuckets] = bucket;
      hashtable->nSkewBuckets++;
      hashtable->spaceUsed += SKEW_BUCKET_OVERHEAD;
      hashtable->spaceUsedSkew += SKEW_BUCKET_OVERHEAD;
      if (hashtable->spaceUsed > hashtable->spacePeak)
      {
        hashtable->spacePeak = hashtable->spaceUsed;
      }
    }

    free_attstatsslot(&sslot);
  }

  ReleaseSysCache(statsTuple);
}

   
                         
   
                                                             
                                                      
                                            
   
int
ExecHashGetSkewBucket(HashJoinTable hashtable, uint32 hashvalue)
{
  int bucket;

     
                                                                             
                                                                
     
  if (!hashtable->skewEnabled)
  {
    return INVALID_SKEW_BUCKET_NO;
  }

     
                                                                        
     
  bucket = hashvalue & (hashtable->skewBucketLen - 1);

     
                                                                        
                                                                             
                           
     
  while (hashtable->skewBucket[bucket] != NULL && hashtable->skewBucket[bucket]->hashvalue != hashvalue)
  {
    bucket = (bucket + 1) & (hashtable->skewBucketLen - 1);
  }

     
                               
     
  if (hashtable->skewBucket[bucket] != NULL)
  {
    return bucket;
  }

     
                                                                
     
  return INVALID_SKEW_BUCKET_NO;
}

   
                           
   
                                            
   
                                                                 
                        
   
static void
ExecHashSkewTableInsert(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue, int bucketNumber)
{
  bool shouldFree;
  MinimalTuple tuple = ExecFetchSlotMinimalTuple(slot, &shouldFree);
  HashJoinTuple hashTuple;
  int hashTupleSize;

                                
  hashTupleSize = HJTUPLE_OVERHEAD + tuple->t_len;
  hashTuple = (HashJoinTuple)MemoryContextAlloc(hashtable->batchCxt, hashTupleSize);
  hashTuple->hashvalue = hashvalue;
  memcpy(HJTUPLE_MINTUPLE(hashTuple), tuple, tuple->t_len);
  HeapTupleHeaderClearMatch(HJTUPLE_MINTUPLE(hashTuple));

                                                        
  hashTuple->next.unshared = hashtable->skewBucket[bucketNumber]->tuples;
  hashtable->skewBucket[bucketNumber]->tuples = hashTuple;
  Assert(hashTuple != hashTuple->next.unshared);

                                                                   
  hashtable->spaceUsed += hashTupleSize;
  hashtable->spaceUsedSkew += hashTupleSize;
  if (hashtable->spaceUsed > hashtable->spacePeak)
  {
    hashtable->spacePeak = hashtable->spaceUsed;
  }
  while (hashtable->spaceUsedSkew > hashtable->spaceAllowedSkew)
  {
    ExecHashRemoveNextSkewBucket(hashtable);
  }

                                                            
  if (hashtable->spaceUsed > hashtable->spaceAllowed)
  {
    ExecHashIncreaseNumBatches(hashtable);
  }

  if (shouldFree)
  {
    heap_free_minimal_tuple(tuple);
  }
}

   
                                 
   
                                                                     
                         
   
static void
ExecHashRemoveNextSkewBucket(HashJoinTable hashtable)
{
  int bucketToRemove;
  HashSkewBucket *bucket;
  uint32 hashvalue;
  int bucketno;
  int batchno;
  HashJoinTuple hashTuple;

                                   
  bucketToRemove = hashtable->skewBucketNums[hashtable->nSkewBuckets - 1];
  bucket = hashtable->skewBucket[bucketToRemove];

     
                                                                       
                                                                             
                                                                             
                                   
     
  hashvalue = bucket->hashvalue;
  ExecHashGetBucketAndBatch(hashtable, hashvalue, &bucketno, &batchno);

                                        
  hashTuple = bucket->tuples;
  while (hashTuple != NULL)
  {
    HashJoinTuple nextHashTuple = hashTuple->next.unshared;
    MinimalTuple tuple;
    Size tupleSize;

       
                                                                     
                                                                     
                                                            
       
    tuple = HJTUPLE_MINTUPLE(hashTuple);
    tupleSize = HJTUPLE_OVERHEAD + tuple->t_len;

                                                                          
    if (batchno == hashtable->curbatch)
    {
                                                 
      HashJoinTuple copyTuple;

         
                                                                         
                                                      
         
      copyTuple = (HashJoinTuple)dense_alloc(hashtable, tupleSize);
      memcpy(copyTuple, hashTuple, tupleSize);
      pfree(hashTuple);

      copyTuple->next.unshared = hashtable->buckets.unshared[bucketno];
      hashtable->buckets.unshared[bucketno] = copyTuple;

                                                                        
      hashtable->spaceUsedSkew -= tupleSize;
    }
    else
    {
                                                            
      Assert(batchno > hashtable->curbatch);
      ExecHashJoinSaveTuple(tuple, hashvalue, &hashtable->innerBatchFile[batchno]);
      pfree(hashTuple);
      hashtable->spaceUsed -= tupleSize;
      hashtable->spaceUsedSkew -= tupleSize;
    }

    hashTuple = nextHashTuple;

                                           
    CHECK_FOR_INTERRUPTS();
  }

     
                                                                          
     
                                                                            
                                                                           
                                                                            
                                                                             
                                                                         
                                                                             
                                                                         
                                                       
     
  hashtable->skewBucket[bucketToRemove] = NULL;
  hashtable->nSkewBuckets--;
  pfree(bucket);
  hashtable->spaceUsed -= SKEW_BUCKET_OVERHEAD;
  hashtable->spaceUsedSkew -= SKEW_BUCKET_OVERHEAD;

     
                                                                            
                                                           
     
  if (hashtable->nSkewBuckets == 0)
  {
    hashtable->skewEnabled = false;
    pfree(hashtable->skewBucket);
    pfree(hashtable->skewBucketNums);
    hashtable->skewBucket = NULL;
    hashtable->skewBucketNums = NULL;
    hashtable->spaceUsed -= hashtable->spaceUsedSkew;
    hashtable->spaceUsedSkew = 0;
  }
}

   
                                                              
   
void
ExecHashEstimate(HashState *node, ParallelContext *pcxt)
{
  size_t size;

                                                          
  if (!node->ps.instrument || pcxt->nworkers == 0)
  {
    return;
  }

  size = mul_size(pcxt->nworkers, sizeof(HashInstrumentation));
  size = add_size(size, offsetof(SharedHashInfo, hinstrument));
  shm_toc_estimate_chunk(&pcxt->estimator, size);
  shm_toc_estimate_keys(&pcxt->estimator, 1);
}

   
                                                                            
                           
   
void
ExecHashInitializeDSM(HashState *node, ParallelContext *pcxt)
{
  size_t size;

                                                          
  if (!node->ps.instrument || pcxt->nworkers == 0)
  {
    return;
  }

  size = offsetof(SharedHashInfo, hinstrument) + pcxt->nworkers * sizeof(HashInstrumentation);
  node->shared_info = (SharedHashInfo *)shm_toc_allocate(pcxt->toc, size);
  memset(node->shared_info, 0, size);
  node->shared_info->num_workers = pcxt->nworkers;
  shm_toc_insert(pcxt->toc, node->ps.plan->plan_node_id, node->shared_info);
}

   
                                                                             
                        
   
void
ExecHashInitializeWorker(HashState *node, ParallelWorkerContext *pwcxt)
{
  SharedHashInfo *shared_info;

                                            
  if (!node->ps.instrument)
  {
    return;
  }

  shared_info = (SharedHashInfo *)shm_toc_lookup(pwcxt->toc, node->ps.plan->plan_node_id, false);
  node->hinstrument = &shared_info->hinstrument[ParallelWorkerNumber];
}

   
                                                                             
                                                                         
                                                                              
                                        
   
void
ExecShutdownHash(HashState *node)
{
  if (node->hinstrument && node->hashtable)
  {
    ExecHashGetInstrumentation(node->hinstrument, node->hashtable);
  }
}

   
                                                                        
                                            
   
void
ExecHashRetrieveInstrumentation(HashState *node)
{
  SharedHashInfo *shared_info = node->shared_info;
  size_t size;

  if (shared_info == NULL)
  {
    return;
  }

                                                                      
  size = offsetof(SharedHashInfo, hinstrument) + shared_info->num_workers * sizeof(HashInstrumentation);
  node->shared_info = palloc(size);
  memcpy(node->shared_info, shared_info, size);
}

   
                                                                             
           
   
void
ExecHashGetInstrumentation(HashInstrumentation *instrument, HashJoinTable hashtable)
{
  instrument->nbuckets = hashtable->nbuckets;
  instrument->nbuckets_original = hashtable->nbuckets_original;
  instrument->nbatch = hashtable->nbatch;
  instrument->nbatch_original = hashtable->nbatch_original;
  instrument->space_peak = hashtable->spacePeak;
}

   
                                                                   
   
static void *
dense_alloc(HashJoinTable hashtable, Size size)
{
  HashMemoryChunk newChunk;
  char *ptr;

                                                             
  size = MAXALIGN(size);

     
                                                                        
     
  if (size > HASH_CHUNK_THRESHOLD)
  {
                                                                    
    newChunk = (HashMemoryChunk)MemoryContextAlloc(hashtable->batchCxt, HASH_CHUNK_HEADER_SIZE + size);
    newChunk->maxlen = size;
    newChunk->used = size;
    newChunk->ntuples = 1;

       
                                                                          
                                                                 
       
    if (hashtable->chunks != NULL)
    {
      newChunk->next = hashtable->chunks->next;
      hashtable->chunks->next.unshared = newChunk;
    }
    else
    {
      newChunk->next.unshared = hashtable->chunks;
      hashtable->chunks = newChunk;
    }

    return HASH_CHUNK_DATA(newChunk);
  }

     
                                                                          
                                  
     
  if ((hashtable->chunks == NULL) || (hashtable->chunks->maxlen - hashtable->chunks->used) < size)
  {
                                                                    
    newChunk = (HashMemoryChunk)MemoryContextAlloc(hashtable->batchCxt, HASH_CHUNK_HEADER_SIZE + HASH_CHUNK_SIZE);

    newChunk->maxlen = HASH_CHUNK_SIZE;
    newChunk->used = size;
    newChunk->ntuples = 1;

    newChunk->next.unshared = hashtable->chunks;
    hashtable->chunks = newChunk;

    return HASH_CHUNK_DATA(newChunk);
  }

                                                                       
  ptr = HASH_CHUNK_DATA(hashtable->chunks) + hashtable->chunks->used;
  hashtable->chunks->used += size;
  hashtable->chunks->ntuples += 1;

                                                       
  return ptr;
}

   
                                                                              
                                                          
   
                                                                            
                                                                            
                                                                               
                                                                              
                                                                       
                                                                    
   
static HashJoinTuple
ExecParallelHashTupleAlloc(HashJoinTable hashtable, size_t size, dsa_pointer *shared)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  dsa_pointer chunk_shared;
  HashMemoryChunk chunk;
  Size chunk_size;
  HashJoinTuple result;
  int curbatch = hashtable->curbatch;

  size = MAXALIGN(size);

     
                                                                          
                                               
     
  chunk = hashtable->current_chunk;
  if (chunk != NULL && size <= HASH_CHUNK_THRESHOLD && chunk->maxlen - chunk->used >= size)
  {

    chunk_shared = hashtable->current_chunk_shared;
    Assert(chunk == dsa_get_address(hashtable->area, chunk_shared));
    *shared = chunk_shared + HASH_CHUNK_HEADER_SIZE + chunk->used;
    result = (HashJoinTuple)(HASH_CHUNK_DATA(chunk) + chunk->used);
    chunk->used += size;

    Assert(chunk->used <= chunk->maxlen);
    Assert(result == dsa_get_address(hashtable->area, *shared));

    return result;
  }

                                               
  LWLockAcquire(&pstate->lock, LW_EXCLUSIVE);

     
                                                                         
     
  if (pstate->growth == PHJ_GROWTH_NEED_MORE_BATCHES || pstate->growth == PHJ_GROWTH_NEED_MORE_BUCKETS)
  {
    ParallelHashGrowth growth = pstate->growth;

    hashtable->current_chunk = NULL;
    LWLockRelease(&pstate->lock);

                                                            
    if (growth == PHJ_GROWTH_NEED_MORE_BATCHES)
    {
      ExecParallelHashIncreaseNumBatches(hashtable);
    }
    else if (growth == PHJ_GROWTH_NEED_MORE_BUCKETS)
    {
      ExecParallelHashIncreaseNumBuckets(hashtable);
    }

                                
    return NULL;
  }

                                             
  if (size > HASH_CHUNK_THRESHOLD)
  {
    chunk_size = size + HASH_CHUNK_HEADER_SIZE;
  }
  else
  {
    chunk_size = HASH_CHUNK_SIZE;
  }

                                                      
  if (pstate->growth != PHJ_GROWTH_DISABLED)
  {
    Assert(curbatch == 0);
    Assert(BarrierPhase(&pstate->build_barrier) == PHJ_BUILD_HASHING_INNER);

       
                                                                        
                                                                          
                                                    
       
    if (hashtable->batches[0].at_least_one_chunk && hashtable->batches[0].shared->size + chunk_size > pstate->space_allowed)
    {
      pstate->growth = PHJ_GROWTH_NEED_MORE_BATCHES;
      hashtable->batches[0].shared->space_exhausted = true;
      LWLockRelease(&pstate->lock);

      return NULL;
    }

                                                           
    if (hashtable->nbatch == 1)
    {
      hashtable->batches[0].shared->ntuples += hashtable->batches[0].ntuples;
      hashtable->batches[0].ntuples = 0;
                                                                  
      if (hashtable->batches[0].shared->ntuples + 1 > hashtable->nbuckets * NTUP_PER_BUCKET && hashtable->nbuckets < (INT_MAX / 2) && hashtable->nbuckets * 2 <= MaxAllocSize / sizeof(dsa_pointer_atomic))
      {
        pstate->growth = PHJ_GROWTH_NEED_MORE_BUCKETS;
        LWLockRelease(&pstate->lock);

        return NULL;
      }
    }
  }

                                               
  chunk_shared = dsa_allocate(hashtable->area, chunk_size);
  hashtable->batches[curbatch].shared->size += chunk_size;
  hashtable->batches[curbatch].at_least_one_chunk = true;

                         
  chunk = (HashMemoryChunk)dsa_get_address(hashtable->area, chunk_shared);
  *shared = chunk_shared + HASH_CHUNK_HEADER_SIZE;
  chunk->maxlen = chunk_size - HASH_CHUNK_HEADER_SIZE;
  chunk->used = size;

     
                                                                            
                                                                            
                                       
     
  chunk->next.shared = hashtable->batches[curbatch].shared->chunks;
  hashtable->batches[curbatch].shared->chunks = chunk_shared;

  if (size <= HASH_CHUNK_THRESHOLD)
  {
       
                                                                       
                                               
       
    hashtable->current_chunk = chunk;
    hashtable->current_chunk_shared = chunk_shared;
  }
  LWLockRelease(&pstate->lock);

  Assert(HASH_CHUNK_DATA(chunk) == dsa_get_address(hashtable->area, *shared));
  result = (HashJoinTuple)HASH_CHUNK_DATA(chunk);

  return result;
}

   
                                                                             
                                                                          
                                                  
   
static void
ExecParallelHashJoinSetUpBatches(HashJoinTable hashtable, int nbatch)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  ParallelHashJoinBatch *batches;
  MemoryContext oldcxt;
  int i;

  Assert(hashtable->batches == NULL);

                       
  pstate->batches = dsa_allocate0(hashtable->area, EstimateParallelHashJoinBatch(hashtable) * nbatch);
  pstate->nbatch = nbatch;
  batches = dsa_get_address(hashtable->area, pstate->batches);

                                     
  oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

                                               
  hashtable->nbatch = nbatch;
  hashtable->batches = (ParallelHashJoinBatchAccessor *)palloc0(sizeof(ParallelHashJoinBatchAccessor) * hashtable->nbatch);

                                                                         
  for (i = 0; i < hashtable->nbatch; ++i)
  {
    ParallelHashJoinBatchAccessor *accessor = &hashtable->batches[i];
    ParallelHashJoinBatch *shared = NthParallelHashJoinBatch(batches, i);
    char name[MAXPGPATH];

       
                                                                         
                       
       
    BarrierInit(&shared->batch_barrier, 0);
    if (i == 0)
    {
                                              
      BarrierAttach(&shared->batch_barrier);
      while (BarrierPhase(&shared->batch_barrier) < PHJ_BATCH_PROBING)
      {
        BarrierArriveAndWait(&shared->batch_barrier, 0);
      }
      BarrierDetach(&shared->batch_barrier);
    }

                                                                        
    accessor->shared = shared;

                                            
    snprintf(name, sizeof(name), "i%dof%d", i, hashtable->nbatch);
    accessor->inner_tuples = sts_initialize(ParallelHashJoinBatchInner(shared), pstate->nparticipants, ParallelWorkerNumber + 1, sizeof(uint32), SHARED_TUPLESTORE_SINGLE_PASS, &pstate->fileset, name);
    snprintf(name, sizeof(name), "o%dof%d", i, hashtable->nbatch);
    accessor->outer_tuples = sts_initialize(ParallelHashJoinBatchOuter(shared, pstate->nparticipants), pstate->nparticipants, ParallelWorkerNumber + 1, sizeof(uint32), SHARED_TUPLESTORE_SINGLE_PASS, &pstate->fileset, name);
  }

  MemoryContextSwitchTo(oldcxt);
}

   
                                                                  
   
static void
ExecParallelHashCloseBatchAccessors(HashJoinTable hashtable)
{
  int i;

  for (i = 0; i < hashtable->nbatch; ++i)
  {
                                           
    sts_end_write(hashtable->batches[i].inner_tuples);
    sts_end_write(hashtable->batches[i].outer_tuples);
    sts_end_parallel_scan(hashtable->batches[i].inner_tuples);
    sts_end_parallel_scan(hashtable->batches[i].outer_tuples);
  }
  pfree(hashtable->batches);
  hashtable->batches = NULL;
}

   
                                                                          
            
   
static void
ExecParallelHashEnsureBatchAccessors(HashJoinTable hashtable)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  ParallelHashJoinBatch *batches;
  MemoryContext oldcxt;
  int i;

  if (hashtable->batches != NULL)
  {
    if (hashtable->nbatch == pstate->nbatch)
    {
      return;
    }
    ExecParallelHashCloseBatchAccessors(hashtable);
  }

     
                                                                         
                                                                         
                                                                         
                                                                             
               
     
  if (!DsaPointerIsValid(pstate->batches))
  {
    return;
  }

                                     
  oldcxt = MemoryContextSwitchTo(hashtable->hashCxt);

                                               
  hashtable->nbatch = pstate->nbatch;
  hashtable->batches = (ParallelHashJoinBatchAccessor *)palloc0(sizeof(ParallelHashJoinBatchAccessor) * hashtable->nbatch);

                                                                           
  batches = (ParallelHashJoinBatch *)dsa_get_address(hashtable->area, pstate->batches);

                                                                
  for (i = 0; i < hashtable->nbatch; ++i)
  {
    ParallelHashJoinBatchAccessor *accessor = &hashtable->batches[i];
    ParallelHashJoinBatch *shared = NthParallelHashJoinBatch(batches, i);

    accessor->shared = shared;
    accessor->preallocated = 0;
    accessor->done = false;
    accessor->inner_tuples = sts_attach(ParallelHashJoinBatchInner(shared), ParallelWorkerNumber + 1, &pstate->fileset);
    accessor->outer_tuples = sts_attach(ParallelHashJoinBatchOuter(shared, pstate->nparticipants), ParallelWorkerNumber + 1, &pstate->fileset);
  }

  MemoryContextSwitchTo(oldcxt);
}

   
                                                                 
   
void
ExecParallelHashTableAlloc(HashJoinTable hashtable, int batchno)
{
  ParallelHashJoinBatch *batch = hashtable->batches[batchno].shared;
  dsa_pointer_atomic *buckets;
  int nbuckets = hashtable->parallel_state->nbuckets;
  int i;

  batch->buckets = dsa_allocate(hashtable->area, sizeof(dsa_pointer_atomic) * nbuckets);
  buckets = (dsa_pointer_atomic *)dsa_get_address(hashtable->area, batch->buckets);
  for (i = 0; i < nbuckets; ++i)
  {
    dsa_pointer_atomic_init(&buckets[i], InvalidDsaPointer);
  }
}

   
                                                                            
                                 
   
void
ExecHashTableDetachBatch(HashJoinTable hashtable)
{
  if (hashtable->parallel_state != NULL && hashtable->curbatch >= 0)
  {
    int curbatch = hashtable->curbatch;
    ParallelHashJoinBatch *batch = hashtable->batches[curbatch].shared;

                                                   
    sts_end_parallel_scan(hashtable->batches[curbatch].inner_tuples);
    sts_end_parallel_scan(hashtable->batches[curbatch].outer_tuples);

                                                        
    if (BarrierArriveAndDetach(&batch->batch_barrier))
    {
         
                                                                      
                                                                      
                                                                   
         
      Assert(BarrierPhase(&batch->batch_barrier) == PHJ_BATCH_DONE);

                                           
      while (DsaPointerIsValid(batch->chunks))
      {
        HashMemoryChunk chunk = dsa_get_address(hashtable->area, batch->chunks);
        dsa_pointer next = chunk->next.shared;

        dsa_free(hashtable->area, batch->chunks);
        batch->chunks = next;
      }
      if (DsaPointerIsValid(batch->buckets))
      {
        dsa_free(hashtable->area, batch->buckets);
        batch->buckets = InvalidDsaPointer;
      }
    }

       
                                                                    
                                                                       
                                                                     
       
    hashtable->spacePeak = Max(hashtable->spacePeak, batch->size + sizeof(dsa_pointer_atomic) * hashtable->nbuckets);

                                                       
    hashtable->curbatch = -1;
  }
}

   
                                                                          
   
void
ExecHashTableDetach(HashJoinTable hashtable)
{
  if (hashtable->parallel_state)
  {
    ParallelHashJoinState *pstate = hashtable->parallel_state;
    int i;

                                                   
    if (hashtable->batches)
    {
      for (i = 0; i < hashtable->nbatch; ++i)
      {
        sts_end_write(hashtable->batches[i].inner_tuples);
        sts_end_write(hashtable->batches[i].outer_tuples);
        sts_end_parallel_scan(hashtable->batches[i].inner_tuples);
        sts_end_parallel_scan(hashtable->batches[i].outer_tuples);
      }
    }

                                                          
    if (BarrierDetach(&pstate->build_barrier))
    {
      if (DsaPointerIsValid(pstate->batches))
      {
        dsa_free(hashtable->area, pstate->batches);
        pstate->batches = InvalidDsaPointer;
      }
    }

    hashtable->parallel_state = NULL;
  }
}

   
                                                               
   
static inline HashJoinTuple
ExecParallelHashFirstTuple(HashJoinTable hashtable, int bucketno)
{
  HashJoinTuple tuple;
  dsa_pointer p;

  Assert(hashtable->parallel_state);
  p = dsa_pointer_atomic_read(&hashtable->buckets.shared[bucketno]);
  tuple = (HashJoinTuple)dsa_get_address(hashtable->area, p);

  return tuple;
}

   
                                                     
   
static inline HashJoinTuple
ExecParallelHashNextTuple(HashJoinTable hashtable, HashJoinTuple tuple)
{
  HashJoinTuple next;

  Assert(hashtable->parallel_state);
  next = (HashJoinTuple)dsa_get_address(hashtable->area, tuple->next.shared);

  return next;
}

   
                                                                              
   
static inline void
ExecParallelHashPushTuple(dsa_pointer_atomic *head, HashJoinTuple tuple, dsa_pointer tuple_shared)
{
  for (;;)
  {
    tuple->next.shared = dsa_pointer_atomic_read(head);
    if (dsa_pointer_atomic_compare_exchange(head, &tuple->next.shared, tuple_shared))
    {
      break;
    }
  }
}

   
                                     
   
void
ExecParallelHashTableSetCurrentBatch(HashJoinTable hashtable, int batchno)
{
  Assert(hashtable->batches[batchno].shared->buckets != InvalidDsaPointer);

  hashtable->curbatch = batchno;
  hashtable->buckets.shared = (dsa_pointer_atomic *)dsa_get_address(hashtable->area, hashtable->batches[batchno].shared->buckets);
  hashtable->nbuckets = hashtable->parallel_state->nbuckets;
  hashtable->log2_nbuckets = my_log2(hashtable->nbuckets);
  hashtable->current_chunk = NULL;
  hashtable->current_chunk_shared = InvalidDsaPointer;
  hashtable->batches[batchno].at_least_one_chunk = false;
}

   
                                                                             
                                                                              
                                                                  
   
static HashMemoryChunk
ExecParallelHashPopChunkQueue(HashJoinTable hashtable, dsa_pointer *shared)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  HashMemoryChunk chunk;

  LWLockAcquire(&pstate->lock, LW_EXCLUSIVE);
  if (DsaPointerIsValid(pstate->chunk_work_queue))
  {
    *shared = pstate->chunk_work_queue;
    chunk = (HashMemoryChunk)dsa_get_address(hashtable->area, *shared);
    pstate->chunk_work_queue = chunk->next.shared;
  }
  else
  {
    chunk = NULL;
  }
  LWLockRelease(&pstate->lock);

  return chunk;
}

   
                                                                              
                                                                           
                                                                         
                                   
   
                                                                               
                                                                               
                                                                           
                                                                              
                                                                           
                                                                             
                                                                              
                                                                     
                  
   
                                                                         
                                                                              
          
   
static bool
ExecParallelHashTuplePrealloc(HashJoinTable hashtable, int batchno, size_t size)
{
  ParallelHashJoinState *pstate = hashtable->parallel_state;
  ParallelHashJoinBatchAccessor *batch = &hashtable->batches[batchno];
  size_t want = Max(size, HASH_CHUNK_SIZE - HASH_CHUNK_HEADER_SIZE);

  Assert(batchno > 0);
  Assert(batchno < hashtable->nbatch);
  Assert(size == MAXALIGN(size));

  LWLockAcquire(&pstate->lock, LW_EXCLUSIVE);

                                                          
  if (pstate->growth == PHJ_GROWTH_NEED_MORE_BATCHES || pstate->growth == PHJ_GROWTH_NEED_MORE_BUCKETS)
  {
    ParallelHashGrowth growth = pstate->growth;

    LWLockRelease(&pstate->lock);
    if (growth == PHJ_GROWTH_NEED_MORE_BATCHES)
    {
      ExecParallelHashIncreaseNumBatches(hashtable);
    }
    else if (growth == PHJ_GROWTH_NEED_MORE_BUCKETS)
    {
      ExecParallelHashIncreaseNumBuckets(hashtable);
    }

    return false;
  }

  if (pstate->growth != PHJ_GROWTH_DISABLED && batch->at_least_one_chunk && (batch->shared->estimated_size + want + HASH_CHUNK_HEADER_SIZE > pstate->space_allowed))
  {
       
                                                                           
                                                                          
       
    batch->shared->space_exhausted = true;
    pstate->growth = PHJ_GROWTH_NEED_MORE_BATCHES;
    LWLockRelease(&pstate->lock);

    return false;
  }

  batch->at_least_one_chunk = true;
  batch->shared->estimated_size += want + HASH_CHUNK_HEADER_SIZE;
  batch->preallocated = want;
  LWLockRelease(&pstate->lock);

  return true;
}
