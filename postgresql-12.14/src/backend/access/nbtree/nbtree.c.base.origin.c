/*-------------------------------------------------------------------------
 *
 * nbtree.c
 *	  Implementation of Lehman and Yao's btree management algorithm for
 *	  Postgres.
 *
 * NOTES
 *	  This file contains only the public interface routines.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtree.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/nbtree.h"
#include "access/nbtxlog.h"
#include "access/relscan.h"
#include "access/xlog.h"
#include "commands/progress.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "nodes/execnodes.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/condition_variable.h"
#include "storage/indexfsm.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/memutils.h"

/* Working state needed by btvacuumpage */
typedef struct
{
  IndexVacuumInfo *info;
  IndexBulkDeleteResult *stats;
  IndexBulkDeleteCallback callback;
  void *callback_state;
  BTCycleId cycleid;
  BlockNumber lastBlockVacuumed; /* highest blkno actually vacuumed */
  BlockNumber lastBlockLocked;   /* highest blkno we've cleanup-locked */
  BlockNumber totFreePages;      /* true total # of free pages */
  TransactionId oldestBtpoXact;
  MemoryContext pagedelcontext;
} BTVacState;

/*
 * BTPARALLEL_NOT_INITIALIZED indicates that the scan has not started.
 *
 * BTPARALLEL_ADVANCING indicates that some process is advancing the scan to
 * a new page; others must wait.
 *
 * BTPARALLEL_IDLE indicates that no backend is currently advancing the scan
 * to a new page; some process can start doing that.
 *
 * BTPARALLEL_DONE indicates that the scan is complete (including error exit).
 * We reach this state once for every distinct combination of array keys.
 */
typedef enum
{
  BTPARALLEL_NOT_INITIALIZED,
  BTPARALLEL_ADVANCING,
  BTPARALLEL_IDLE,
  BTPARALLEL_DONE
} BTPS_State;

/*
 * BTParallelScanDescData contains btree specific shared information required
 * for parallel scan.
 */
typedef struct BTParallelScanDescData
{
  BlockNumber btps_scanPage;  /* latest or next page to be scanned */
  BTPS_State btps_pageStatus; /* indicates whether next page is
                               * available for scan. see above for
                               * possible states of parallel scan. */
  int btps_arrayKeyCount;     /* count indicating number of array scan
                               * keys processed by parallel scan */
  slock_t btps_mutex;         /* protects above variables */
  ConditionVariable btps_cv;  /* used to synchronize parallel scan */
} BTParallelScanDescData;

typedef struct BTParallelScanDescData *BTParallelScanDesc;

static void
btvacuumscan(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state, BTCycleId cycleid);
static void
btvacuumpage(BTVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno);

/*
 * Btree handler function: return IndexAmRoutine with access method parameters
 * and callbacks.
 */
Datum
bthandler(PG_FUNCTION_ARGS)
{










































}

/*
 *	btbuildempty() -- build an empty btree index in the initialization fork
 */
void
btbuildempty(Relation index)
{























}

/*
 *	btinsert() -- insert an index tuple into a btree.
 *
 *		Descend the tree recursively, find the appropriate location for
 *our new tuple, and put it there.
 */
bool
btinsert(Relation rel, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{












}

/*
 *	btgettuple() -- Get the next tuple in the scan.
 */
bool
btgettuple(IndexScanDesc scan, ScanDirection dir)
{











































































}

/*
 * btgetbitmap() -- gets all matching tuples, and adds them to a bitmap
 */
int64
btgetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{






















































}

/*
 *	btbeginscan() -- start a scan on a btree index
 */
IndexScanDesc
btbeginscan(Relation rel, int nkeys, int norderbys)
{










































}

/*
 *	btrescan() -- rescan an index relation
 */
void
btrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{





















































}

/*
 *	btendscan() -- close down a scan
 */
void
btendscan(IndexScanDesc scan)
{






































}

/*
 *	btmarkpos() -- save current scan position
 */
void
btmarkpos(IndexScanDesc scan)
{


























}

/*
 *	btrestrpos() -- restore scan to last saved position
 */
void
btrestrpos(IndexScanDesc scan)
{























































}

/*
 * btestimateparallelscan -- estimate storage for BTParallelScanDescData
 */
Size
btestimateparallelscan(void)
{

}

/*
 * btinitparallelscan -- initialize BTParallelScanDesc for parallel btree scan
 */
void
btinitparallelscan(void *target)
{







}

/*
 *	btparallelrescan() -- reset parallel scan
 */
void
btparallelrescan(IndexScanDesc scan)
{

















}

/*
 * _bt_parallel_seize() -- Begin the process of advancing the scan to a new
 *		page.  Other scans must wait until we call
 *_bt_parallel_release() or _bt_parallel_done().
 *
 * The return value is true if we successfully seized the scan and false
 * if we did not.  The latter case occurs if no pages remain for the current
 * set of scankeys.
 *
 * If the return value is true, *pageno returns the next or current page
 * of the scan (depending on the scan direction).  An invalid block number
 * means the scan hasn't yet started, and P_NONE means we've reached the end.
 * The first time a participating process reaches the last page, it will return
 * true and set *pageno to P_NONE; after that, further attempts to seize the
 * scan will return false.
 *
 * Callers should ignore the value of pageno if the return value is false.
 */
bool
_bt_parallel_seize(IndexScanDesc scan, BlockNumber *pageno)
{

















































}

/*
 * _bt_parallel_release() -- Complete the process of advancing the scan to a
 *		new page.  We now have the new value btps_scanPage; some other
 *backend can now begin advancing the scan.
 */
void
_bt_parallel_release(IndexScanDesc scan, BlockNumber scan_page)
{










}

/*
 * _bt_parallel_done() -- Mark the parallel scan as complete.
 *
 * When there are no pages left to scan, this function should be called to
 * notify other workers.  Otherwise, they might wait forever for the scan to
 * advance to the next page.
 */
void
_bt_parallel_done(IndexScanDesc scan)
{































}

/*
 * _bt_parallel_advance_array_keys() -- Advances the parallel scan for array
 *			keys.
 *
 * Updates the count of array keys processed for both local and parallel
 * scans.
 */
void
_bt_parallel_advance_array_keys(IndexScanDesc scan)
{















}

/*
 * _bt_vacuum_needs_cleanup() -- Checks if index needs cleanup
 *
 * Called by btvacuumcleanup when btbulkdelete was never called because no
 * tuples need to be deleted.
 *
 * When we return false, VACUUM can even skip the cleanup-only call to
 * btvacuumscan (i.e. there will be no btvacuumscan call for this index at
 * all).  Otherwise, a cleanup-only btvacuumscan call is required.
 */
static bool
_bt_vacuum_needs_cleanup(IndexVacuumInfo *info)
{



















































}

/*
 * Bulk deletion of all index entries pointing to a set of heap tuples.
 * The set of target tuples is specified via a callback routine that tells
 * whether any given heap tuple (identified by ItemPointer) is being deleted.
 *
 * Result: a palloc'd struct containing statistical info for VACUUM displays.
 */
IndexBulkDeleteResult *
btbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{





















}

/*
 * Post-VACUUM cleanup.
 *
 * Result: a palloc'd struct containing statistical info for VACUUM displays.
 */
IndexBulkDeleteResult *
btvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{











































}

/*
 * btvacuumscan --- scan the index for VACUUMing purposes
 *
 * This combines the functions of looking for leaf tuples that are deletable
 * according to the vacuum callback, looking for empty pages that can be
 * deleted, and looking for old deleted pages that can be recycled.  Both
 * btbulkdelete and btvacuumcleanup invoke this (the latter only if no
 * btbulkdelete call occurred and _bt_vacuum_needs_cleanup returned true).
 * Note that this is also where the metadata used by _bt_vacuum_needs_cleanup
 * is maintained.
 *
 * The caller is responsible for initially allocating/zeroing a stats struct
 * and for obtaining a vacuum cycle ID if necessary.
 */
static void
btvacuumscan(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state, BTCycleId cycleid)
{



























































































































































}

/*
 * btvacuumpage --- VACUUM one page
 *
 * This processes a single page for btvacuumscan().  In some cases we
 * must go back and re-examine previously-scanned pages; this routine
 * recurses when necessary to handle that case.
 *
 * blkno is the page to process.  orig_blkno is the highest block number
 * reached by the outer btvacuumscan loop (the same as blkno, unless we
 * are recursing to re-examine a previous page).
 */
static void
btvacuumpage(BTVacState *vstate, BlockNumber blkno, BlockNumber orig_blkno)
{







































































































































































































































































}

/*
 *	btcanreturn() -- Check whether btree indexes support index-only scans.
 *
 * btrees always do, so this is trivial.
 */
bool
btcanreturn(Relation index, int attno)
{

}