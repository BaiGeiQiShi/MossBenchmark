/*-------------------------------------------------------------------------
 *
 * nodeHash.c
 *	  Routines to hash relations for hashjoin
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeHash.c
 *
 * See note on parallelism in nodeHashjoin.c.
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *		MultiExecHash	- generate an in-memory hash table of the
 *relation ExecInitHash	- initialize node and subnodes ExecEndHash
 *- shutdown node and subnodes
 */

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

/* ----------------------------------------------------------------
 *		ExecHash
 *
 *		stub for pro forma compliance
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecHash(PlanState *pstate)
{


}

/* ----------------------------------------------------------------
 *		MultiExecHash
 *
 *		build hash table for hashjoin, doing partitioning if more
 *		than one batch is required.
 * ----------------------------------------------------------------
 */
Node *
MultiExecHash(HashState *node)
{





























}

/* ----------------------------------------------------------------
 *		MultiExecPrivateHash
 *
 *		parallel-oblivious version, building a backend-private
 *		hash table and (if necessary) batch files.
 * ----------------------------------------------------------------
 */
static void
MultiExecPrivateHash(HashState *node)
{


































































}

/* ----------------------------------------------------------------
 *		MultiExecParallelHash
 *
 *		parallel-aware version, building a shared hash table and
 *		(if necessary) batch files using the combined effort of
 *		a set of co-operating backends.
 * ----------------------------------------------------------------
 */
static void
MultiExecParallelHash(HashState *node)
{




































































































































}

/* ----------------------------------------------------------------
 *		ExecInitHash
 *
 *		Init routine for Hash node
 * ----------------------------------------------------------------
 */
HashState *
ExecInitHash(Hash *node, EState *estate, int eflags)
{









































}

/* ---------------------------------------------------------------
 *		ExecEndHash
 *
 *		clean up routine for Hash node
 * ----------------------------------------------------------------
 */
void
ExecEndHash(HashState *node)
{












}

/* ----------------------------------------------------------------
 *		ExecHashTableCreate
 *
 *		create an empty hashtable data structure for hashjoin.
 * ----------------------------------------------------------------
 */
HashJoinTable
ExecHashTableCreate(HashState *state, List *hashOperators, List *hashCollations, bool keepNulls)
{














































































































































































































}

/*
 * Compute appropriate size for hashtable given the estimated size of the
 * relation to be hashed (number of rows and average row width).
 *
 * This is exported so that the planner's costsize.c can use it.
 */

/* Target bucket loading (tuples per bucket) */
#define NTUP_PER_BUCKET 1

void
ExecChooseHashTableSize(double ntuples, int tupwidth, bool useskew, bool try_combined_work_mem, int parallel_workers, size_t *space_allowed, int *numbuckets, int *numbatches, int *num_skew_mcvs)
{











































































































































































}

/* ----------------------------------------------------------------
 *		ExecHashTableDestroy
 *
 *		destroy a hash table
 * ----------------------------------------------------------------
 */
void
ExecHashTableDestroy(HashJoinTable hashtable)
{



























}

/*
 * ExecHashIncreaseNumBatches
 *		increase the original number of batches in order to reduce
 *		current memory consumption
 */
static void
ExecHashIncreaseNumBatches(HashJoinTable hashtable)
{






















































































































































}

/*
 * ExecParallelHashIncreaseNumBatches
 *		Every participant attached to grow_batches_barrier must run this
 *		function when it observes growth ==
 *PHJ_GROWTH_NEED_MORE_BATCHES.
 */
static void
ExecParallelHashIncreaseNumBatches(HashJoinTable hashtable)
{















































































































































































































}

/*
 * Repartition the tuples currently loaded into memory for inner batch 0
 * because the number of batches has been increased.  Some tuples are retained
 * in memory and some are written out to a later batch.
 */
static void
ExecParallelHashRepartitionFirst(HashJoinTable hashtable)
{



















































}

/*
 * Help repartition inner batches 1..n.
 */
static void
ExecParallelHashRepartitionRest(HashJoinTable hashtable)
{














































}

/*
 * Transfer the backend-local per-batch counters to the shared totals.
 */
static void
ExecParallelHashMergeCounters(HashJoinTable hashtable)
{




















}

/*
 * ExecHashIncreaseNumBuckets
 *		increase the original number of buckets in order to reduce
 *		number of tuples per bucket
 */
static void
ExecHashIncreaseNumBuckets(HashJoinTable hashtable)
{






















































}

static void
ExecParallelHashIncreaseNumBuckets(HashJoinTable hashtable)
{












































































}

/*
 * ExecHashTableInsert
 *		insert a tuple into the hash table depending on the hash value
 *		it may just go to a temp file for later batches
 *
 * Note: the passed TupleTableSlot may contain a regular, minimal, or virtual
 * tuple; the minimal case in particular is certain to happen while reloading
 * tuples from batch files.  We could save some cycles in the regular-tuple
 * case by not forcing the slot contents into minimal form; not clear if it's
 * worth the messiness required.
 */
void
ExecHashTableInsert(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue)
{













































































}

/*
 * ExecParallelHashTableInsert
 *		insert a tuple into a shared hash table or shared batch
 *tuplestore
 */
void
ExecParallelHashTableInsert(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue)
{





















































}

/*
 * Insert a tuple into the current hash table.  Unlike
 * ExecParallelHashTableInsert, this version is not prepared to send the tuple
 * to other batches or to run out of memory, and should only be called with
 * tuples that belong in the current batch once growth has been disabled.
 */
void
ExecParallelHashTableInsertCurrentBatch(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue)
{



















}

/*
 * ExecHashGetHashValue
 *		Compute the hash value for a tuple
 *
 * The tuple to be tested must be in econtext->ecxt_outertuple (thus Vars in
 * the hashkeys expressions need to have OUTER_VAR as varno). If outer_tuple
 * is false (meaning it's the HashJoin's inner node, Hash), econtext,
 * hashkeys, and slot need to be from Hash, with hashkeys/slot referencing and
 * being suitable for tuples from the node below the Hash. Conversely, if
 * outer_tuple is true, econtext is from HashJoin, and hashkeys/slot need to
 * be appropriate for tuples from HashJoin's outer node.
 *
 * A true result means the tuple's hash value has been successfully computed
 * and stored at *hashvalue.  A false result means the tuple cannot match
 * because it contains a null attribute, and hence it should be discarded
 * immediately.  (If keep_nulls is true then false is never returned.)
 */
bool
ExecHashGetHashValue(HashJoinTable hashtable, ExprContext *econtext, List *hashkeys, bool outer_tuple, bool keep_nulls, uint32 *hashvalue)
{











































































}

/*
 * ExecHashGetBucketAndBatch
 *		Determine the bucket number and batch number for a hash value
 *
 * Note: on-the-fly increases of nbatch must not change the bucket number
 * for a given hash code (since we don't move tuples to different hash
 * chains), and must only cause the batch number to remain the same or
 * increase.  Our algorithm is
 *		bucketno = hashvalue MOD nbuckets
 *		batchno = ROR(hashvalue, log2_nbuckets) MOD nbatch
 * where nbuckets and nbatch are both expected to be powers of 2, so we can
 * do the computations by shifting and masking.  (This assumes that all hash
 * functions are good about randomizing all their output bits, else we are
 * likely to have very skewed bucket or batch occupancy.)
 *
 * nbuckets and log2_nbuckets may change while nbatch == 1 because of dynamic
 * bucket count growth.  Once we start batching, the value is fixed and does
 * not change over the course of the join (making it possible to compute batch
 * number the way we do here).
 *
 * nbatch is always a power of 2; we increase it only by doubling it.  This
 * effectively adds one more bit to the top of the batchno.  In very large
 * joins, we might run out of bits to add, so we do this by rotating the hash
 * value.  This causes batchno to steal bits from bucketno when the number of
 * virtual buckets exceeds 2^32.  It's better to have longer bucket chains
 * than to lose the ability to divide batches.
 */
void
ExecHashGetBucketAndBatch(HashJoinTable hashtable, uint32 hashvalue, int *bucketno, int *batchno)
{













}

/*
 * ExecScanHashBucket
 *		scan a hash bucket for matches to the current outer tuple
 *
 * The current outer tuple must be stored in econtext->ecxt_outertuple.
 *
 * On success, the inner tuple is stored into hjstate->hj_CurTuple and
 * econtext->ecxt_innertuple, using hjstate->hj_HashTupleSlot as the slot
 * for the latter.
 */
bool
ExecScanHashBucket(HashJoinState *hjstate, ExprContext *econtext)
{

















































}

/*
 * ExecParallelScanHashBucket
 *		scan a hash bucket for matches to the current outer tuple
 *
 * The current outer tuple must be stored in econtext->ecxt_outertuple.
 *
 * On success, the inner tuple is stored into hjstate->hj_CurTuple and
 * econtext->ecxt_innertuple, using hjstate->hj_HashTupleSlot as the slot
 * for the latter.
 */
bool
ExecParallelScanHashBucket(HashJoinState *hjstate, ExprContext *econtext)
{










































}

/*
 * ExecPrepHashTableForUnmatched
 *		set up for a series of ExecScanHashTableForUnmatched calls
 */
void
ExecPrepHashTableForUnmatched(HashJoinState *hjstate)
{











}

/*
 * ExecScanHashTableForUnmatched
 *		scan the hash table for unmatched inner tuples
 *
 * On success, the inner tuple is stored into hjstate->hj_CurTuple and
 * econtext->ecxt_innertuple, using hjstate->hj_HashTupleSlot as the slot
 * for the latter.
 */
bool
ExecScanHashTableForUnmatched(HashJoinState *hjstate, ExprContext *econtext)
{































































}

/*
 * ExecHashTableReset
 *
 *		reset hash table header for new batch
 */
void
ExecHashTableReset(HashJoinTable hashtable)
{



















}

/*
 * ExecHashTableResetMatchFlags
 *		Clear all the HeapTupleHeaderHasMatch flags in the table
 */
void
ExecHashTableResetMatchFlags(HashJoinTable hashtable)
{























}

void
ExecReScanHash(HashState *node)
{








}

/*
 * ExecHashBuildSkewHash
 *
 *		Set up for skew optimization if we can identify the most common
 *values (MCVs) of the outer relation's join key.  We make a skew hash bucket
 *		for the hash value of each MCV, up to the number of slots
 *allowed based on available memory.
 */
static void
ExecHashBuildSkewHash(HashJoinTable hashtable, Hash *node, int mcvsToUse)
{



















































































































































}

/*
 * ExecHashGetSkewBucket
 *
 *		Returns the index of the skew bucket for this hashvalue,
 *		or INVALID_SKEW_BUCKET_NO if the hashvalue is not
 *		associated with any active skew bucket.
 */
int
ExecHashGetSkewBucket(HashJoinTable hashtable, uint32 hashvalue)
{






































}

/*
 * ExecHashSkewTableInsert
 *
 *		Insert a tuple into the skew hashtable.
 *
 * This should generally match up with the current-batch case in
 * ExecHashTableInsert.
 */
static void
ExecHashSkewTableInsert(HashJoinTable hashtable, TupleTableSlot *slot, uint32 hashvalue, int bucketNumber)
{







































}

/*
 *		ExecHashRemoveNextSkewBucket
 *
 *		Remove the least valuable skew bucket by pushing its tuples into
 *		the main hash table.
 */
static void
ExecHashRemoveNextSkewBucket(HashJoinTable hashtable)
{








































































































}

/*
 * Reserve space in the DSM segment for instrumentation data.
 */
void
ExecHashEstimate(HashState *node, ParallelContext *pcxt)
{












}

/*
 * Set up a space in the DSM for all workers to record instrumentation data
 * about their hash table.
 */
void
ExecHashInitializeDSM(HashState *node, ParallelContext *pcxt)
{













}

/*
 * Locate the DSM space for hash table instrumentation data that we'll write
 * to at shutdown time.
 */
void
ExecHashInitializeWorker(HashState *node, ParallelWorkerContext *pwcxt)
{










}

/*
 * Copy instrumentation data from this worker's hash table (if it built one)
 * to DSM memory so the leader can retrieve it.  This must be done in an
 * ExecShutdownHash() rather than ExecEndHash() because the latter runs after
 * we've detached from the DSM segment.
 */
void
ExecShutdownHash(HashState *node)
{




}

/*
 * Retrieve instrumentation data from workers before the DSM segment is
 * detached, so that EXPLAIN can access it.
 */
void
ExecHashRetrieveInstrumentation(HashState *node)
{












}

/*
 * Copy the instrumentation data from 'hashtable' into a HashInstrumentation
 * struct.
 */
void
ExecHashGetInstrumentation(HashInstrumentation *instrument, HashJoinTable hashtable)
{





}

/*
 * Allocate 'size' bytes from the currently active HashMemoryChunk
 */
static void *
dense_alloc(HashJoinTable hashtable, Size size)
{





























































}

/*
 * Allocate space for a tuple in shared dense storage.  This is equivalent to
 * dense_alloc but for Parallel Hash using shared memory.
 *
 * While loading a tuple into shared memory, we might run out of memory and
 * decide to repartition, or determine that the load factor is too high and
 * decide to expand the bucket array, or discover that another participant has
 * commanded us to help do that.  Return NULL if number of buckets or batches
 * has changed, indicating that the caller must retry (considering the
 * possibility that the tuple no longer belongs in the same batch).
 */
static HashJoinTuple
ExecParallelHashTupleAlloc(HashJoinTable hashtable, size_t size, dsa_pointer *shared)
{








































































































































}

/*
 * One backend needs to set up the shared batch state including tuplestores.
 * Other backends will ensure they have correctly configured accessors by
 * called ExecParallelHashEnsureBatchAccessors().
 */
static void
ExecParallelHashJoinSetUpBatches(HashJoinTable hashtable, int nbatch)
{





















































}

/*
 * Free the current set of ParallelHashJoinBatchAccessor objects.
 */
static void
ExecParallelHashCloseBatchAccessors(HashJoinTable hashtable)
{












}

/*
 * Make sure this backend has up-to-date accessors for the current set of
 * batches.
 */
static void
ExecParallelHashEnsureBatchAccessors(HashJoinTable hashtable)
{


















































}

/*
 * Allocate an empty shared memory hash table for a given batch.
 */
void
ExecParallelHashTableAlloc(HashJoinTable hashtable, int batchno)
{











}

/*
 * If we are currently attached to a shared hash join batch, detach.  If we
 * are last to detach, clean up.
 */
void
ExecHashTableDetachBatch(HashJoinTable hashtable)
{













































}

/*
 * Detach from all shared resources.  If we are last to detach, clean up.
 */
void
ExecHashTableDetach(HashJoinTable hashtable)
{





























}

/*
 * Get the first tuple in a given bucket identified by number.
 */
static inline HashJoinTuple
ExecParallelHashFirstTuple(HashJoinTable hashtable, int bucketno)
{








}

/*
 * Get the next tuple in the same bucket as 'tuple'.
 */
static inline HashJoinTuple
ExecParallelHashNextTuple(HashJoinTable hashtable, HashJoinTuple tuple)
{






}

/*
 * Insert a tuple at the front of a chain of tuples in DSA memory atomically.
 */
static inline void
ExecParallelHashPushTuple(dsa_pointer_atomic *head, HashJoinTuple tuple, dsa_pointer tuple_shared)
{








}

/*
 * Prepare to work on a given batch.
 */
void
ExecParallelHashTableSetCurrentBatch(HashJoinTable hashtable, int batchno)
{









}

/*
 * Take the next available chunk from the queue of chunks being worked on in
 * parallel.  Return NULL if there are none left.  Otherwise return a pointer
 * to the chunk, and set *shared to the DSA pointer to the chunk.
 */
static HashMemoryChunk
ExecParallelHashPopChunkQueue(HashJoinTable hashtable, dsa_pointer *shared)
{

















}

/*
 * Increase the space preallocated in this backend for a given inner batch by
 * at least a given amount.  This allows us to track whether a given batch
 * would fit in memory when loaded back in.  Also increase the number of
 * batches or buckets if required.
 *
 * This maintains a running estimation of how much space will be taken when we
 * load the batch back into memory by simulating the way chunks will be handed
 * out to workers.  It's not perfectly accurate because the tuples will be
 * packed into memory chunks differently by ExecParallelHashTupleAlloc(), but
 * it should be pretty close.  It tends to overestimate by a fraction of a
 * chunk per worker since all workers gang up to preallocate during hashing,
 * but workers tend to reload batches alone if there are enough to go around,
 * leaving fewer partially filled chunks.  This effect is bounded by
 * nparticipants.
 *
 * Return false if the number of batches or buckets has changed, and the
 * caller should reconsider which batch a given tuple now belongs in and call
 * again.
 */
static bool
ExecParallelHashTuplePrealloc(HashJoinTable hashtable, int batchno, size_t size)
{















































}