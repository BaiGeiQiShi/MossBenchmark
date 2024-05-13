/*-------------------------------------------------------------------------
 *
 * pruneheap.c
 *	  heap page pruning and HOT-chain management code
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/heap/pruneheap.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/htup_details.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "utils/snapmgr.h"
#include "utils/rel.h"

/* Working data for heap_page_prune and subroutines */
typedef struct
{
  TransactionId new_prune_xid;    /* new prune hint value for page */
  TransactionId latestRemovedXid; /* latest xid to be removed by this prune */
  int nredirected;                /* numbers of entries in arrays below */
  int ndead;
  int nunused;
  /* arrays that accumulate indexes of items to be changed */
  OffsetNumber redirected[MaxHeapTuplesPerPage * 2];
  OffsetNumber nowdead[MaxHeapTuplesPerPage];
  OffsetNumber nowunused[MaxHeapTuplesPerPage];
  /* marked[i] is true if item i is entered in one of the above arrays */
  bool marked[MaxHeapTuplesPerPage + 1];
} PruneState;

/* Local functions */
static int
heap_prune_chain(Relation relation, Buffer buffer, OffsetNumber rootoffnum, TransactionId OldestXmin, PruneState *prstate);
static void
heap_prune_record_prunable(PruneState *prstate, TransactionId xid);
static void
heap_prune_record_redirect(PruneState *prstate, OffsetNumber offnum, OffsetNumber rdoffnum);
static void
heap_prune_record_dead(PruneState *prstate, OffsetNumber offnum);
static void
heap_prune_record_unused(PruneState *prstate, OffsetNumber offnum);

/*
 * Optionally prune and repair fragmentation in the specified page.
 *
 * This is an opportunistic function.  It will perform housekeeping
 * only if the page heuristically looks like a candidate for pruning and we
 * can acquire buffer cleanup lock without blocking.
 *
 * Note: this is called quite often.  It's important that it fall out quickly
 * if there's not any use in pruning.
 *
 * Caller must have pin on the buffer, and must *not* have a lock on it.
 *
 * OldestXmin is the cutoff XID used to distinguish whether tuples are DEAD
 * or RECENTLY_DEAD (see HeapTupleSatisfiesVacuum).
 */
void
heap_page_prune_opt(Relation relation, Buffer buffer)
{



























































































}

/*
 * Prune and repair fragmentation in the specified page.
 *
 * Caller must have pin and buffer cleanup lock on the page.
 *
 * OldestXmin is the cutoff XID used to distinguish whether tuples are DEAD
 * or RECENTLY_DEAD (see HeapTupleSatisfiesVacuum).
 *
 * If report_stats is true then we send the number of reclaimed heap-only
 * tuples to pgstats.  (This must be false during vacuum, since vacuum will
 * send its own new total to pgstats, and we don't want this delta applied
 * on top of that.)
 *
 * Returns the number of tuples deleted from the page and sets
 * latestRemovedXid.
 */
int
heap_page_prune(Relation relation, Buffer buffer, TransactionId OldestXmin, bool report_stats, TransactionId *latestRemovedXid)
{





































































































































}

/*
 * Prune specified line pointer or a HOT chain originating at line pointer.
 *
 * If the item is an index-referenced tuple (i.e. not a heap-only tuple),
 * the HOT chain is pruned by removing all DEAD tuples at the start of the HOT
 * chain.  We also prune any RECENTLY_DEAD tuples preceding a DEAD tuple.
 * This is OK because a RECENTLY_DEAD tuple preceding a DEAD tuple is really
 * DEAD, the OldestXmin test is just too coarse to detect it.
 *
 * The root line pointer is redirected to the tuple immediately after the
 * latest DEAD tuple.  If all tuples in the chain are DEAD, the root line
 * pointer is marked LP_DEAD.  (This includes the case of a DEAD simple
 * tuple, which we treat as a chain of length 1.)
 *
 * OldestXmin is the cutoff XID used to identify dead tuples.
 *
 * We don't actually change the page here, except perhaps for hint-bit updates
 * caused by HeapTupleSatisfiesVacuum.  We just add entries to the arrays in
 * prstate showing the changes to be made.  Items to be redirected are added
 * to the redirected[] array (two entries per redirection); items to be set to
 * LP_DEAD state are added to nowdead[]; and items to be set to LP_UNUSED
 * state are added to nowunused[].
 *
 * Returns the number of tuples (to be) deleted from the page.
 */
static int
heap_prune_chain(Relation relation, Buffer buffer, OffsetNumber rootoffnum, TransactionId OldestXmin, PruneState *prstate)
{
















































































































































































































































































}

/* Record lowest soon-prunable XID */
static void
heap_prune_record_prunable(PruneState *prstate, TransactionId xid)
{









}

/* Record line pointer to be redirected */
static void
heap_prune_record_redirect(PruneState *prstate, OffsetNumber offnum, OffsetNumber rdoffnum)
{








}

/* Record line pointer to be marked dead */
static void
heap_prune_record_dead(PruneState *prstate, OffsetNumber offnum)
{





}

/* Record line pointer to be marked unused */
static void
heap_prune_record_unused(PruneState *prstate, OffsetNumber offnum)
{





}

/*
 * Perform the actual page changes needed by heap_page_prune.
 * It is expected that the caller has suitable pin and lock on the
 * buffer, and is inside a critical section.
 *
 * This is split out because it is also used by heap_xlog_clean()
 * to replay the WAL record when needed after a crash.  Note that the
 * arguments are identical to those of log_heap_clean().
 */
void
heap_page_prune_execute(Buffer buffer, OffsetNumber *redirected, int nredirected, OffsetNumber *nowdead, int ndead, OffsetNumber *nowunused, int nunused)
{








































}

/*
 * For all items in this page, find their respective root line pointers.
 * If item k is part of a HOT-chain with root at item j, then we set
 * root_offsets[k - 1] = j.
 *
 * The passed-in root_offsets array must have MaxHeapTuplesPerPage entries.
 * Unused entries are filled with InvalidOffsetNumber (zero).
 *
 * The function must be called with at least share lock on the buffer, to
 * prevent concurrent prune operations.
 *
 * Note: The information collected here is valid only as long as the caller
 * holds a pin on the buffer. Once pin is released, a tuple might be pruned
 * and reused by a completely unrelated tuple.
 */
void
heap_get_root_tuples(Page page, OffsetNumber *root_offsets)
{


































































































}