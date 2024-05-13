/*
 * brin.c
 *		Implementation of BRIN indexes for Postgres
 *
 * See src/backend/access/brin/README for details.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin.c
 *
 * TODO
 *		* ScalarArrayOpExpr (amsearcharray -> SK_SEARCHARRAY)
 */
#include "postgres.h"

#include "access/brin.h"
#include "access/brin_page.h"
#include "access/brin_pageops.h"
#include "access/brin_xlog.h"
#include "access/relation.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/xloginsert.h"
#include "catalog/index.h"
#include "catalog/pg_am.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/memutils.h"
#include "utils/rel.h"

/*
 * We use a BrinBuildState during initial construction of a BRIN index.
 * The running state is kept in a BrinMemTuple.
 */
typedef struct BrinBuildState
{
  Relation bs_irel;
  int bs_numtuples;
  Buffer bs_currentInsertBuf;
  BlockNumber bs_pagesPerRange;
  BlockNumber bs_currRangeStart;
  BrinRevmap *bs_rmAccess;
  BrinDesc *bs_bdesc;
  BrinMemTuple *bs_dtuple;
} BrinBuildState;

/*
 * Struct used as "opaque" during index scans
 */
typedef struct BrinOpaque
{
  BlockNumber bo_pagesPerRange;
  BrinRevmap *bo_rmAccess;
  BrinDesc *bo_bdesc;
} BrinOpaque;

#define BRIN_ALL_BLOCKRANGES InvalidBlockNumber

static BrinBuildState *
initialize_brin_buildstate(Relation idxRel, BrinRevmap *revmap, BlockNumber pagesPerRange);
static void
terminate_brin_buildstate(BrinBuildState *state);
static void
brinsummarize(Relation index, Relation heapRel, BlockNumber pageRange, bool include_partial, double *numSummarized, double *numExisting);
static void
form_and_insert_tuple(BrinBuildState *state);
static void
union_tuples(BrinDesc *bdesc, BrinMemTuple *a, BrinTuple *b);
static void
brin_vacuum_scan(Relation idxrel, BufferAccessStrategy strategy);

/*
 * BRIN handler function: return IndexAmRoutine with access method parameters
 * and callbacks.
 */
Datum
brinhandler(PG_FUNCTION_ARGS)
{










































}

/*
 * A tuple in the heap is being inserted.  To keep a brin index up to date,
 * we need to obtain the relevant index tuple and compare its stored values
 * with those of the new tuple.  If the tuple values are not consistent with
 * the summary tuple, we need to update the index tuple.
 *
 * If autosummarization is enabled, check if we need to summarize the previous
 * page range.
 *
 * If the range is not currently summarized (i.e. the revmap returns NULL for
 * it), there's nothing to do for this tuple.
 */
bool
brininsert(Relation idxRel, Datum *values, bool *nulls, ItemPointer heaptid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{








































































































































































}

/*
 * Initialize state for a BRIN index scan.
 *
 * We read the metapage here to determine the pages-per-range number that this
 * index was built with.  Note that since this cannot be changed while we're
 * holding lock on index, it's not necessary to recompute it during brinrescan.
 */
IndexScanDesc
brinbeginscan(Relation r, int nkeys, int norderbys)
{











}

/*
 * Execute the index scan.
 *
 * This works by reading index TIDs from the revmap, and obtaining the index
 * tuples pointed to by them; the summary values in the index tuples are
 * compared to the scan keys.  We return into the TID bitmap all the pages in
 * ranges corresponding to index tuples that match the scan keys.
 *
 * If a TID from the revmap is read as InvalidTID, we know that range is
 * unsummarized.  Pages in those ranges need to be returned regardless of scan
 * keys.
 */
int64
bringetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{















































































































































































}

/*
 * Re-initialize state for a BRIN index scan
 */
void
brinrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{












}

/*
 * Close down a BRIN index scan
 */
void
brinendscan(IndexScanDesc scan)
{





}

/*
 * Per-heap-tuple callback for table_index_build_scan.
 *
 * Note we don't worry about the page range at the end of the table here; it is
 * present in the build state struct after we're called the last time, but not
 * inserted into the index.  Caller must ensure to do so, if appropriate.
 */
static void
brinbuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *brstate)
{










































}

/*
 * brinbuild() -- build a new BRIN index.
 */
IndexBuildResult *
brinbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{














































































}

void
brinbuildempty(Relation index)
{














}

/*
 * brinbulkdelete
 *		Since there are no per-heap-tuple index tuples in BRIN indexes,
 *		there's not a lot we can do here.
 *
 * XXX we could mark item tuples as "dirty" (when a minimum or maximum heap
 * tuple is deleted), meaning the need to re-run summarization on the affected
 * range.  Would need to add an extra flag in brintuples for that.
 */
IndexBulkDeleteResult *
brinbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{







}

/*
 * This routine is in charge of "vacuuming" a BRIN index: we just summarize
 * ranges that are currently unsummarized.
 */
IndexBulkDeleteResult *
brinvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
























}

/*
 * reloptions processor for BRIN indexes
 */
bytea *
brinoptions(Datum reloptions, bool validate)
{




















}

/*
 * SQL-callable function to scan through an index and summarize all ranges
 * that are not currently summarized.
 */
Datum
brin_summarize_new_values(PG_FUNCTION_ARGS)
{



}

/*
 * SQL-callable function to summarize the indicated page range, if not already
 * summarized.  If the second argument is BRIN_ALL_BLOCKRANGES, all
 * unsummarized ranges are summarized.
 */
Datum
brin_summarize_range(PG_FUNCTION_ARGS)
{





























































































}

/*
 * SQL-callable interface to mark a range as no longer summarized
 */
Datum
brin_desummarize_range(PG_FUNCTION_ARGS)
{










































































}

/*
 * Build a BrinDesc used to create or scan a BRIN index
 */
BrinDesc *
brin_build_desc(Relation rel)
{
















































}

void
brin_free_desc(BrinDesc *bdesc)
{




}

/*
 * Fetch index's statistical data into *stats
 */
void
brinGetStats(Relation index, BrinStatsData *stats)
{













}

/*
 * Initialize a BrinBuildState appropriate to create tuples on the given index.
 */
static BrinBuildState *
initialize_brin_buildstate(Relation idxRel, BrinRevmap *revmap, BlockNumber pagesPerRange)
{
















}

/*
 * Release resources associated with a BrinBuildState.
 */
static void
terminate_brin_buildstate(BrinBuildState *state)
{





















}

/*
 * On the given BRIN index, summarize the heap page range that corresponds
 * to the heap block number given.
 *
 * This routine can run in parallel with insertions into the heap.  To avoid
 * missing those values from the summary tuple, we first insert a placeholder
 * index tuple into the index, then execute the heap scan; transactions
 * concurrent with the scan update the placeholder tuple.  After the scan, we
 * union the placeholder tuple with the one computed by this routine.  The
 * update of the index value happens in a loop, so that if somebody updates
 * the placeholder tuple after we read it, we detect the case and try again.
 * This ensures that the concurrently inserted tuples are not lost.
 *
 * A further corner case is this routine being asked to summarize the partial
 * range at the end of the table.  heapNumBlocks is the (possibly outdated)
 * table size; if we notice that the requested range lies beyond that size,
 * we re-compute the table size after inserting the placeholder tuple, to
 * avoid missing pages that were appended recently.
 */
static void
summarize_range(IndexInfo *indexInfo, BrinBuildState *state, Relation heapRel, BlockNumber heapBlk, BlockNumber heapNumBlks)
{







































































































}

/*
 * Summarize page ranges that are not already summarized.  If pageRange is
 * BRIN_ALL_BLOCKRANGES then the whole table is scanned; otherwise, only the
 * page range containing the given heap page number is scanned.
 * If include_partial is true, then the partial range at the end of the table
 * is summarized, otherwise not.
 *
 * For each new index tuple inserted, *numSummarized (if not NULL) is
 * incremented; for each existing tuple, *numExisting (if not NULL) is
 * incremented.
 */
static void
brinsummarize(Relation index, Relation heapRel, BlockNumber pageRange, bool include_partial, double *numSummarized, double *numExisting)
{






























































































}

/*
 * Given a deformed tuple in the build state, convert it into the on-disk
 * format and insert it into the index, making the revmap point to it.
 */
static void
form_and_insert_tuple(BrinBuildState *state)
{








}

/*
 * Given two deformed tuples, adjust the first one so that it's consistent
 * with the summary values in both.
 */
static void
union_tuples(BrinDesc *bdesc, BrinMemTuple *a, BrinTuple *b)
{






















}

/*
 * brin_vacuum_scan
 *		Do a complete scan of the index during VACUUM.
 *
 * This routine scans the complete index looking for uncatalogued index pages,
 * i.e. those that might have been lost due to a crash after index extension
 * and such.
 */
static void
brin_vacuum_scan(Relation idxrel, BufferAccessStrategy strategy)
{



























}