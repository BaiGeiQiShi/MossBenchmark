/*-------------------------------------------------------------------------
 *
 * reorderbuffer.c
 *	  PostgreSQL logical replay/reorder buffer management
 *
 *
 * Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/replication/reorderbuffer.c
 *
 * NOTES
 *	  This module gets handed individual pieces of transactions in the order
 *	  they are written to the WAL and is responsible to reassemble them into
 *	  toplevel transaction sized pieces. When a transaction is completely
 *	  reassembled - signalled by reading the transaction commit record - it
 *	  will then call the output plugin (cf. ReorderBufferCommit()) with the
 *	  individual changes. The output plugins rely on snapshots built by
 *	  snapbuild.c which hands them to us.
 *
 *	  Transactions and subtransactions/savepoints in postgres are not
 *	  immediately linked to each other from outside the performing
 *	  backend. Only at commit/abort (or special xact_assignment records)
 *they are linked together. Which means that we will have to splice together a
 *	  toplevel transaction from its subtransactions. To do that efficiently
 *we build a binary heap indexed by the smallest current lsn of the individual
 *	  subtransactions' changestreams. As the individual streams are
 *inherently ordered by LSN - since that is where we build them from - the
 *transaction can easily be reassembled by always using the subtransaction with
 *the smallest current LSN from the heap.
 *
 *	  In order to cope with large transactions - which can be several times
 *as big as the available memory - this module supports spooling the contents of
 *a large transactions to disk. When the transaction is replayed the contents of
 *individual (sub-)transactions will be read from disk in chunks.
 *
 *	  This module also has to deal with reassembling toast records from the
 *	  individual chunks stored in WAL. When a new (or initial) version of a
 *	  tuple is stored in WAL it will always be preceded by the toast chunks
 *	  emitted for the columns stored out of line. Within a single toplevel
 *	  transaction there will be no other data carrying records between a
 *row's toast chunks and the row data itself. See ReorderBufferToast* for
 *	  details.
 *
 *	  ReorderBuffer uses two special memory context types - SlabContext for
 *	  allocations of fixed-length structures (changes and transactions), and
 *	  GenerationContext for the variable-length transaction data (allocated
 *	  and freed in groups with similar lifespan).
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "access/heapam.h"
#include "access/rewriteheap.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "access/xlog_internal.h"
#include "catalog/catalog.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/logical.h"
#include "replication/reorderbuffer.h"
#include "replication/slot.h"
#include "replication/snapbuild.h" /* just for SnapBuildSnapDecRefcount */
#include "storage/bufmgr.h"
#include "storage/fd.h"
#include "storage/sinval.h"
#include "utils/builtins.h"
#include "utils/combocid.h"
#include "utils/memdebug.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/relfilenodemap.h"

/* entry for a hash table we use to map from xid to our transaction state */
typedef struct ReorderBufferTXNByIdEnt
{
  TransactionId xid;
  ReorderBufferTXN *txn;
} ReorderBufferTXNByIdEnt;

/* data structures for (relfilenode, ctid) => (cmin, cmax) mapping */
typedef struct ReorderBufferTupleCidKey
{
  RelFileNode relnode;
  ItemPointerData tid;
} ReorderBufferTupleCidKey;

typedef struct ReorderBufferTupleCidEnt
{
  ReorderBufferTupleCidKey key;
  CommandId cmin;
  CommandId cmax;
  CommandId combocid; /* just for debugging */
} ReorderBufferTupleCidEnt;

/* Virtual file descriptor with file offset tracking */
typedef struct TXNEntryFile
{
  File vfd;        /* -1 when the file is closed */
  off_t curOffset; /* offset for next write or read. Reset to 0
                    * when vfd is opened. */
} TXNEntryFile;

/* k-way in-order change iteration support structures */
typedef struct ReorderBufferIterTXNEntry
{
  XLogRecPtr lsn;
  ReorderBufferChange *change;
  ReorderBufferTXN *txn;
  TXNEntryFile file;
  XLogSegNo segno;
} ReorderBufferIterTXNEntry;

typedef struct ReorderBufferIterTXNState
{
  binaryheap *heap;
  Size nr_txns;
  dlist_head old_change;
  ReorderBufferIterTXNEntry entries[FLEXIBLE_ARRAY_MEMBER];
} ReorderBufferIterTXNState;

/* toast datastructures */
typedef struct ReorderBufferToastEnt
{
  Oid chunk_id;                  /* toast_table.chunk_id */
  int32 last_chunk_seq;          /* toast_table.chunk_seq of the last chunk we
                                  * have seen */
  Size num_chunks;               /* number of chunks we've already seen */
  Size size;                     /* combined size of chunks seen */
  dlist_head chunks;             /* linked list of chunks */
  struct varlena *reconstructed; /* reconstructed varlena now pointed to in
                                  * main tup */
} ReorderBufferToastEnt;

/* Disk serialization support datastructures */
typedef struct ReorderBufferDiskChange
{
  Size size;
  ReorderBufferChange change;
  /* data follows */
} ReorderBufferDiskChange;

/*
 * Maximum number of changes kept in memory, per transaction. After that,
 * changes are spooled to disk.
 *
 * The current value should be sufficient to decode the entire transaction
 * without hitting disk in OLTP workloads, while starting to spool to disk in
 * other workloads reasonably fast.
 *
 * At some point in the future it probably makes sense to have a more elaborate
 * resource management here, but it's not entirely clear what that would look
 * like.
 */
static const Size max_changes_in_memory = 4096;

/* ---------------------------------------
 * primary reorderbuffer support routines
 * ---------------------------------------
 */
static ReorderBufferTXN *
ReorderBufferGetTXN(ReorderBuffer *rb);
static void
ReorderBufferReturnTXN(ReorderBuffer *rb, ReorderBufferTXN *txn);
static ReorderBufferTXN *
ReorderBufferTXNByXid(ReorderBuffer *rb, TransactionId xid, bool create, bool *is_new, XLogRecPtr lsn, bool create_as_top);
static void
ReorderBufferTransferSnapToParent(ReorderBufferTXN *txn, ReorderBufferTXN *subtxn);

static void
AssertTXNLsnOrder(ReorderBuffer *rb);

/* ---------------------------------------
 * support functions for lsn-order iterating over the ->changes of a
 * transaction and its subtransactions
 *
 * used for iteration over the k-way heap merge of a transaction and its
 * subtransactions
 * ---------------------------------------
 */
static void
ReorderBufferIterTXNInit(ReorderBuffer *rb, ReorderBufferTXN *txn, ReorderBufferIterTXNState *volatile *iter_state);
static ReorderBufferChange *
ReorderBufferIterTXNNext(ReorderBuffer *rb, ReorderBufferIterTXNState *state);
static void
ReorderBufferIterTXNFinish(ReorderBuffer *rb, ReorderBufferIterTXNState *state);
static void
ReorderBufferExecuteInvalidations(ReorderBuffer *rb, ReorderBufferTXN *txn);

/*
 * ---------------------------------------
 * Disk serialization support functions
 * ---------------------------------------
 */
static void
ReorderBufferCheckSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferSerializeChange(ReorderBuffer *rb, ReorderBufferTXN *txn, int fd, ReorderBufferChange *change);
static Size
ReorderBufferRestoreChanges(ReorderBuffer *rb, ReorderBufferTXN *txn, TXNEntryFile *file, XLogSegNo *segno);
static void
ReorderBufferRestoreChange(ReorderBuffer *rb, ReorderBufferTXN *txn, char *change);
static void
ReorderBufferRestoreCleanup(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferCleanupSerializedTXNs(const char *slotname);
static void
ReorderBufferSerializedPath(char *path, ReplicationSlot *slot, TransactionId xid, XLogSegNo segno);

static void
ReorderBufferFreeSnap(ReorderBuffer *rb, Snapshot snap);
static Snapshot
ReorderBufferCopySnap(ReorderBuffer *rb, Snapshot orig_snap, ReorderBufferTXN *txn, CommandId cid);

/* ---------------------------------------
 * toast reassembly support
 * ---------------------------------------
 */
static void
ReorderBufferToastInitHash(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferToastReset(ReorderBuffer *rb, ReorderBufferTXN *txn);
static void
ReorderBufferToastReplace(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change);
static void
ReorderBufferToastAppendChunk(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change);

/*
 * Allocate a new ReorderBuffer and clean out any old serialized state from
 * prior ReorderBuffer instances for the same slot.
 */
ReorderBuffer *
ReorderBufferAllocate(void)
{














































}

/*
 * Free a ReorderBuffer
 */
void
ReorderBufferFree(ReorderBuffer *rb)
{










}

/*
 * Get an unused, possibly preallocated, ReorderBufferTXN.
 */
static ReorderBufferTXN *
ReorderBufferGetTXN(ReorderBuffer *rb)
{











}

/*
 * Free a ReorderBufferTXN.
 */
static void
ReorderBufferReturnTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{

























}

/*
 * Get an fresh ReorderBufferChange.
 */
ReorderBufferChange *
ReorderBufferGetChange(ReorderBuffer *rb)
{






}

/*
 * Free an ReorderBufferChange.
 */
void
ReorderBufferReturnChange(ReorderBuffer *rb, ReorderBufferChange *change)
{






















































}

/*
 * Get a fresh ReorderBufferTupleBuf fitting at least a tuple of size
 * tuple_len (excluding header overhead).
 */
ReorderBufferTupleBuf *
ReorderBufferGetTupleBuf(ReorderBuffer *rb, Size tuple_len)
{










}

/*
 * Free an ReorderBufferTupleBuf.
 */
void
ReorderBufferReturnTupleBuf(ReorderBuffer *rb, ReorderBufferTupleBuf *tuple)
{

}

/*
 * Get an array for relids of truncated relations.
 *
 * We use the global memory context (for the whole reorder buffer), because
 * none of the existing ones seems like a good match (some are SLAB, so we
 * can't use those, and tup_context is meant for tuple data, not relids). We
 * could add yet another context, but it seems like an overkill - TRUNCATE is
 * not particularly common operation, so it does not seem worth it.
 */
Oid *
ReorderBufferGetRelids(ReorderBuffer *rb, int nrelids)
{








}

/*
 * Free an array of relids.
 */
void
ReorderBufferReturnRelids(ReorderBuffer *rb, Oid *relids)
{

}

/*
 * Return the ReorderBufferTXN from the given buffer, specified by Xid.
 * If create is true, and a transaction doesn't already exist, create it
 * (with the given LSN, and as top transaction if that's specified);
 * when this happens, is_new is set to true.
 */
static ReorderBufferTXN *
ReorderBufferTXNByXid(ReorderBuffer *rb, TransactionId xid, bool create, bool *is_new, XLogRecPtr lsn, bool create_as_top)
{















































































}

/*
 * Queue a change into a transaction so it can be replayed upon commit.
 */
void
ReorderBufferQueueChange(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, ReorderBufferChange *change)
{











}

/*
 * Queue message into a transaction so it can be processed upon commit.
 */
void
ReorderBufferQueueMessage(ReorderBuffer *rb, TransactionId xid, Snapshot snapshot, XLogRecPtr lsn, bool transactional, const char *prefix, Size message_size, const char *message)
{













































}

/*
 * AssertTXNLsnOrder
 *		Verify LSN ordering of transaction lists in the reorderbuffer
 *
 * Other LSN-related invariants are checked too.
 *
 * No-op if assertions are not in use.
 */
static void
AssertTXNLsnOrder(ReorderBuffer *rb)
{


































































}

/*
 * ReorderBufferGetOldestTXN
 *		Return oldest transaction in reorderbuffer
 */
ReorderBufferTXN *
ReorderBufferGetOldestTXN(ReorderBuffer *rb)
{














}

/*
 * ReorderBufferGetOldestXmin
 *		Return oldest Xmin in reorderbuffer
 *
 * Returns oldest possibly running Xid from the point of view of snapshots
 * used in the transactions kept by reorderbuffer, or InvalidTransactionId if
 * there are none.
 *
 * Since snapshots are assigned monotonically, this equals the Xmin of the
 * base snapshot with minimal base_snapshot_lsn.
 */
TransactionId
ReorderBufferGetOldestXmin(ReorderBuffer *rb)
{











}

void
ReorderBufferSetRestartPoint(ReorderBuffer *rb, XLogRecPtr ptr)
{

}

/*
 * ReorderBufferAssignChild
 *
 * Make note that we know that subxid is a subtransaction of xid, seen as of
 * the given lsn.
 */
void
ReorderBufferAssignChild(ReorderBuffer *rb, TransactionId xid, TransactionId subxid, XLogRecPtr lsn)
{







































}

/*
 * ReorderBufferTransferSnapToParent
 *		Transfer base snapshot from subtxn to top-level txn, if needed
 *
 * This is done if the top-level txn doesn't have a base snapshot, or if the
 * subtxn's base snapshot has an earlier LSN than the top-level txn's base
 * snapshot's LSN.  This can happen if there are no changes in the toplevel
 * txn but there are some in the subtxn, or the first change in subtxn has
 * earlier LSN than first change in the top-level txn and we learned about
 * their kinship only now.
 *
 * The subtransaction's snapshot is cleared regardless of the transfer
 * happening, since it's not needed anymore in either case.
 *
 * We do this as soon as we become aware of their kinship, to avoid queueing
 * extra snapshots to txns known-as-subtxns -- only top-level txns will
 * receive further snapshots.
 */
static void
ReorderBufferTransferSnapToParent(ReorderBufferTXN *txn, ReorderBufferTXN *subtxn)
{










































}

/*
 * Associate a subtransaction with its toplevel transaction at commit
 * time. There may be no further changes added after this.
 */
void
ReorderBufferCommitChild(ReorderBuffer *rb, TransactionId xid, TransactionId subxid, XLogRecPtr commit_lsn, XLogRecPtr end_lsn)
{




















}

/*
 * Support for efficiently iterating over a transaction's and its
 * subtransactions' changes.
 *
 * We do by doing a k-way merge between transactions/subtransactions. For that
 * we model the current heads of the different transactions as a binary heap
 * so we easily know which (sub-)transaction has the change with the smallest
 * lsn next.
 *
 * We assume the changes in individual transactions are already sorted by LSN.
 */

/*
 * Binary heap comparison function.
 */
static int
ReorderBufferIterCompare(Datum a, Datum b, void *arg)
{













}

/*
 * Allocate & initialize an iterator which iterates in lsn order over a
 * transaction and all its subtransactions.
 *
 * Note: The iterator state is returned through iter_state parameter rather
 * than the function's return value.  This is because the state gets cleaned up
 * in a PG_CATCH block in the caller, so we want to make sure the caller gets
 * back the state even if this function throws an exception.
 */
static void
ReorderBufferIterTXNInit(ReorderBuffer *rb, ReorderBufferTXN *txn, ReorderBufferIterTXNState *volatile *iter_state)
{













































































































}

/*
 * Return the next change when iterating over a transaction and its
 * subtransactions.
 *
 * Returns NULL when no further changes exist.
 */
static ReorderBufferChange *
ReorderBufferIterTXNNext(ReorderBuffer *rb, ReorderBufferIterTXNState *state)
{









































































}

/*
 * Deallocate the iterator
 */
static void
ReorderBufferIterTXNFinish(ReorderBuffer *rb, ReorderBufferIterTXNState *state)
{






















}

/*
 * Cleanup the contents of a transaction, usually after the transaction
 * committed or aborted.
 */
static void
ReorderBufferCleanupTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{











































































}

/*
 * Build a hash with a (relfilenode, ctid) -> (cmin, cmax) mapping for use by
 * HeapTupleSatisfiesHistoricMVCC.
 */
static void
ReorderBufferBuildTupleCidHash(ReorderBuffer *rb, ReorderBufferTXN *txn)
{





























































}

/*
 * Copy a provided snapshot so we can modify it privately. This is needed so
 * that catalog modifying transactions can look into intermediate catalog
 * states.
 */
static Snapshot
ReorderBufferCopySnap(ReorderBuffer *rb, Snapshot orig_snap, ReorderBufferTXN *txn, CommandId cid)
{
















































}

/*
 * Free a previously ReorderBufferCopySnap'ed snapshot
 */
static void
ReorderBufferFreeSnap(ReorderBuffer *rb, Snapshot snap)
{








}

/*
 * Perform the replay of a transaction and its non-aborted subtransactions.
 *
 * Subtransactions previously have to be processed by
 * ReorderBufferCommitChild(), even if previously assigned to the toplevel
 * transaction with ReorderBufferAssignChild.
 *
 * We currently can only decode a transaction's contents when its commit
 * record is read because that's the only place where we know about cache
 * invalidations. Thus, once a toplevel commit is read, we iterate over the top
 * and subtransactions (using a k-way merge) and replay the changes in lsn
 * order.
 */
void
ReorderBufferCommit(ReorderBuffer *rb, TransactionId xid, XLogRecPtr commit_lsn, XLogRecPtr end_lsn, TimestampTz commit_time, RepOriginId origin_id, XLogRecPtr origin_lsn)
{


























































































































































































































































































































































































































































}

/*
 * Abort a transaction that possibly has previous changes. Needs to be first
 * called for subtransactions and then for the toplevel xid.
 *
 * NB: Transactions handled here have to have actively aborted (i.e. have
 * produced an abort record). Implicitly aborted transactions are handled via
 * ReorderBufferAbortOld(); transactions we're just not interested in, but
 * which have committed are handled in ReorderBufferForget().
 *
 * This function purges this transaction and its contents from memory and
 * disk.
 */
void
ReorderBufferAbort(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{















}

/*
 * Abort all transactions that aren't actually running anymore because the
 * server restarted.
 *
 * NB: These really have to be transactions that have aborted due to a server
 * crash/immediate restart, as we don't deal with invalidations here.
 */
void
ReorderBufferAbortOld(ReorderBuffer *rb, TransactionId oldestRunningXid)
{



























}

/*
 * Forget the contents of a transaction if we aren't interested in its
 * contents. Needs to be first called for subtransactions and then for the
 * toplevel xid.
 *
 * This is significantly different to ReorderBufferAbort() because
 * transactions that have committed need to be treated differently from aborted
 * ones since they may have modified the catalog.
 *
 * Note that this is only allowed to be called in the moment a transaction
 * commit has just been read, not earlier; otherwise later records referring
 * to this xid might re-create the transaction incompletely.
 */
void
ReorderBufferForget(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{





























}

/*
 * Execute invalidations happening outside the context of a decoded
 * transaction. That currently happens either for xid-less commits
 * (cf. RecordTransactionCommit()) or for invalidations in uninteresting
 * transactions (via ReorderBufferForget()).
 */
void
ReorderBufferImmediateInvalidation(ReorderBuffer *rb, uint32 ninvalidations, SharedInvalidationMessage *invalidations)
{




























}

/*
 * Tell reorderbuffer about an xid seen in the WAL stream. Has to be called at
 * least once for every xid in XLogRecord->xl_xid (other places in records
 * may, but do not have to be passed through here).
 *
 * Reorderbuffer keeps some datastructures about transactions in LSN order,
 * for efficiency. To do that it has to know about when transactions are seen
 * first in the WAL. As many types of records are not actually interesting for
 * logical decoding, they do not necessarily pass though here.
 */
void
ReorderBufferProcessXid(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{





}

/*
 * Add a new snapshot to this transaction that may only used after lsn 'lsn'
 * because the previous snapshot doesn't describe the catalog correctly for
 * following rows.
 */
void
ReorderBufferAddSnapshot(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, Snapshot snap)
{






}

/*
 * Set up the transaction's base snapshot.
 *
 * If we know that xid is a subtransaction, set the base snapshot on the
 * top-level transaction instead.
 */
void
ReorderBufferSetBaseSnapshot(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, Snapshot snap)
{





















}

/*
 * Access the catalog with this CommandId at this point in the changestream.
 *
 * May only be called for command ids > 1
 */
void
ReorderBufferAddNewCommandId(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, CommandId cid)
{






}

/*
 * Add new (relfilenode, tid) -> (cmin, cmax) mappings.
 */
void
ReorderBufferAddNewTupleCids(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, RelFileNode node, ItemPointerData tid, CommandId cmin, CommandId cmax, CommandId combocid)
{















}

/*
 * Setup the invalidation of the toplevel transaction.
 *
 * This needs to be done before ReorderBufferCommit is called!
 */
void
ReorderBufferAddInvalidations(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn, Size nmsgs, SharedInvalidationMessage *msgs)
{














}

/*
 * Apply all invalidations we know. Possibly we only need parts at this point
 * in the changestream but we don't know which those are.
 */
static void
ReorderBufferExecuteInvalidations(ReorderBuffer *rb, ReorderBufferTXN *txn)
{






}

/*
 * Mark a transaction as containing catalog changes
 */
void
ReorderBufferXidSetCatalogChanges(ReorderBuffer *rb, TransactionId xid, XLogRecPtr lsn)
{





}

/*
 * Query whether a transaction is already *known* to contain catalog
 * changes. This can be wrong until directly before the commit!
 */
bool
ReorderBufferXidHasCatalogChanges(ReorderBuffer *rb, TransactionId xid)
{









}

/*
 * ReorderBufferXidHasBaseSnapshot
 *		Have we already set the base snapshot for the given txn/subtxn?
 */
bool
ReorderBufferXidHasBaseSnapshot(ReorderBuffer *rb, TransactionId xid)
{

















}

/*
 * ---------------------------------------
 * Disk serialization support
 * ---------------------------------------
 */

/*
 * Ensure the IO buffer is >= sz.
 */
static void
ReorderBufferSerializeReserve(ReorderBuffer *rb, Size sz)
{










}

/*
 * Check whether the transaction tx should spill its data to disk.
 */
static void
ReorderBufferCheckSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{









}

/*
 * Spill data of a large transaction (and its subtransactions) to disk.
 */
static void
ReorderBufferSerializeTXN(ReorderBuffer *rb, ReorderBufferTXN *txn)
{






































































}

/*
 * Serialize individual change to disk.
 */
static void
ReorderBufferSerializeChange(ReorderBuffer *rb, ReorderBufferTXN *txn, int fd, ReorderBufferChange *change)
{





















































































































































































}

/*
 * Restore a number of changes spilled to disk back into memory.
 */
static Size
ReorderBufferRestoreChanges(ReorderBuffer *rb, ReorderBufferTXN *txn, TXNEntryFile *file, XLogSegNo *segno)
{





















































































































}

/*
 * Convert change from its on-disk format to in-memory format and queue it onto
 * the TXN's ->changes list.
 *
 * Note: although "data" is declared char*, at entry it points to a
 * maxalign'd buffer, making it safe in most of this function to assume
 * that the pointed-to data is suitably aligned for direct access.
 */
static void
ReorderBufferRestoreChange(ReorderBuffer *rb, ReorderBufferTXN *txn, char *data)
{

























































































































}

/*
 * Remove all on-disk stored for the passed in transaction.
 */
static void
ReorderBufferRestoreCleanup(ReorderBuffer *rb, ReorderBufferTXN *txn)
{





















}

/*
 * Remove any leftover serialized reorder buffers from a slot directory after a
 * prior crash or decoding session exit.
 */
static void
ReorderBufferCleanupSerializedTXNs(const char *slotname)
{




























}

/*
 * Given a replication slot, transaction ID and segment number, fill in the
 * corresponding spill file into 'path', which is a caller-owned buffer of size
 * at least MAXPGPATH.
 */
static void
ReorderBufferSerializedPath(char *path, ReplicationSlot *slot, TransactionId xid, XLogSegNo segno)
{





}

/*
 * Delete all data spilled to disk after we've restarted/crashed. It will be
 * recreated when the respective slots are reused.
 */
void
StartupReorderBuffer(void)
{
  DIR *logical_dir;
  struct dirent *logical_de;

  logical_dir = AllocateDir("pg_replslot");
  while ((logical_de = ReadDir(logical_dir, "pg_replslot")) != NULL)
  {
    if (strcmp(logical_de->d_name, ".") == 0 || strcmp(logical_de->d_name, "..") == 0)
    {
      continue;
    }

    /* if it cannot be a slot, skip the directory */





    /*
     * ok, has to be a surviving logical slot, iterate and delete
     * everything starting with xid-*
     */

  }
  FreeDir(logical_dir);
}

/* ---------------------------------------
 * toast reassembly support
 * ---------------------------------------
 */

/*
 * Initialize per tuple toast reconstruction support.
 */
static void
ReorderBufferToastInitHash(ReorderBuffer *rb, ReorderBufferTXN *txn)
{









}

/*
 * Per toast-chunk handling for toast reconstruction
 *
 * Appends a toast chunk so we can reconstruct it when the tuple "owning" the
 * toasted Datum comes along.
 */
static void
ReorderBufferToastAppendChunk(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change)
{


































































}

/*
 * Rejigger change->newtuple to point to in-memory toast tuples instead to
 * on-disk toast tuples that may not longer exist (think DROP TABLE or VACUUM).
 *
 * We cannot replace unchanged toast tuples though, so those will still point
 * to on-disk toast data.
 */
static void
ReorderBufferToastReplace(ReorderBuffer *rb, ReorderBufferTXN *txn, Relation relation, ReorderBufferChange *change)
{
















































































































































































}

/*
 * Free all resources allocated for toast reconstruction.
 */
static void
ReorderBufferToastReset(ReorderBuffer *rb, ReorderBufferTXN *txn)
{






























}

/* ---------------------------------------
 * Visibility support for logical decoding
 *
 *
 * Lookup actual cmin/cmax values when using decoding snapshot. We can't
 * always rely on stored cmin/cmax values because of two scenarios:
 *
 * * A tuple got changed multiple times during a single transaction and thus
 *	 has got a combocid. Combocid's are only valid for the duration of a
 *	 single transaction.
 * * A tuple with a cmin but no cmax (and thus no combocid) got
 *	 deleted/updated in another transaction than the one which created it
 *	 which we are looking at right now. As only one of cmin, cmax or
 *combocid is actually stored in the heap we don't have access to the value we
 *	 need anymore.
 *
 * To resolve those problems we have a per-transaction hash of (cmin,
 * cmax) tuples keyed by (relfilenode, ctid) which contains the actual
 * (cmin, cmax) values. That also takes care of combocids by simply
 * not caring about them at all. As we have the real cmin/cmax values
 * combocids aren't interesting.
 *
 * As we only care about catalog tuples here the overhead of this
 * hashtable should be acceptable.
 *
 * Heap rewrites complicate this a bit, check rewriteheap.c for
 * details.
 * -------------------------------------------------------------------------
 */

/* struct for qsort()ing mapping files by lsn somewhat efficiently */
typedef struct RewriteMappingFile
{
  XLogRecPtr lsn;
  char fname[MAXPGPATH];
} RewriteMappingFile;

#if NOT_USED
static void
DisplayMapping(HTAB *tuplecid_data)
{
  HASH_SEQ_STATUS hstat;
  ReorderBufferTupleCidEnt *ent;

  hash_seq_init(&hstat, tuplecid_data);
  while ((ent = (ReorderBufferTupleCidEnt *)hash_seq_search(&hstat)) != NULL)
  {
    elog(DEBUG3, "mapping: node: %u/%u/%u tid: %u/%u cmin: %u, cmax: %u", ent->key.relnode.dbNode, ent->key.relnode.spcNode, ent->key.relnode.relNode, ItemPointerGetBlockNumber(&ent->key.tid), ItemPointerGetOffsetNumber(&ent->key.tid), ent->cmin, ent->cmax);
  }
}
#endif

/*
 * Apply a single mapping file to tuplecid_data.
 *
 * The mapping file has to have been verified to be a) committed b) for our
 * transaction c) applied in LSN order.
 */
static void
ApplyLogicalMappingFile(HTAB *tuplecid_data, Oid relid, const char *fname)
{















































































}

/*
 * Check whether the TransactionOid 'xid' is in the pre-sorted array 'xip'.
 */
static bool
TransactionIdInArray(TransactionId xid, TransactionId *xip, Size num)
{

}

/*
 * qsort() comparator for sorting RewriteMappingFiles in LSN order.
 */
static int
file_sort_by_lsn(const void *a_p, const void *b_p)
{












}

/*
 * Apply any existing logical remapping files if there are any targeted at our
 * transaction for relid.
 */
static void
UpdateLogicalMappings(HTAB *tuplecid_data, Oid relid, Snapshot snapshot)
{
























































































}

/*
 * Lookup cmin/cmax of a tuple, during logical decoding where we can't rely on
 * combocids.
 */
bool
ResolveCminCmaxDuringDecoding(HTAB *tuplecid_data, Snapshot snapshot, HeapTuple htup, Buffer buffer, CommandId *cmin, CommandId *cmax)
{





















































}