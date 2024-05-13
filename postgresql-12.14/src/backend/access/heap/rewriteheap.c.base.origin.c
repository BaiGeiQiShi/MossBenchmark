/*-------------------------------------------------------------------------
 *
 * rewriteheap.c
 *	  Support functions to rewrite tables.
 *
 * These functions provide a facility to completely rewrite a heap, while
 * preserving visibility information and update chains.
 *
 * INTERFACE
 *
 * The caller is responsible for creating the new heap, all catalog
 * changes, supplying the tuples to be written to the new heap, and
 * rebuilding indexes.  The caller must hold AccessExclusiveLock on the
 * target table, because we assume no one else is writing into it.
 *
 * To use the facility:
 *
 * begin_heap_rewrite
 * while (fetch next tuple)
 * {
 *	   if (tuple is dead)
 *		   rewrite_heap_dead_tuple
 *	   else
 *	   {
 *		   // do any transformations here if required
 *		   rewrite_heap_tuple
 *	   }
 * }
 * end_heap_rewrite
 *
 * The contents of the new relation shouldn't be relied on until after
 * end_heap_rewrite is called.
 *
 *
 * IMPLEMENTATION
 *
 * This would be a fairly trivial affair, except that we need to maintain
 * the ctid chains that link versions of an updated tuple together.
 * Since the newly stored tuples will have tids different from the original
 * ones, if we just copied t_ctid fields to the new table the links would
 * be wrong.  When we are required to copy a (presumably recently-dead or
 * delete-in-progress) tuple whose ctid doesn't point to itself, we have
 * to substitute the correct ctid instead.
 *
 * For each ctid reference from A -> B, we might encounter either A first
 * or B first.  (Note that a tuple in the middle of a chain is both A and B
 * of different pairs.)
 *
 * If we encounter A first, we'll store the tuple in the unresolved_tups
 * hash table. When we later encounter B, we remove A from the hash table,
 * fix the ctid to point to the new location of B, and insert both A and B
 * to the new heap.
 *
 * If we encounter B first, we can insert B to the new heap right away.
 * We then add an entry to the old_new_tid_map hash table showing B's
 * original tid (in the old heap) and new tid (in the new heap).
 * When we later encounter A, we get the new location of B from the table,
 * and can write A immediately with the correct ctid.
 *
 * Entries in the hash tables can be removed as soon as the later tuple
 * is encountered.  That helps to keep the memory usage down.  At the end,
 * both tables are usually empty; we should have encountered both A and B
 * of each pair.  However, it's possible for A to be RECENTLY_DEAD and B
 * entirely DEAD according to HeapTupleSatisfiesVacuum, because the test
 * for deadness using OldestXmin is not exact.  In such a case we might
 * encounter B first, and skip it, and find A later.  Then A would be added
 * to unresolved_tups, and stay there until end of the rewrite.  Since
 * this case is very unusual, we don't worry about the memory usage.
 *
 * Using in-memory hash tables means that we use some memory for each live
 * update chain in the table, from the time we find one end of the
 * reference until we find the other end.  That shouldn't be a problem in
 * practice, but if you do something like an UPDATE without a where-clause
 * on a large table, and then run CLUSTER in the same transaction, you
 * could run out of memory.  It doesn't seem worthwhile to add support for
 * spill-to-disk, as there shouldn't be that many RECENTLY_DEAD tuples in a
 * table under normal circumstances.  Furthermore, in the typical scenario
 * of CLUSTERing on an unchanging key column, we'll see all the versions
 * of a given tuple together anyway, and so the peak memory usage is only
 * proportional to the number of RECENTLY_DEAD versions of a single row, not
 * in the whole table.  Note that if we do fail halfway through a CLUSTER,
 * the old table is still valid, so failure is not catastrophic.
 *
 * We can't use the normal heap_insert function to insert into the new
 * heap, because heap_insert overwrites the visibility information.
 * We use a special-purpose raw_heap_insert function instead, which
 * is optimized for bulk inserting a lot of tuples, knowing that we have
 * exclusive access to the heap.  raw_heap_insert builds new pages in
 * local storage.  When a page is full, or at the end of the process,
 * we insert it to WAL as a single record and then write it to disk
 * directly through smgr.  Note, however, that any data sent to the new
 * heap's TOAST table will go through the normal bufmgr.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994-5, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/heap/rewriteheap.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "miscadmin.h"

#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/rewriteheap.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "access/xloginsert.h"

#include "catalog/catalog.h"

#include "lib/ilist.h"

#include "pgstat.h"

#include "replication/logical.h"
#include "replication/slot.h"

#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/smgr.h"

#include "utils/memutils.h"
#include "utils/rel.h"

#include "storage/procarray.h"

/*
 * State associated with a rewrite operation. This is opaque to the user
 * of the rewrite facility.
 */
typedef struct RewriteStateData
{
  Relation rs_old_rel;            /* source heap */
  Relation rs_new_rel;            /* destination heap */
  Page rs_buffer;                 /* page currently being built */
  BlockNumber rs_blockno;         /* block where page will go */
  bool rs_buffer_valid;           /* T if any tuples in buffer */
  bool rs_use_wal;                /* must we WAL-log inserts? */
  bool rs_logical_rewrite;        /* do we need to do logical rewriting */
  TransactionId rs_oldest_xmin;   /* oldest xmin used by caller to determine
                                   * tuple visibility */
  TransactionId rs_freeze_xid;    /* Xid that will be used as freeze cutoff
                                   * point */
  TransactionId rs_logical_xmin;  /* Xid that will be used as cutoff point
                                   * for logical rewrites */
  MultiXactId rs_cutoff_multi;    /* MultiXactId that will be used as cutoff
                                   * point for multixacts */
  MemoryContext rs_cxt;           /* for hash tables and entries and tuples in
                                   * them */
  XLogRecPtr rs_begin_lsn;        /* XLogInsertLsn when starting the rewrite */
  HTAB *rs_unresolved_tups;       /* unmatched A tuples */
  HTAB *rs_old_new_tid_map;       /* unmatched B tuples */
  HTAB *rs_logical_mappings;      /* logical remapping files */
  uint32 rs_num_rewrite_mappings; /* # in memory mappings */
} RewriteStateData;

/*
 * The lookup keys for the hash tables are tuple TID and xmin (we must check
 * both to avoid false matches from dead tuples).  Beware that there is
 * probably some padding space in this struct; it must be zeroed out for
 * correct hashtable operation.
 */
typedef struct
{
  TransactionId xmin;  /* tuple xmin */
  ItemPointerData tid; /* tuple location in old heap */
} TidHashKey;

/*
 * Entry structures for the hash tables
 */
typedef struct
{
  TidHashKey key;          /* expected xmin/old location of B tuple */
  ItemPointerData old_tid; /* A's location in the old heap */
  HeapTuple tuple;         /* A's tuple contents */
} UnresolvedTupData;

typedef UnresolvedTupData *UnresolvedTup;

typedef struct
{
  TidHashKey key;          /* actual xmin/old location of B tuple */
  ItemPointerData new_tid; /* where we put it in the new heap */
} OldToNewMappingData;

typedef OldToNewMappingData *OldToNewMapping;

/*
 * In-Memory data for an xid that might need logical remapping entries
 * to be logged.
 */
typedef struct RewriteMappingFile
{
  TransactionId xid;    /* xid that might need to see the row */
  int vfd;              /* fd of mappings file */
  off_t off;            /* how far have we written yet */
  uint32 num_mappings;  /* number of in-memory mappings */
  dlist_head mappings;  /* list of in-memory mappings */
  char path[MAXPGPATH]; /* path, for error messages */
} RewriteMappingFile;

/*
 * A single In-Memory logical rewrite mapping, hanging off
 * RewriteMappingFile->mappings.
 */
typedef struct RewriteMappingDataEntry
{
  LogicalRewriteMappingData map; /* map between old and new location of the
                                  * tuple */
  dlist_node node;
} RewriteMappingDataEntry;

/* prototypes for internal functions */
static void
raw_heap_insert(RewriteState state, HeapTuple tup);

/* internal logical remapping prototypes */
static void
logical_begin_heap_rewrite(RewriteState state);
static void
logical_rewrite_heap_tuple(RewriteState state, ItemPointerData old_tid, HeapTuple new_tuple);
static void
logical_end_heap_rewrite(RewriteState state);

/*
 * Begin a rewrite of a table
 *
 * old_heap		old, locked heap relation tuples will be read from
 * new_heap		new, locked heap relation to insert tuples to
 * oldest_xmin	xid used by the caller to determine which tuples are dead
 * freeze_xid	xid before which tuples will be frozen
 * min_multi	multixact before which multis will be removed
 * use_wal		should the inserts to the new heap be WAL-logged?
 *
 * Returns an opaque RewriteState, allocated in current memory context,
 * to be used in subsequent calls to the other functions.
 */
RewriteState
begin_heap_rewrite(Relation old_heap, Relation new_heap, TransactionId oldest_xmin, TransactionId freeze_xid, MultiXactId cutoff_multi, bool use_wal)
{














































}

/*
 * End a rewrite.
 *
 * state and any other resources are freed.
 */
void
end_heap_rewrite(RewriteState state)
{
















































}

/*
 * Add a tuple to the new heap.
 *
 * Visibility information is copied from the original tuple, except that
 * we "freeze" very-old tuples.  Note that since we scribble on new_tuple,
 * it had better be temp storage not a pointer to the original tuple.
 *
 * state		opaque state as returned by begin_heap_rewrite
 * old_tuple	original tuple in the old heap
 * new_tuple	new, rewritten tuple to be inserted to new heap
 */
void
rewrite_heap_tuple(RewriteState state, HeapTuple old_tuple, HeapTuple new_tuple)
{











































































































































































}

/*
 * Register a dead tuple with an ongoing rewrite. Dead tuples are not
 * copied to the new table, but we still make note of them so that we
 * can release some resources earlier.
 *
 * Returns true if a tuple was removed from the unresolved_tups table.
 * This indicates that that tuple, previously thought to be "recently dead",
 * is now known really dead and won't be written to the output.
 */
bool
rewrite_heap_dead_tuple(RewriteState state, HeapTuple old_tuple)
{



































}

/*
 * Insert a tuple to the new relation.  This has to track heap_insert
 * and its subsidiary functions!
 *
 * t_self of the tuple is set to the new TID of the tuple. If t_ctid of the
 * tuple is invalid on entry, it's replaced with the new TID as well (in
 * the inserted data only, not in the caller's copy).
 */
static void
raw_heap_insert(RewriteState state, HeapTuple tup)
{


























































































































}

/* ------------------------------------------------------------------------
 * Logical rewrite support
 *
 * When doing logical decoding - which relies on using cmin/cmax of catalog
 * tuples, via xl_heap_new_cid records - heap rewrites have to log enough
 * information to allow the decoding backend to updates its internal mapping
 * of (relfilenode,ctid) => (cmin, cmax) to be correct for the rewritten heap.
 *
 * For that, every time we find a tuple that's been modified in a catalog
 * relation within the xmin horizon of any decoding slot, we log a mapping
 * from the old to the new location.
 *
 * To deal with rewrites that abort the filename of a mapping file contains
 * the xid of the transaction performing the rewrite, which then can be
 * checked before being read in.
 *
 * For efficiency we don't immediately spill every single map mapping for a
 * row to disk but only do so in batches when we've collected several of them
 * in memory or when end_heap_rewrite() has been called.
 *
 * Crash-Safety: This module diverts from the usual patterns of doing WAL
 * since it cannot rely on checkpoint flushing out all buffers and thus
 * waiting for exclusive locks on buffers. Usually the XLogInsert() covering
 * buffer modifications is performed while the buffer(s) that are being
 * modified are exclusively locked guaranteeing that both the WAL record and
 * the modified heap are on either side of the checkpoint. But since the
 * mapping files we log aren't in shared_buffers that interlock doesn't work.
 *
 * Instead we simply write the mapping files out to disk, *before* the
 * XLogInsert() is performed. That guarantees that either the XLogInsert() is
 * inserted after the checkpoint's redo pointer or that the checkpoint (via
 * LogicalRewriteHeapCheckpoint()) has flushed the (partial) mapping file to
 * disk. That leaves the tail end that has not yet been flushed open to
 * corruption, which is solved by including the current offset in the
 * xl_heap_rewrite_mapping records and truncating the mapping file to it
 * during replay. Every time a rewrite is finished all generated mapping files
 * are synced to disk.
 *
 * Note that if we were only concerned about crash safety we wouldn't have to
 * deal with WAL logging at all - an fsync() at the end of a rewrite would be
 * sufficient for crash safety. Any mapping that hasn't been safely flushed to
 * disk has to be by an aborted (explicitly or via a crash) transaction and is
 * ignored by virtue of the xid in its name being subject to a
 * TransactionDidCommit() check. But we want to support having standbys via
 * physical replication, both for availability and to do logical decoding
 * there.
 * ------------------------------------------------------------------------
 */

/*
 * Do preparations for logging logical mappings during a rewrite if
 * necessary. If we detect that we don't need to log anything we'll prevent
 * any further action by the various logical rewrite functions.
 */
static void
logical_begin_heap_rewrite(RewriteState state)
{







































}

/*
 * Flush all logical in-memory mappings to disk, but don't fsync them yet.
 */
static void
logical_heap_rewrite_flush_mappings(RewriteState state)
{































































































}

/*
 * Logical remapping part of end_heap_rewrite().
 */
static void
logical_end_heap_rewrite(RewriteState state)
{


























}

/*
 * Log a single (old->new) mapping for 'xid'.
 */
static void
logical_rewrite_log_mapping(RewriteState state, TransactionId xid, LogicalRewriteMappingData *map)
{























































}

/*
 * Perform logical remapping for a tuple that's mapped from old_tid to
 * new_tuple->t_self by rewrite_heap_tuple() if necessary for the tuple.
 */
static void
logical_rewrite_heap_tuple(RewriteState state, ItemPointerData old_tid, HeapTuple new_tuple)
{











































































}

/*
 * Replay XLOG_HEAP2_REWRITE records
 */
void
heap_xlog_logical_rewrite(XLogReaderState *r)
{



































































}

/* ---
 * Perform a checkpoint for logical rewrite mappings
 *
 * This serves two tasks:
 * 1) Remove all mappings not needed anymore based on the logical restart LSN
 * 2) Flush all remaining mappings to disk, so that replay after a checkpoint
 *	  only has to deal with the parts of a mapping that have been written
 *out after the checkpoint started.
 * ---
 */
void
CheckPointLogicalRewriteHeap(void)
{





































































































}