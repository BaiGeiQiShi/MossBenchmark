/*----------------------------------------------------------------------
 *
 * tableam.c
 *		Table access method routines too big to be inline functions.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/table/tableam.c
 *
 * NOTES
 *	  Note that most function in here are documented in tableam.h, rather
 *than here. That's because there's a lot of inline functions in tableam.h and
 *	  it'd be harder to understand if one constantly had to switch between
 *files.
 *
 *----------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h" /* for ss_* */
#include "access/tableam.h"
#include "access/xact.h"
#include "storage/bufmgr.h"
#include "storage/shmem.h"

/* GUC variables */
char *default_table_access_method = DEFAULT_TABLE_ACCESS_METHOD;
bool synchronize_seqscans = true;

/* ----------------------------------------------------------------------------
 * Slot functions.
 * ----------------------------------------------------------------------------
 */

const TupleTableSlotOps *
table_slot_callbacks(Relation relation)
{





























}

TupleTableSlot *
table_slot_create(Relation relation, List **reglist)
{












}

/* ----------------------------------------------------------------------------
 * Table scan functions.
 * ----------------------------------------------------------------------------
 */

TableScanDesc
table_beginscan_catalog(Relation relation, int nkeys, struct ScanKeyData *key)
{





}

void
table_scan_update_snapshot(TableScanDesc scan, Snapshot snapshot)
{





}

/* ----------------------------------------------------------------------------
 * Parallel table scan related functions.
 * ----------------------------------------------------------------------------
 */

Size
table_parallelscan_estimate(Relation rel, Snapshot snapshot)
{














}

void
table_parallelscan_initialize(Relation rel, ParallelTableScanDesc pscan, Snapshot snapshot)
{














}

TableScanDesc
table_beginscan_parallel(Relation relation, ParallelTableScanDesc parallel_scan)
{



















}

/* ----------------------------------------------------------------------------
 * Index scan related functions.
 * ----------------------------------------------------------------------------
 */

/*
 * To perform that check simply start an index scan, create the necessary
 * slot, do the heap lookup, and shut everything down again. This could be
 * optimized, but is unlikely to matter from a performance POV. If there
 * frequently are live index pointers also matching a unique index key, the
 * CPU overhead of this routine is unlikely to matter.
 */
bool
table_index_fetch_tuple_check(Relation rel, ItemPointer tid, Snapshot snapshot, bool *all_dead)
{












}

/* ------------------------------------------------------------------------
 * Functions for non-modifying operations on individual tuples
 * ------------------------------------------------------------------------
 */

void
table_tuple_get_latest_tid(TableScanDesc scan, ItemPointer tid)
{













}

/* ----------------------------------------------------------------------------
 * Functions to make modifications a bit simpler.
 * ----------------------------------------------------------------------------
 */

/*
 * simple_table_tuple_insert - insert a tuple
 *
 * Currently, this routine differs from table_tuple_insert only in supplying a
 * default command ID and not allowing access to the speedup options.
 */
void
simple_table_tuple_insert(Relation rel, TupleTableSlot *slot)
{

}

/*
 * simple_table_tuple_delete - delete a tuple
 *
 * This routine may be used to delete a tuple when concurrent updates of
 * the target tuple are not expected (for example, because we have a lock
 * on the relation associated with the tuple).  Any failure is reported
 * via ereport().
 */
void
simple_table_tuple_delete(Relation rel, ItemPointer tid, Snapshot snapshot)
{




























}

/*
 * simple_table_tuple_update - replace a tuple
 *
 * This routine may be used to update a tuple when concurrent updates of
 * the target tuple are not expected (for example, because we have a lock
 * on the relation associated with the tuple).  Any failure is reported
 * via ereport().
 */
void
simple_table_tuple_update(Relation rel, ItemPointer otid, TupleTableSlot *slot, Snapshot snapshot, bool *update_indexes)
{





























}

/* ----------------------------------------------------------------------------
 * Helper functions to implement parallel scans for block oriented AMs.
 * ----------------------------------------------------------------------------
 */

Size
table_block_parallelscan_estimate(Relation rel)
{

}

Size
table_block_parallelscan_initialize(Relation rel, ParallelTableScanDesc pscan)
{











}

void
table_block_parallelscan_reinitialize(Relation rel, ParallelTableScanDesc pscan)
{



}

/*
 * find and set the scan's startblock
 *
 * Determine where the parallel seq scan should start.  This function may be
 * called many times, once by each parallel worker.  We must be careful only
 * to set the startblock once.
 */
void
table_block_parallelscan_startblock_init(Relation rel, ParallelBlockTableScanDesc pbscan)
{


































}

/*
 * get the next page to scan
 *
 * Get the next page to scan.  Even if there are no pages left to scan,
 * another backend could have grabbed a page to scan and not yet finished
 * looking at it, so it doesn't follow that the scan is done when the first
 * backend gets an InvalidBlockNumber return.
 */
BlockNumber
table_block_parallelscan_nextpage(Relation rel, ParallelBlockTableScanDesc pbscan)
{
















































}