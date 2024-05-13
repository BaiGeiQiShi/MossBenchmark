/*-------------------------------------------------------------------------
 *
 * heapam_handler.c
 *	  heap table access method code
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/heap/heapam_handler.c
 *
 *
 * NOTES
 *	  This files wires up the lower level heapam.c et al routines with the
 *	  tableam abstraction.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "miscadmin.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/rewriteheap.h"
#include "access/tableam.h"
#include "access/tsmapi.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "commands/progress.h"
#include "executor/executor.h"
#include "optimizer/plancat.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/rel.h"

static void
reform_and_rewrite_tuple(HeapTuple tuple, Relation OldHeap, Relation NewHeap, Datum *values, bool *isnull, RewriteState rwstate);

static bool
SampleHeapTupleVisible(TableScanDesc scan, Buffer buffer, HeapTuple tuple, OffsetNumber tupoffset);

static BlockNumber
heapam_scan_get_blocks_done(HeapScanDesc hscan);

static const TableAmRoutine heapam_methods;

/* ------------------------------------------------------------------------
 * Slot related callbacks for heap AM
 * ------------------------------------------------------------------------
 */

static const TupleTableSlotOps *
heapam_slot_callbacks(Relation relation)
{

}

/* ------------------------------------------------------------------------
 * Index Scan Callbacks for heap AM
 * ------------------------------------------------------------------------
 */

static IndexFetchTableData *
heapam_index_fetch_begin(Relation rel)
{






}

static void
heapam_index_fetch_reset(IndexFetchTableData *scan)
{







}

static void
heapam_index_fetch_end(IndexFetchTableData *scan)
{





}

static bool
heapam_index_fetch_tuple(struct IndexFetchTableData *scan, ItemPointer tid, Snapshot snapshot, TupleTableSlot *slot, bool *call_again, bool *all_dead)
{















































}

/* ------------------------------------------------------------------------
 * Callbacks for non-modifying operations on individual tuples for heap AM
 * ------------------------------------------------------------------------
 */

static bool
heapam_fetch_row_version(Relation relation, ItemPointer tid, Snapshot snapshot, TupleTableSlot *slot)
{
















}

static bool
heapam_tuple_tid_valid(TableScanDesc scan, ItemPointer tid)
{



}

static bool
heapam_tuple_satisfies_snapshot(Relation rel, TupleTableSlot *slot, Snapshot snapshot)
{















}

/* ----------------------------------------------------------------------------
 *  Functions for manipulations of physical tuples for heap AM.
 * ----------------------------------------------------------------------------
 */

static void
heapam_tuple_insert(Relation relation, TupleTableSlot *slot, CommandId cid, int options, BulkInsertState bistate)
{















}

static void
heapam_tuple_insert_speculative(Relation relation, TupleTableSlot *slot, CommandId cid, int options, BulkInsertState bistate, uint32 specToken)
{


















}

static void
heapam_tuple_complete_speculative(Relation relation, TupleTableSlot *slot, uint32 specToken, bool succeeded)
{

















}

static TM_Result
heapam_tuple_delete(Relation relation, ItemPointer tid, CommandId cid, Snapshot snapshot, Snapshot crosscheck, bool wait, TM_FailureData *tmfd, bool changingPart)
{






}

static TM_Result
heapam_tuple_update(Relation relation, ItemPointer otid, TupleTableSlot *slot, CommandId cid, Snapshot snapshot, Snapshot crosscheck, bool wait, TM_FailureData *tmfd, LockTupleMode *lockmode, bool *update_indexes)
{



























}

static TM_Result
heapam_tuple_lock(Relation relation, ItemPointer tid, Snapshot snapshot, TupleTableSlot *slot, CommandId cid, LockTupleMode mode, LockWaitPolicy wait_policy, uint8 flags, TM_FailureData *tmfd)
{




































































































































































































}

static void
heapam_finish_bulk_insert(Relation relation, int options)
{








}

/* ------------------------------------------------------------------------
 * DDL related callbacks for heap AM.
 * ------------------------------------------------------------------------
 */

static void
heapam_relation_set_new_filenode(Relation rel, const RelFileNode *newrnode, char persistence, TransactionId *freezeXid, MultiXactId *minmulti)
{







































}

static void
heapam_relation_nontransactional_truncate(Relation rel)
{

}

static void
heapam_relation_copy_data(Relation rel, const RelFileNode *newrnode)
{














































}

static void
heapam_relation_copy_for_cluster(Relation OldHeap, Relation NewHeap, Relation OldIndex, bool use_sort, TransactionId OldestXmin, TransactionId *xid_cutoff, MultiXactId *multi_cutoff, double *num_tuples, double *tups_vacuumed, double *tups_recently_dead)
{






































































































































































































































































































}

static bool
heapam_scan_analyze_next_block(TableScanDesc scan, BlockNumber blockno, BufferAccessStrategy bstrategy)
{


















}

static bool
heapam_scan_analyze_next_tuple(TableScanDesc scan, TransactionId OldestXmin, double *liverows, double *deadrows, TupleTableSlot *slot)
{











































































































































}

static double
heapam_index_build_range_scan(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo, bool allow_sync, bool anyvisible, bool progress, BlockNumber start_blockno, BlockNumber numblocks, IndexBuildCallback callback, void *callback_state, TableScanDesc scan)
{
















































































































































































































































































































































































































































































































































































}

static void
heapam_index_validate_scan(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo, Snapshot snapshot, ValidateIndexState *state)
{
























































































































































































































}

/*
 * Return the number of blocks that have been read by this scan since
 * starting.  This is meant for progress reporting rather than be fully
 * accurate: in a parallel scan, workers can be concurrently reading blocks
 * further ahead than what we report.
 */
static BlockNumber
heapam_scan_get_blocks_done(HeapScanDesc hscan)
{































}

/* ------------------------------------------------------------------------
 * Miscellaneous callbacks for the heap AM
 * ------------------------------------------------------------------------
 */

static uint64
heapam_relation_size(Relation rel, ForkNumber forkNumber)
{
























}

/*
 * Check to see whether the table needs a TOAST table.  It does only if
 * (1) there are any toastable attributes, and (2) the maximum length
 * of a tuple could exceed TOAST_TUPLE_THRESHOLD.  (We don't want to
 * create a toast table for something like "f1 varchar(20)".)
 */
static bool
heapam_relation_needs_toast_table(Relation rel)
{

















































}

/* ------------------------------------------------------------------------
 * Planner related callbacks for the heap AM
 * ------------------------------------------------------------------------
 */

static void
heapam_estimate_rel_size(Relation rel, int32 *attr_widths, BlockNumber *pages, double *tuples, double *allvisfrac)
{







































































































}

/* ------------------------------------------------------------------------
 * Executor related callbacks for the heap AM
 * ------------------------------------------------------------------------
 */

static bool
heapam_scan_bitmap_next_block(TableScanDesc scan, TBMIterateResult *tbmres)
{












































































































}

static bool
heapam_scan_bitmap_next_tuple(TableScanDesc scan, TBMIterateResult *tbmres, TupleTableSlot *slot)
{


































}

static bool
heapam_scan_sample_next_block(TableScanDesc scan, SampleScanState *scanstate)
{











































































}

static bool
heapam_scan_sample_next_tuple(TableScanDesc scan, SampleScanState *scanstate, TupleTableSlot *slot)
{



































































































}

/* ----------------------------------------------------------------------------
 *  Helper functions for the above.
 * ----------------------------------------------------------------------------
 */

/*
 * Reconstruct and rewrite the given tuple
 *
 * We cannot simply copy the tuple as-is, for several reasons:
 *
 * 1. We'd like to squeeze out the values of any dropped columns, both
 * to save space and to ensure we have no corner-case failures. (It's
 * possible for example that the new table hasn't got a TOAST table
 * and so is unable to store any large values of dropped cols.)
 *
 * 2. The tuple might not even be legal for the new table; this is
 * currently only known to happen as an after-effect of ALTER TABLE
 * SET WITHOUT OIDS.
 *
 * So, we must reconstruct the tuple from component Datums.
 */
static void
reform_and_rewrite_tuple(HeapTuple tuple, Relation OldHeap, Relation NewHeap, Datum *values, bool *isnull, RewriteState rwstate)
{






















}

/*
 * Check visibility of the tuple.
 */
static bool
SampleHeapTupleVisible(TableScanDesc scan, Buffer buffer, HeapTuple tuple, OffsetNumber tupoffset)
{









































}

/* ------------------------------------------------------------------------
 * Definition of the heap table access method.
 * ------------------------------------------------------------------------
 */

static const TableAmRoutine heapam_methods = {.type = T_TableAmRoutine,

    .slot_callbacks = heapam_slot_callbacks,

    .scan_begin = heap_beginscan,
    .scan_end = heap_endscan,
    .scan_rescan = heap_rescan,
    .scan_getnextslot = heap_getnextslot,

    .parallelscan_estimate = table_block_parallelscan_estimate,
    .parallelscan_initialize = table_block_parallelscan_initialize,
    .parallelscan_reinitialize = table_block_parallelscan_reinitialize,

    .index_fetch_begin = heapam_index_fetch_begin,
    .index_fetch_reset = heapam_index_fetch_reset,
    .index_fetch_end = heapam_index_fetch_end,
    .index_fetch_tuple = heapam_index_fetch_tuple,

    .tuple_insert = heapam_tuple_insert,
    .tuple_insert_speculative = heapam_tuple_insert_speculative,
    .tuple_complete_speculative = heapam_tuple_complete_speculative,
    .multi_insert = heap_multi_insert,
    .tuple_delete = heapam_tuple_delete,
    .tuple_update = heapam_tuple_update,
    .tuple_lock = heapam_tuple_lock,
    .finish_bulk_insert = heapam_finish_bulk_insert,

    .tuple_fetch_row_version = heapam_fetch_row_version,
    .tuple_get_latest_tid = heap_get_latest_tid,
    .tuple_tid_valid = heapam_tuple_tid_valid,
    .tuple_satisfies_snapshot = heapam_tuple_satisfies_snapshot,
    .compute_xid_horizon_for_tuples = heap_compute_xid_horizon_for_tuples,

    .relation_set_new_filenode = heapam_relation_set_new_filenode,
    .relation_nontransactional_truncate = heapam_relation_nontransactional_truncate,
    .relation_copy_data = heapam_relation_copy_data,
    .relation_copy_for_cluster = heapam_relation_copy_for_cluster,
    .relation_vacuum = heap_vacuum_rel,
    .scan_analyze_next_block = heapam_scan_analyze_next_block,
    .scan_analyze_next_tuple = heapam_scan_analyze_next_tuple,
    .index_build_range_scan = heapam_index_build_range_scan,
    .index_validate_scan = heapam_index_validate_scan,

    .relation_size = heapam_relation_size,
    .relation_needs_toast_table = heapam_relation_needs_toast_table,

    .relation_estimate_size = heapam_estimate_rel_size,

    .scan_bitmap_next_block = heapam_scan_bitmap_next_block,
    .scan_bitmap_next_tuple = heapam_scan_bitmap_next_tuple,
    .scan_sample_next_block = heapam_scan_sample_next_block,
    .scan_sample_next_tuple = heapam_scan_sample_next_tuple};

const TableAmRoutine *
GetHeapamTableAmRoutine(void)
{

}

Datum
heap_tableam_handler(PG_FUNCTION_ARGS)
{

}