/*-------------------------------------------------------------------------
 *
 * nodeGatherMerge.c
 *		Scan a plan in multiple workers, and do order-preserving merge.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/executor/nodeGatherMerge.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/relscan.h"
#include "access/xact.h"
#include "executor/execdebug.h"
#include "executor/execParallel.h"
#include "executor/nodeGatherMerge.h"
#include "executor/nodeSubplan.h"
#include "executor/tqueue.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * When we read tuples from workers, it's a good idea to read several at once
 * for efficiency when possible: this minimizes context-switching overhead.
 * But reading too many at a time wastes memory without improving performance.
 * We'll read up to MAX_TUPLE_STORE tuples (in addition to the first one).
 */
#define MAX_TUPLE_STORE 10

/*
 * Pending-tuple array for each worker.  This holds additional tuples that
 * we were able to fetch from the worker, but can't process yet.  In addition,
 * this struct holds the "done" flag indicating the worker is known to have
 * no more tuples.  (We do not use this struct for the leader; we don't keep
 * any pending tuples for the leader, and the need_to_scan_locally flag serves
 * as its "done" indicator.)
 */
typedef struct GMReaderTupleBuffer
{
  HeapTuple *tuple; /* array of length MAX_TUPLE_STORE */
  int nTuples;      /* number of tuples currently stored */
  int readCounter;  /* index of next tuple to extract */
  bool done;        /* true if reader is known exhausted */
} GMReaderTupleBuffer;

static TupleTableSlot *
ExecGatherMerge(PlanState *pstate);
static int32
heap_compare_slots(Datum a, Datum b, void *arg);
static TupleTableSlot *
gather_merge_getnext(GatherMergeState *gm_state);
static HeapTuple
gm_readnext_tuple(GatherMergeState *gm_state, int nreader, bool nowait, bool *done);
static void
ExecShutdownGatherMergeWorkers(GatherMergeState *node);
static void
gather_merge_setup(GatherMergeState *gm_state);
static void
gather_merge_init(GatherMergeState *gm_state);
static void
gather_merge_clear_tuples(GatherMergeState *gm_state);
static bool
gather_merge_readnext(GatherMergeState *gm_state, int reader, bool nowait);
static void
load_tuple_array(GatherMergeState *gm_state, int reader);

/* ----------------------------------------------------------------
 *		ExecInitGather
 * ----------------------------------------------------------------
 */
GatherMergeState *
ExecInitGatherMerge(GatherMerge *node, EState *estate, int eflags)
{







































































































}

/* ----------------------------------------------------------------
 *		ExecGatherMerge(node)
 *
 *		Scans the relation via multiple workers and returns
 *		the next qualifying tuple.
 * ----------------------------------------------------------------
 */
static TupleTableSlot *
ExecGatherMerge(PlanState *pstate)
{




























































































}

/* ----------------------------------------------------------------
 *		ExecEndGatherMerge
 *
 *		frees any storage allocated through C routines.
 * ----------------------------------------------------------------
 */
void
ExecEndGatherMerge(GatherMergeState *node)
{







}

/* ----------------------------------------------------------------
 *		ExecShutdownGatherMerge
 *
 *		Destroy the setup for parallel workers including parallel
 *context.
 * ----------------------------------------------------------------
 */
void
ExecShutdownGatherMerge(GatherMergeState *node)
{








}

/* ----------------------------------------------------------------
 *		ExecShutdownGatherMergeWorkers
 *
 *		Stop all the parallel workers.
 * ----------------------------------------------------------------
 */
static void
ExecShutdownGatherMergeWorkers(GatherMergeState *node)
{











}

/* ----------------------------------------------------------------
 *		ExecReScanGatherMerge
 *
 *		Prepare to re-scan the result of a GatherMerge.
 * ----------------------------------------------------------------
 */
void
ExecReScanGatherMerge(GatherMergeState *node)
{







































}

/*
 * Set up the data structures that we'll need for Gather Merge.
 *
 * We allocate these once on the basis of gm->num_workers, which is an
 * upper bound for the number of workers we'll actually have.  During
 * a rescan, we reset the structures to empty.  This approach simplifies
 * not leaking memory across rescans.
 *
 * In the gm_slots[] array, index 0 is for the leader, and indexes 1 to n
 * are for workers.  The values placed into gm_heap correspond to indexes
 * in gm_slots[].  The gm_tuple_buffers[] array, however, is indexed from
 * 0 to n-1; it has no entry for the leader.
 */
static void
gather_merge_setup(GatherMergeState *gm_state)
{





























}

/*
 * Initialize the Gather Merge.
 *
 * Reset data structures to ensure they're empty.  Then pull at least one
 * tuple from leader + each worker (or set its "done" indicator), and set up
 * the heap.
 */
static void
gather_merge_init(GatherMergeState *gm_state)
{









































































}

/*
 * Clear out the tuple table slot, and any unused pending tuples,
 * for each gather merge input.
 */
static void
gather_merge_clear_tuples(GatherMergeState *gm_state)
{













}

/*
 * Read the next tuple for gather merge.
 *
 * Fetch the sorted tuple out of the heap.
 */
static TupleTableSlot *
gather_merge_getnext(GatherMergeState *gm_state)
{











































}

/*
 * Read tuple(s) for given reader in nowait mode, and load into its tuple
 * array, until we have MAX_TUPLE_STORE of them or would have to block.
 */
static void
load_tuple_array(GatherMergeState *gm_state, int reader)
{






























}

/*
 * Store the next tuple for a given reader into the appropriate slot.
 *
 * Returns true if successful, false if not (either reader is exhausted,
 * or we didn't want to wait for a tuple).  Sets done flag if reader
 * is found to be exhausted.
 */
static bool
gather_merge_readnext(GatherMergeState *gm_state, int reader, bool nowait)
{





































































}

/*
 * Attempt to read a tuple from given worker.
 */
static HeapTuple
gm_readnext_tuple(GatherMergeState *gm_state, int nreader, bool nowait, bool *done)
{


















}

/*
 * We have one slot for each item in the heap array.  We use SlotNumber
 * to store slot indexes.  This doesn't actually provide any formal
 * type-safety, but it makes the code more self-documenting.
 */
typedef int32 SlotNumber;

/*
 * Compare the tuples in the two given slots.
 */
static int32
heap_compare_slots(Datum a, Datum b, void *arg)
{






























}