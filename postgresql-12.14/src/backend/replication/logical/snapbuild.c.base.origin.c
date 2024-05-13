/*-------------------------------------------------------------------------
 *
 * snapbuild.c
 *
 *	  Infrastructure for building historic catalog snapshots based on
 *contents of the WAL, for the purpose of decoding heapam.c style values in the
 *	  WAL.
 *
 * NOTES:
 *
 * We build snapshots which can *only* be used to read catalog contents and we
 * do so by reading and interpreting the WAL stream. The aim is to build a
 * snapshot that behaves the same as a freshly taken MVCC snapshot would have
 * at the time the XLogRecord was generated.
 *
 * To build the snapshots we reuse the infrastructure built for Hot
 * Standby. The in-memory snapshots we build look different than HS' because
 * we have different needs. To successfully decode data from the WAL we only
 * need to access catalog tables and (sys|rel|cat)cache, not the actual user
 * tables since the data we decode is wholly contained in the WAL
 * records. Also, our snapshots need to be different in comparison to normal
 * MVCC ones because in contrast to those we cannot fully rely on the clog and
 * pg_subtrans for information about committed transactions because they might
 * commit in the future from the POV of the WAL entry we're currently
 * decoding. This definition has the advantage that we only need to prevent
 * removal of catalog rows, while normal table's rows can still be
 * removed. This is achieved by using the replication slot mechanism.
 *
 * As the percentage of transactions modifying the catalog normally is fairly
 * small in comparisons to ones only manipulating user data, we keep track of
 * the committed catalog modifying ones inside [xmin, xmax) instead of keeping
 * track of all running transactions like it's done in a normal snapshot. Note
 * that we're generally only looking at transactions that have acquired an
 * xid. That is we keep a list of transactions between snapshot->(xmin, xmax)
 * that we consider committed, everything else is considered aborted/in
 * progress. That also allows us not to care about subtransactions before they
 * have committed which means this module, in contrast to HS, doesn't have to
 * care about suboverflowed subtransactions and similar.
 *
 * One complexity of doing this is that to e.g. handle mixed DDL/DML
 * transactions we need Snapshots that see intermediate versions of the
 * catalog in a transaction. During normal operation this is achieved by using
 * CommandIds/cmin/cmax. The problem with that however is that for space
 * efficiency reasons only one value of that is stored
 * (cf. combocid.c). Since ComboCids are only available in memory we log
 * additional information which allows us to get the original (cmin, cmax)
 * pair during visibility checks. Check the reorderbuffer.c's comment above
 * ResolveCminCmaxDuringDecoding() for details.
 *
 * To facilitate all this we need our own visibility routine, as the normal
 * ones are optimized for different usecases.
 *
 * To replace the normal catalog snapshots with decoding ones use the
 * SetupHistoricSnapshot() and TeardownHistoricSnapshot() functions.
 *
 *
 *
 * The snapbuild machinery is starting up in several stages, as illustrated
 * by the following graph describing the SnapBuild->state transitions:
 *
 *		   +-------------------------+
 *	  +----|		 START			 |-------------+
 *	  |    +-------------------------+			   |
 *	  |					|
 *| |					|
 *| |		   running_xacts #1					   | |
 *|						   | |
 *|						   | |
 *v						   | |
 *+-------------------------+			   v |    |   BUILDING_SNAPSHOT
 *|------------>| |    +-------------------------+			   | |
 *|						   | |
 *|						   | | running_xacts #2, xacts
 *from #1 finished   | |					|
 *| |					|
 *| |					v
 *| |    +-------------------------+			   v |    |
 *FULL_SNAPSHOT	 |------------>| |    +-------------------------+
 *| |					|
 *| running_xacts		|					   saved
 *snapshot with zero xacts		|				  at
 *running_xacts's lsn |					|
 *| | running_xacts with xacts from #2 finished  | |
 *|						   | |
 *v						   | |
 *+-------------------------+			   |
 *	  +--->|SNAPBUILD_CONSISTENT	 |<------------+
 *		   +-------------------------+
 *
 * Initially the machinery is in the START stage. When an xl_running_xacts
 * record is read that is sufficiently new (above the safe xmin horizon),
 * there's a state transition. If there were no running xacts when the
 * running_xacts record was generated, we'll directly go into CONSISTENT
 * state, otherwise we'll switch to the BUILDING_SNAPSHOT state. Having a full
 * snapshot means that all transactions that start henceforth can be decoded
 * in their entirety, but transactions that started previously can't. In
 * FULL_SNAPSHOT we'll switch into CONSISTENT once all those previously
 * running transactions have committed or aborted.
 *
 * Only transactions that commit after CONSISTENT state has been reached will
 * be replayed, even though they might have started while still in
 * FULL_SNAPSHOT. That ensures that we'll reach a point where no previous
 * changes has been exported, but all the following ones will be. That point
 * is a convenient point to initialize replication from, which is why we
 * export a snapshot at that point, which *can* be used to read normal data.
 *
 * Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/snapbuild.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "miscadmin.h"

#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/xact.h"

#include "pgstat.h"

#include "replication/logical.h"
#include "replication/reorderbuffer.h"
#include "replication/snapbuild.h"

#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapshot.h"
#include "utils/snapmgr.h"

#include "storage/block.h" /* debugging output */
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/standby.h"

/*
 * This struct contains the current state of the snapshot building
 * machinery. Besides a forward declaration in the header, it is not exposed
 * to the public, so we can easily change its contents.
 */
struct SnapBuild
{
  /* how far are we along building our first full snapshot */
  SnapBuildState state;

  /* private memory context used to allocate memory for this module. */
  MemoryContext context;

  /* all transactions < than this have committed/aborted */
  TransactionId xmin;

  /* all transactions >= than this are uncommitted */
  TransactionId xmax;

  /*
   * Don't replay commits from an LSN < this LSN. This can be set externally
   * but it will also be advanced (never retreat) from within snapbuild.c.
   */
  XLogRecPtr start_decoding_at;

  /*
   * Don't start decoding WAL until the "xl_running_xacts" information
   * indicates there are no running xids with an xid smaller than this.
   */
  TransactionId initial_xmin_horizon;

  /* Indicates if we are building full snapshot or just catalog one. */
  bool building_full_snapshot;

  /*
   * Snapshot that's valid to see the catalog state seen at this moment.
   */
  Snapshot snapshot;

  /*
   * LSN of the last location we are sure a snapshot has been serialized to.
   */
  XLogRecPtr last_serialized_snapshot;

  /*
   * The reorderbuffer we need to update with usable snapshots et al.
   */
  ReorderBuffer *reorder;

  /*
   * Outdated: This struct isn't used for its original purpose anymore, but
   * can't be removed / changed in a minor version, because it's stored
   * on-disk.
   */
  struct
  {
    /*
     * NB: This field is misused, until a major version can break on-disk
     * compatibility. See SnapBuildNextPhaseAt() /
     * SnapBuildStartNextPhaseAt().
     */
    TransactionId was_xmin;
    TransactionId was_xmax;

    size_t was_xcnt;        /* number of used xip entries */
    size_t was_xcnt_space;  /* allocated size of xip */
    TransactionId *was_xip; /* running xacts array, xidComparator-sorted */
  } was_running;

  /*
   * Array of transactions which could have catalog changes that committed
   * between xmin and xmax.
   */
  struct
  {
    /* number of committed transactions */
    size_t xcnt;

    /* available space for committed transactions */
    size_t xcnt_space;

    /*
     * Until we reach a CONSISTENT state, we record commits of all
     * transactions, not just the catalog changing ones. Record when that
     * changes so we know we cannot export a snapshot safely anymore.
     */
    bool includes_all_transactions;

    /*
     * Array of committed transactions that have modified the catalog.
     *
     * As this array is frequently modified we do *not* keep it in
     * xidComparator order. Instead we sort the array when building &
     * distributing a snapshot.
     *
     * TODO: It's unclear whether that reasoning has much merit. Every
     * time we add something here after becoming consistent will also
     * require distributing a snapshot. Storing them sorted would
     * potentially also make it easier to purge (but more complicated wrt
     * wraparound?). Should be improved if sorting while building the
     * snapshot shows up in profiles.
     */
    TransactionId *xip;
  } committed;
};

/*
 * Starting a transaction -- which we need to do while exporting a snapshot --
 * removes knowledge about the previously used resowner, so we save it here.
 */
static ResourceOwner SavedResourceOwnerDuringExport = NULL;
static bool ExportInProgress = false;

/*
 * Array of transactions and subtransactions that were running when
 * the xl_running_xacts record that we decoded was written. The array is
 * sorted in xidComparator order. We remove xids from this array when
 * they become old enough to matter, and then it eventually becomes empty.
 * This array is allocated in builder->context so its lifetime is the same
 * as the snapshot builder.
 *
 * We normally rely on some WAL record types such as HEAP2_NEW_CID to know
 * if the transaction has changed the catalog. But it could happen that the
 * logical decoding decodes only the commit record of the transaction after
 * restoring the previously serialized snapshot in which case we will miss
 * adding the xid to the snapshot and end up looking at the catalogs with the
 * wrong snapshot.
 *
 * Now to avoid the above problem, if the COMMIT record of the xid listed in
 * InitialRunningXacts has XACT_XINFO_HAS_INVALS flag, we mark both the top
 * transaction and its substransactions as containing catalog changes.
 *
 * We could end up adding the transaction that didn't change catalog
 * to the snapshot since we cannot distinguish whether the transaction
 * has catalog changes only by checking the COMMIT record. It doesn't
 * have the information on which (sub) transaction has catalog changes,
 * and XACT_XINFO_HAS_INVALS doesn't necessarily indicate that the
 * transaction has catalog change. But that won't be a problem since we
 * use snapshot built during decoding only for reading system catalogs.
 */
static TransactionId *InitialRunningXacts = NULL;
static int NInitialRunningXacts = 0;

/* ->committed and InitailRunningXacts manipulation */
static void
SnapBuildPurgeOlderTxn(SnapBuild *builder);

/* snapshot building/manipulation/distribution functions */
static Snapshot
SnapBuildBuildSnapshot(SnapBuild *builder);

static void
SnapBuildFreeSnapshot(Snapshot snap);

static void
SnapBuildSnapIncRefcount(Snapshot snap);

static void
SnapBuildDistributeNewCatalogSnapshot(SnapBuild *builder, XLogRecPtr lsn);

/* xlog reading helper functions for SnapBuildProcessRecord */
static bool
SnapBuildFindSnapshot(SnapBuild *builder, XLogRecPtr lsn, xl_running_xacts *running);
static void
SnapBuildWaitSnapshot(xl_running_xacts *running, TransactionId cutoff);

/* serialization functions */
static void
SnapBuildSerialize(SnapBuild *builder, XLogRecPtr lsn);
static bool
SnapBuildRestore(SnapBuild *builder, XLogRecPtr lsn);

/*
 * Return TransactionId after which the next phase of initial snapshot
 * building will happen.
 */
static inline TransactionId
SnapBuildNextPhaseAt(SnapBuild *builder)
{





}

/*
 * Set TransactionId after which the next phase of initial snapshot building
 * will happen.
 */
static inline void
SnapBuildStartNextPhaseAt(SnapBuild *builder, TransactionId at)
{





}

/*
 * Allocate a new snapshot builder.
 *
 * xmin_horizon is the xid >= which we can be sure no catalog rows have been
 * removed, start_lsn is the LSN >= we want to replay commits.
 */
SnapBuild *
AllocateSnapshotBuilder(ReorderBuffer *reorder, TransactionId xmin_horizon, XLogRecPtr start_lsn, bool need_full_snapshot)
{






























}

/*
 * Free a snapshot builder.
 */
void
FreeSnapshotBuilder(SnapBuild *builder)
{















}

/*
 * Free an unreferenced snapshot that has previously been built by us.
 */
static void
SnapBuildFreeSnapshot(Snapshot snap)
{





















}

/*
 * In which state of snapshot building are we?
 */
SnapBuildState
SnapBuildCurrentState(SnapBuild *builder)
{

}

/*
 * Should the contents of transaction ending at 'ptr' be decoded?
 */
bool
SnapBuildXactNeedsSkip(SnapBuild *builder, XLogRecPtr ptr)
{

}

/*
 * Increase refcount of a snapshot.
 *
 * This is used when handing out a snapshot to some external resource or when
 * adding a Snapshot as builder->snapshot.
 */
static void
SnapBuildSnapIncRefcount(Snapshot snap)
{

}

/*
 * Decrease refcount of a snapshot and free if the refcount reaches zero.
 *
 * Externally visible, so that external resources that have been handed an
 * IncRef'ed Snapshot can adjust its refcount easily.
 */
void
SnapBuildSnapDecRefcount(Snapshot snap)
{























}

/*
 * Build a new snapshot, based on currently committed catalog-modifying
 * transactions.
 *
 * In-progress transactions with catalog access are *not* allowed to modify
 * these snapshots; they have to copy them and fill in appropriate ->curcid
 * and ->subxip/subxcnt values.
 */
static Snapshot
SnapBuildBuildSnapshot(SnapBuild *builder)
{






























































}

/*
 * Build the initial slot snapshot and convert it to a normal snapshot that
 * is understood by HeapTupleSatisfiesMVCC.
 *
 * The snapshot will be usable directly in current transaction or exported
 * for loading in different transaction.
 */
Snapshot
SnapBuildInitialSnapshot(SnapBuild *builder)
{



















































































}

/*
 * Export a snapshot so it can be set in another session with SET TRANSACTION
 * SNAPSHOT.
 *
 * For that we need to start a transaction in the current backend as the
 * importing side checks whether the source transaction is still open to make
 * sure the xmin horizon hasn't advanced since then.
 */
const char *
SnapBuildExportSnapshot(SnapBuild *builder)
{
































}

/*
 * Ensure there is a snapshot and if not build one for current transaction.
 */
Snapshot
SnapBuildGetOrBuildSnapshot(SnapBuild *builder, TransactionId xid)
{











}

/*
 * Reset a previously SnapBuildExportSnapshot()'ed snapshot if there is
 * any. Aborts the previously started transaction and resets the resource
 * owner back to its original value.
 */
void
SnapBuildClearExportedSnapshot(void)
{























}

/*
 * Clear snapshot export state during transaction abort.
 */
void
SnapBuildResetExportedSnapshotState(void)
{


}

/*
 * Handle the effects of a single heap change, appropriate to the current state
 * of the snapshot builder and returns whether changes made at (xid, lsn) can
 * be decoded.
 */
bool
SnapBuildProcessChange(SnapBuild *builder, TransactionId xid, XLogRecPtr lsn)
{










































}

/*
 * Do CommandId/ComboCid handling after reading an xl_heap_new_cid record.
 * This implies that a transaction has done some form of write to system
 * catalogs.
 */
void
SnapBuildProcessNewCid(SnapBuild *builder, TransactionId xid, XLogRecPtr lsn, xl_heap_new_cid *xlrec)
{






























}

/*
 * Add a new Snapshot to all transactions we're decoding that currently are
 * in-progress so they can see new catalog contents made by the transaction
 * that just committed. This is necessary because those in-progress
 * transactions will use the new catalog's contents from here on (at the very
 * least everything they do needs to be compatible with newer catalog
 * contents).
 */
static void
SnapBuildDistributeNewCatalogSnapshot(SnapBuild *builder, XLogRecPtr lsn)
{






































}

/*
 * Keep track of a new catalog changing transaction that has committed.
 */
static void
SnapBuildAddCommittedTxn(SnapBuild *builder, TransactionId xid)
{

















}

/*
 * Remove knowledge about transactions we treat as committed and the initial
 * running transactions that are smaller than ->xmin. Those won't ever get
 * checked via the ->committed or InitialRunningXacts array, respectively.
 * The committed xids will get checked via the clog machinery.
 *
 * We can ideally remove the transaction from InitialRunningXacts array
 * once it is finished (committed/aborted) but that could be costly as we need
 * to maintain the xids order in the array.
 */
static void
SnapBuildPurgeOlderTxn(SnapBuild *builder)
{










































































}

/*
 * Handle everything that needs to be done when a transaction commits
 */
void
SnapBuildCommitTxn(SnapBuild *builder, XLogRecPtr lsn, TransactionId xid, int nsubxacts, TransactionId *subxacts)
{





























































































































































}

/* -----------------------------------
 * Snapshot building functions dealing with xlog records
 * -----------------------------------
 */

/*
 * Process a running xacts record, and use its information to first build a
 * historic snapshot and later to release resources that aren't needed
 * anymore.
 */
void
SnapBuildProcessRunningXacts(SnapBuild *builder, XLogRecPtr lsn, xl_running_xacts *running)
{































































































}

/*
 * Build the start of a snapshot that's capable of decoding the catalog.
 *
 * Helper function for SnapBuildProcessRunningXacts() while we're not yet
 * consistent.
 *
 * Returns true if there is a point in performing internal maintenance/cleanup
 * using the xl_running_xacts record.
 */
static bool
SnapBuildFindSnapshot(SnapBuild *builder, XLogRecPtr lsn, xl_running_xacts *running)
{




































































































































































}

/* ---
 * Iterate through xids in record, wait for all older than the cutoff to
 * finish.  Then, if possible, log a new xl_running_xacts record.
 *
 * This isn't required for the correctness of decoding, but to:
 * a) allow isolationtester to notice that we're currently waiting for
 *	  something.
 * b) log a new xl_running_xacts record where it'd be helpful, without having
 *	  to write for bgwriter or checkpointer.
 * ---
 */
static void
SnapBuildWaitSnapshot(xl_running_xacts *running, TransactionId cutoff)
{


































}

/* -----------------------------------
 * Snapshot serialization support
 * -----------------------------------
 */

/*
 * We store current state of struct SnapBuild on disk in the following manner:
 *
 * struct SnapBuildOnDisk;
 * TransactionId * running.xcnt_space;
 * TransactionId * committed.xcnt; (*not xcnt_space*)
 *
 */
typedef struct SnapBuildOnDisk
{
  /* first part of this struct needs to be version independent */

  /* data not covered by checksum */
  uint32 magic;
  pg_crc32c checksum;

  /* data covered by checksum */

  /* version, in case we want to support pg_upgrade */
  uint32 version;
  /* how large is the on disk data, excluding the constant sized part */
  uint32 length;

  /* version dependent part */
  SnapBuild builder;

  /* variable amount of TransactionIds follows */
} SnapBuildOnDisk;

#define SnapBuildOnDiskConstantSize offsetof(SnapBuildOnDisk, builder)
#define SnapBuildOnDiskNotChecksummedSize offsetof(SnapBuildOnDisk, version)

#define SNAPBUILD_MAGIC 0x51A1E001
#define SNAPBUILD_VERSION 2

/*
 * Store/Load a snapshot from disk, depending on the snapshot builder's state.
 *
 * Supposed to be used by external (i.e. not snapbuild.c) code that just read
 * a record that's a potential location for a serialized snapshot.
 */
void
SnapBuildSerializationPoint(SnapBuild *builder, XLogRecPtr lsn)
{








}

/*
 * Serialize the snapshot 'builder' at the location 'lsn' if it hasn't already
 * been done by another decoding process.
 */
static void
SnapBuildSerialize(SnapBuild *builder, XLogRecPtr lsn)
{






























































































































































































}

/*
 * Restore a snapshot into 'builder' if previously one has been stored at the
 * location indicated by 'lsn'. Returns true if successful, false otherwise.
 */
static bool
SnapBuildRestore(SnapBuild *builder, XLogRecPtr lsn)
{























































































































































































































}

/*
 * Remove all serialized snapshots that are not required anymore because no
 * slot can need them. This doesn't actually have to run during a checkpoint,
 * but it's a convenient point to schedule this.
 *
 * NB: We run this during checkpoints even if logical decoding is disabled so
 * we cleanup old slots at some point after it got disabled.
 */
void
CheckPointSnapBuild(void)
{














































































}

/*
 * Mark the transaction as containing catalog changes. In addition, if the
 * given xid is in the list of the initial running xacts, we mark its
 * subtransactions as well. See comments for NInitialRunningXacts and
 * InitialRunningXacts for additional info.
 */
void
SnapBuildXidSetCatalogChanges(SnapBuild *builder, TransactionId xid, int subxcnt, TransactionId *subxacts, XLogRecPtr lsn)
{















}