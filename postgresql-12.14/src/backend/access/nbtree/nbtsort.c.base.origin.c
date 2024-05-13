/*-------------------------------------------------------------------------
 *
 * nbtsort.c
 *		Build a btree from sorted input by loading leaf pages
 *sequentially.
 *
 * NOTES
 *
 * We use tuplesort.c to sort the given index tuples into order.
 * Then we scan the index tuples in order and build the btree pages
 * for each level.  We load source tuples into leaf-level pages.
 * Whenever we fill a page at one level, we add a link to it to its
 * parent level (starting a new parent level if necessary).  When
 * done, we write out each final page on each level, adding it to
 * its parent level.  When we have only one page on a level, it must be
 * the root -- it can be attached to the btree metapage and we are done.
 *
 * It is not wise to pack the pages entirely full, since then *any*
 * insertion would cause a split (and not only of the leaf page; the need
 * for a split would cascade right up the tree).  The steady-state load
 * factor for btrees is usually estimated at 70%.  We choose to pack leaf
 * pages to the user-controllable fill factor (default 90%) while upper pages
 * are always packed to 70%.  This gives us reasonable density (there aren't
 * many upper pages if the keys are reasonable-size) without risking a lot of
 * cascading splits during early insertions.
 *
 * Formerly the index pages being built were kept in shared buffers, but
 * that is of no value (since other backends have no interest in them yet)
 * and it created locking problems for CHECKPOINT, because the upper-level
 * pages were held exclusive-locked for long periods.  Now we just build
 * the pages in local memory and smgrwrite or smgrextend them as we finish
 * them.  They will need to be re-read into shared buffers on first use after
 * the build finishes.
 *
 * Since the index will never be used unless it is completely built,
 * from a crash-recovery point of view there is no need to WAL-log the
 * steps of the build.  After completing the index build, we can just sync
 * the whole file to disk using smgrimmedsync() before exiting this module.
 * This can be seen to be sufficient for crash recovery by considering that
 * it's effectively equivalent to what would happen if a CHECKPOINT occurred
 * just after the index build.  However, it is clearly not sufficient if the
 * DBA is using the WAL log for PITR or replication purposes, since another
 * machine would not be able to reconstruct the index from WAL.  Therefore,
 * we log the completed index pages to WAL if and only if WAL archiving is
 * active.
 *
 * This code isn't concerned about the FSM at all. The caller is responsible
 * for initializing that.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/nbtree/nbtsort.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/nbtree.h"
#include "access/parallel.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/index.h"
#include "commands/progress.h"
#include "executor/instrument.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h" /* pgrminclude ignore */
#include "utils/rel.h"
#include "utils/sortsupport.h"
#include "utils/tuplesort.h"

/* Magic numbers for parallel state sharing */
#define PARALLEL_KEY_BTREE_SHARED UINT64CONST(0xA000000000000001)
#define PARALLEL_KEY_TUPLESORT UINT64CONST(0xA000000000000002)
#define PARALLEL_KEY_TUPLESORT_SPOOL2 UINT64CONST(0xA000000000000003)
#define PARALLEL_KEY_QUERY_TEXT UINT64CONST(0xA000000000000004)
#define PARALLEL_KEY_BUFFER_USAGE UINT64CONST(0xA000000000000005)

/*
 * DISABLE_LEADER_PARTICIPATION disables the leader's participation in
 * parallel index builds.  This may be useful as a debugging aid.
#undef DISABLE_LEADER_PARTICIPATION
 */

/*
 * Status record for spooling/sorting phase.  (Note we may have two of
 * these due to the special requirements for uniqueness-checking with
 * dead tuples.)
 */
typedef struct BTSpool
{
  Tuplesortstate *sortstate; /* state data for tuplesort.c */
  Relation heap;
  Relation index;
  bool isunique;
} BTSpool;

/*
 * Status for index builds performed in parallel.  This is allocated in a
 * dynamic shared memory segment.  Note that there is a separate tuplesort TOC
 * entry, private to tuplesort.c but allocated by this module on its behalf.
 */
typedef struct BTShared
{
  /*
   * These fields are not modified during the sort.  They primarily exist
   * for the benefit of worker processes that need to create BTSpool state
   * corresponding to that used by the leader.
   */
  Oid heaprelid;
  Oid indexrelid;
  bool isunique;
  bool isconcurrent;
  int scantuplesortstates;

  /*
   * workersdonecv is used to monitor the progress of workers.  All parallel
   * participants must indicate that they are done before leader can use
   * mutable state that workers maintain during scan (and before leader can
   * proceed to tuplesort_performsort()).
   */
  ConditionVariable workersdonecv;

  /*
   * mutex protects all fields before heapdesc.
   *
   * These fields contain status information of interest to B-Tree index
   * builds that must work just the same when an index is built in parallel.
   */
  slock_t mutex;

  /*
   * Mutable state that is maintained by workers, and reported back to
   * leader at end of parallel scan.
   *
   * nparticipantsdone is number of worker processes finished.
   *
   * reltuples is the total number of input heap tuples.
   *
   * havedead indicates if RECENTLY_DEAD tuples were encountered during
   * build.
   *
   * indtuples is the total number of tuples that made it into the index.
   *
   * brokenhotchain indicates if any worker detected a broken HOT chain
   * during build.
   */
  int nparticipantsdone;
  double reltuples;
  bool havedead;
  double indtuples;
  bool brokenhotchain;

  /*
   * ParallelTableScanDescData data follows. Can't directly embed here, as
   * implementations of the parallel table scan desc interface might need
   * stronger alignment.
   */
} BTShared;

/*
 * Return pointer to a BTShared's parallel table scan.
 *
 * c.f. shm_toc_allocate as to why BUFFERALIGN is used, rather than just
 * MAXALIGN.
 */
#define ParallelTableScanFromBTShared(shared) (ParallelTableScanDesc)((char *)(shared) + BUFFERALIGN(sizeof(BTShared)))

/*
 * Status for leader in parallel index build.
 */
typedef struct BTLeader
{
  /* parallel context itself */
  ParallelContext *pcxt;

  /*
   * nparticipanttuplesorts is the exact number of worker processes
   * successfully launched, plus one leader process if it participates as a
   * worker (only DISABLE_LEADER_PARTICIPATION builds avoid leader
   * participating as a worker).
   */
  int nparticipanttuplesorts;

  /*
   * Leader process convenience pointers to shared state (leader avoids TOC
   * lookups).
   *
   * btshared is the shared state for entire build.  sharedsort is the
   * shared, tuplesort-managed state passed to each process tuplesort.
   * sharedsort2 is the corresponding btspool2 shared state, used only when
   * building unique indexes.  snapshot is the snapshot used by the scan iff
   * an MVCC snapshot is required.
   */
  BTShared *btshared;
  Sharedsort *sharedsort;
  Sharedsort *sharedsort2;
  Snapshot snapshot;
  BufferUsage *bufferusage;
} BTLeader;

/*
 * Working state for btbuild and its callback.
 *
 * When parallel CREATE INDEX is used, there is a BTBuildState for each
 * participant.
 */
typedef struct BTBuildState
{
  bool isunique;
  bool havedead;
  Relation heap;
  BTSpool *spool;

  /*
   * spool2 is needed only when the index is a unique index. Dead tuples are
   * put into spool2 instead of spool in order to avoid uniqueness check.
   */
  BTSpool *spool2;
  double indtuples;

  /*
   * btleader is only present when a parallel index build is performed, and
   * only in the leader process. (Actually, only the leader has a
   * BTBuildState.  Workers have their own spool and spool2, though.)
   */
  BTLeader *btleader;
} BTBuildState;

/*
 * Status record for a btree page being built.  We have one of these
 * for each active tree level.
 *
 * The reason we need to store a copy of the minimum key is that we'll
 * need to propagate it to the parent node when this page is linked
 * into its parent.  However, if the page is not a leaf page, the first
 * entry on the page doesn't need to contain a key, so we will not have
 * stored the key itself on the page.  (You might think we could skip
 * copying the minimum key on leaf pages, but actually we must have a
 * writable copy anyway because we'll poke the page's address into it
 * before passing it up to the parent...)
 */
typedef struct BTPageState
{
  Page btps_page;                /* workspace for page building */
  BlockNumber btps_blkno;        /* block # to write this page at */
  IndexTuple btps_minkey;        /* copy of minimum key (first item) on page */
  OffsetNumber btps_lastoff;     /* last item offset loaded */
  uint32 btps_level;             /* tree level (0 = leaf) */
  Size btps_full;                /* "full" if less than this much free space */
  struct BTPageState *btps_next; /* link to parent level, if any */
} BTPageState;

/*
 * Overall status record for index writing phase.
 */
typedef struct BTWriteState
{
  Relation heap;
  Relation index;
  BTScanInsert inskey;            /* generic insertion scankey */
  bool btws_use_wal;              /* dump pages to WAL? */
  BlockNumber btws_pages_alloced; /* # pages allocated */
  BlockNumber btws_pages_written; /* # pages written out */
  Page btws_zeropage;             /* workspace for filling zeroes */
} BTWriteState;

static double
_bt_spools_heapscan(Relation heap, Relation index, BTBuildState *buildstate, IndexInfo *indexInfo);
static void
_bt_spooldestroy(BTSpool *btspool);
static void
_bt_spool(BTSpool *btspool, ItemPointer self, Datum *values, bool *isnull);
static void
_bt_leafbuild(BTSpool *btspool, BTSpool *btspool2);
static void
_bt_build_callback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state);
static Page
_bt_blnewpage(uint32 level);
static BTPageState *
_bt_pagestate(BTWriteState *wstate, uint32 level);
static void
_bt_slideleft(Page page);
static void
_bt_sortaddtup(Page page, Size itemsize, IndexTuple itup, OffsetNumber itup_off);
static void
_bt_buildadd(BTWriteState *wstate, BTPageState *state, IndexTuple itup);
static void
_bt_uppershutdown(BTWriteState *wstate, BTPageState *state);
static void
_bt_load(BTWriteState *wstate, BTSpool *btspool, BTSpool *btspool2);
static void
_bt_begin_parallel(BTBuildState *buildstate, bool isconcurrent, int request);
static void
_bt_end_parallel(BTLeader *btleader);
static Size
_bt_parallel_estimate_shared(Relation heap, Snapshot snapshot);
static double
_bt_parallel_heapscan(BTBuildState *buildstate, bool *brokenhotchain);
static void
_bt_leader_participate_as_worker(BTBuildState *buildstate);
static void
_bt_parallel_scan_and_sort(BTSpool *btspool, BTSpool *btspool2, BTShared *btshared, Sharedsort *sharedsort, Sharedsort *sharedsort2, int sortmem, bool progress);

/*
 *	btbuild() -- build a new btree index.
 */
IndexBuildResult *
btbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{




























































}

/*
 * Create and initialize one or two spool structures, and save them in caller's
 * buildstate argument.  May also fill-in fields within indexInfo used by index
 * builds.
 *
 * Scans the heap, possibly in parallel, filling spools with IndexTuples.  This
 * routine encapsulates all aspects of managing parallelism.  Caller need only
 * call _bt_end_parallel() in parallel case after it is done with spool/spool2.
 *
 * Returns the total number of heap tuples scanned.
 */
static double
_bt_spools_heapscan(Relation heap, Relation index, BTBuildState *buildstate, IndexInfo *indexInfo)
{
































































































































}

/*
 * clean up a spool structure and its substructures.
 */
static void
_bt_spooldestroy(BTSpool *btspool)
{


}

/*
 * spool an index entry into the sort file.
 */
static void
_bt_spool(BTSpool *btspool, ItemPointer self, Datum *values, bool *isnull)
{

}

/*
 * given a spool loaded by successive calls to _bt_spool,
 * create an entire btree.
 */
static void
_bt_leafbuild(BTSpool *btspool, BTSpool *btspool2)
{




































}

/*
 * Per-tuple callback for table_index_build_scan
 */
static void
_bt_build_callback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{


















}

/*
 * allocate workspace for a new, clean btree page, not linked to any siblings.
 */
static Page
_bt_blnewpage(uint32 level)
{



















}

/*
 * emit a completed btree page, and release the working storage.
 */
static void
_bt_blwritepage(BTWriteState *wstate, Page page, BlockNumber blkno)
{











































}

/*
 * allocate and initialize a new BTPageState.  the returned structure
 * is suitable for immediate use by _bt_buildadd.
 */
static BTPageState *
_bt_pagestate(BTWriteState *wstate, uint32 level)
{

























}

/*
 * slide an array of ItemIds back one slot (from P_FIRSTKEY to
 * P_HIKEY, overwriting P_HIKEY).  we need to do this when we discover
 * that we have built an ItemId array in what has turned out to be a
 * P_RIGHTMOST page.
 */
static void
_bt_slideleft(Page page)
{

















}

/*
 * Add an item to a page being built.
 *
 * The main difference between this routine and a bare PageAddItem call
 * is that this code knows that the leftmost data item on a non-leaf
 * btree page doesn't need to have a key.  Therefore, it strips such
 * items down to just the item header.
 *
 * This is almost like nbtinsert.c's _bt_pgaddtup(), but we can't use
 * that because it assumes that P_RIGHTMOST() will return the correct
 * answer for the page.  Here, we don't know yet if the page will be
 * rightmost.  Offset P_FIRSTKEY is always the first data key.
 */
static void
_bt_sortaddtup(Page page, Size itemsize, IndexTuple itup, OffsetNumber itup_off)
{

















}

/*----------
 * Add an item to a disk page from the sort output.
 *
 * We must be careful to observe the page layout conventions of nbtsearch.c:
 * - rightmost pages start data items at P_HIKEY instead of at P_FIRSTKEY.
 * - on non-leaf pages, the key portion of the first item need not be
 *	 stored, we should store only the link.
 *
 * A leaf page being built looks like:
 *
 * +----------------+---------------------------------+
 * | PageHeaderData | linp0 linp1 linp2 ...           |
 * +-----------+----+---------------------------------+
 * | ... linpN |
 *|
 * +-----------+--------------------------------------+
 * |	 ^ last | |
 *|
 * +-------------+------------------------------------+
 * |			 | itemN ...                          |
 * +-------------+------------------+-----------------+
 * |		  ... item3 item2 item1 | "special space" |
 * +--------------------------------+-----------------+
 *
 * Contrast this with the diagram in bufpage.h; note the mismatch
 * between linps and items.  This is because we reserve linp0 as a
 * placeholder for the pointer to the "high key" item; when we have
 * filled up the page, we will set linp0 to point to itemN and clear
 * linpN.  On the other hand, if we find this is the last (rightmost)
 * page, we leave the items alone and slide the linp array over.  If
 * the high key is to be truncated, offset 1 is deleted, and we insert
 * the truncated high key at offset 1.
 *
 * 'last' pointer indicates the last offset added to the page.
 *----------
 */
static void
_bt_buildadd(BTWriteState *wstate, BTPageState *state, IndexTuple itup)
{

































































































































































































































}

/*
 * Finish writing out the completed btree.
 */
static void
_bt_uppershutdown(BTWriteState *wstate, BTPageState *state)
{


























































}

/*
 * Read tuples in correct sort order from tuplesort, and load them into
 * btree leaves.
 */
static void
_bt_load(BTWriteState *wstate, BTSpool *btspool, BTSpool *btspool2)
{


































































































































































}

/*
 * Create parallel context, and launch workers for leader.
 *
 * buildstate argument should be initialized (with the exception of the
 * tuplesort state in spools, which may later be created based on shared
 * state initially set up here).
 *
 * isconcurrent indicates if operation is CREATE INDEX CONCURRENTLY.
 *
 * request is the target number of parallel worker processes to launch.
 *
 * Sets buildstate's BTLeader, which caller must use to shut down parallel
 * mode by passing it to _bt_end_parallel() at the very end of its index
 * build.  If not even a single worker process can be launched, this is
 * never set, and caller should proceed with a serial index build.
 */
static void
_bt_begin_parallel(BTBuildState *buildstate, bool isconcurrent, int request)
{







































































































































































































}

/*
 * Shut down workers, destroy parallel context, and end parallel mode.
 */
static void
_bt_end_parallel(BTLeader *btleader)
{





















}

/*
 * Returns size of shared memory required to store state for a parallel
 * btree index build based on the snapshot its parallel scan will use.
 */
static Size
_bt_parallel_estimate_shared(Relation heap, Snapshot snapshot)
{


}

/*
 * Within leader, wait for end of heap scan.
 *
 * When called, parallel heap scan started by _bt_begin_parallel() will
 * already be underway within worker processes (when leader participates
 * as a worker, we should end up here just as workers are finishing).
 *
 * Fills in fields needed for ambuild statistics, and lets caller set
 * field indicating that some worker encountered a broken HOT chain.
 *
 * Returns the total number of heap tuples scanned.
 */
static double
_bt_parallel_heapscan(BTBuildState *buildstate, bool *brokenhotchain)
{

























}

/*
 * Within leader, participate as a parallel worker.
 */
static void
_bt_leader_participate_as_worker(BTBuildState *buildstate)
{












































}

/*
 * Perform work within a launched parallel process.
 */
void
_bt_parallel_build_main(dsm_segment *seg, shm_toc *toc)
{































































































}

/*
 * Perform a worker's portion of a parallel sort.
 *
 * This generates a tuplesort for passed btspool, and a second tuplesort
 * state if a second btspool is need (i.e. for unique index builds).  All
 * other spool fields should already be set when this is called.
 *
 * sortmem is the amount of working memory to use within each worker,
 * expressed in KBs.
 *
 * When this returns, workers are done, and need only release resources.
 */
static void
_bt_parallel_scan_and_sort(BTSpool *btspool, BTSpool *btspool2, BTShared *btshared, Sharedsort *sharedsort, Sharedsort *sharedsort2, int sortmem, bool progress)
{





























































































}