/*-------------------------------------------------------------------------
 *
 * hash.c
 *	  Implementation of Margo Seltzer's Hashing package for postgres.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hash.c
 *
 * NOTES
 *	  This file contains only the public interface routines.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
#include "access/hash_xlog.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "catalog/index.h"
#include "commands/progress.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "optimizer/plancat.h"
#include "pgstat.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/rel.h"
#include "miscadmin.h"

/* Working state for hashbuild and its callback */
typedef struct
{
  HSpool *spool;    /* NULL if not using spooling */
  double indtuples; /* # tuples accepted into index */
  Relation heapRel; /* heap relation descriptor */
} HashBuildState;

static void
hashbuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state);

/*
 * Hash handler function: return IndexAmRoutine with access method parameters
 * and callbacks.
 */
Datum
hashhandler(PG_FUNCTION_ARGS)
{










































}

/*
 *	hashbuild() -- build a new hash index.
 */
IndexBuildResult *
hashbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{




















































































}

/*
 *	hashbuildempty() -- build an empty hash index in the initialization fork
 */
void
hashbuildempty(Relation index)
{

}

/*
 * Per-tuple callback for table_index_build_scan
 */
static void
hashbuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{


























}

/*
 *	hashinsert() -- insert an index tuple into a hash table.
 *
 *	Hash on the heap tuple's key, form an index tuple with hash code.
 *	Find the appropriate location for the new tuple, and put it there.
 */
bool
hashinsert(Relation rel, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{



















}

/*
 *	hashgettuple() -- Get the next tuple in the scan.
 */
bool
hashgettuple(IndexScanDesc scan, ScanDirection dir)
{
















































}

/*
 *	hashgetbitmap() -- get all tuples at once
 */
int64
hashgetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{























}

/*
 *	hashbeginscan() -- start a scan on a hash index
 */
IndexScanDesc
hashbeginscan(Relation rel, int nkeys, int norderbys)
{






















}

/*
 *	hashrescan() -- rescan an index relation
 */
void
hashrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{

























}

/*
 *	hashendscan() -- close down a scan
 */
void
hashendscan(IndexScanDesc scan)
{




















}

/*
 * Bulk deletion of all index entries pointing to a set of heap tuples.
 * The set of target tuples is specified via a callback routine that tells
 * whether any given heap tuple (identified by ItemPointer) is being deleted.
 *
 * This function also deletes the tuples that are moved by split to other
 * bucket.
 *
 * Result: a palloc'd struct containing statistical info for VACUUM displays.
 */
IndexBulkDeleteResult *
hashbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
















































































































































































}

/*
 * Post-VACUUM cleanup.
 *
 * Result: a palloc'd struct containing statistical info for VACUUM displays.
 */
IndexBulkDeleteResult *
hashvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{















}

/*
 * Helper function to perform deletion of index entries from a bucket.
 *
 * This function expects that the caller has acquired a cleanup lock on the
 * primary bucket page, and will return with a write lock again held on the
 * primary bucket page.  The lock won't necessarily be held continuously,
 * though, because we'll release it when visiting overflow pages.
 *
 * There can't be any concurrent scans in progress when we first enter this
 * function because of the cleanup lock we hold on the primary bucket page,
 * but as soon as we release that lock, there might be.  If those scans got
 * ahead of our cleanup scan, they might see a tuple before we kill it and
 * wake up only after VACUUM has completed and the TID has been recycled for
 * an unrelated tuple.  To avoid that calamity, we prevent scans from passing
 * our cleanup scan by locking the next page in the bucket chain before
 * releasing the lock on the previous page.  (This type of lock chaining is not
 * ideal, so we might want to look for a better solution at some point.)
 *
 * We need to retain a pin on the primary bucket to ensure that no concurrent
 * split can start.
 */
void
hashbucketcleanup(Relation rel, Bucket cur_bucket, Buffer bucket_buf, BlockNumber bucket_blkno, BufferAccessStrategy bstrategy, uint32 maxbucket, uint32 highmask, uint32 lowmask, double *tuples_removed, double *num_index_tuples, bool split_cleanup, IndexBulkDeleteCallback callback, void *callback_state)
{














































































































































































































































}