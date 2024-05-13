/*-------------------------------------------------------------------------
 *
 * snapmgr.c
 *		PostgreSQL snapshot manager
 *
 * We keep track of snapshots in two ways: those "registered" by resowner.c,
 * and the "active snapshot" stack.  All snapshots in either of them live in
 * persistent memory.  When a snapshot is no longer in any of these lists
 * (tracked by separate refcounts on each snapshot), its memory can be freed.
 *
 * The FirstXactSnapshot, if any, is treated a bit specially: we increment its
 * regd_count and list it in RegisteredSnapshots, but this reference is not
 * tracked by a resource owner. We used to use the TopTransactionResourceOwner
 * to track this snapshot reference, but that introduces logical circularity
 * and thus makes it impossible to clean up in a sane fashion.  It's better to
 * handle this reference as an internally-tracked registration, so that this
 * module is entirely lower-level than ResourceOwners.
 *
 * Likewise, any snapshots that have been exported by pg_export_snapshot
 * have regd_count = 1 and are listed in RegisteredSnapshots, but are not
 * tracked by any resource owner.
 *
 * Likewise, the CatalogSnapshot is listed in RegisteredSnapshots when it
 * is valid, but is not tracked by any resource owner.
 *
 * The same is true for historic snapshots used during logical decoding,
 * their lifetime is managed separately (as they live longer than one xact.c
 * transaction).
 *
 * These arrangements let us reset MyPgXact->xmin when there are no snapshots
 * referenced by this transaction, and advance it when the one with oldest
 * Xmin is no longer referenced.  For simplicity however, only registered
 * snapshots not active snapshots participate in tracking which one is oldest;
 * we don't try to change MyPgXact->xmin except when the active-snapshot
 * stack is empty.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/time/snapmgr.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "lib/pairingheap.h"
#include "miscadmin.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinval.h"
#include "storage/sinvaladt.h"
#include "storage/spin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/resowner_private.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/*
 * GUC parameters
 */
int old_snapshot_threshold; /* number of minutes, -1 disables */

/*
 * Structure for dealing with old_snapshot_threshold implementation.
 */
typedef struct OldSnapshotControlData
{
  /*
   * Variables for old snapshot handling are shared among processes and are
   * only allowed to move forward.
   */
  slock_t mutex_current;           /* protect current_timestamp */
  TimestampTz current_timestamp;   /* latest snapshot timestamp */
  slock_t mutex_latest_xmin;       /* protect latest_xmin and next_map_update */
  TransactionId latest_xmin;       /* latest snapshot xmin */
  TimestampTz next_map_update;     /* latest snapshot valid up to */
  slock_t mutex_threshold;         /* protect threshold fields */
  TimestampTz threshold_timestamp; /* earlier snapshot is old */
  TransactionId threshold_xid;     /* earlier xid may be gone */

  /*
   * Keep one xid per minute for old snapshot error handling.
   *
   * Use a circular buffer with a head offset, a count of entries currently
   * used, and a timestamp corresponding to the xid at the head offset.  A
   * count_used value of zero means that there are no times stored; a
   * count_used value of OLD_SNAPSHOT_TIME_MAP_ENTRIES means that the buffer
   * is full and the head must be advanced to add new entries.  Use
   * timestamps aligned to minute boundaries, since that seems less
   * surprising than aligning based on the first usage timestamp.  The
   * latest bucket is effectively stored within latest_xmin.  The circular
   * buffer is updated when we get a new xmin value that doesn't fall into
   * the same interval.
   *
   * It is OK if the xid for a given time slot is from earlier than
   * calculated by adding the number of minutes corresponding to the
   * (possibly wrapped) distance from the head offset to the time of the
   * head entry, since that just results in the vacuuming of old tuples
   * being slightly less aggressive.  It would not be OK for it to be off in
   * the other direction, since it might result in vacuuming tuples that are
   * still expected to be there.
   *
   * Use of an SLRU was considered but not chosen because it is more
   * heavyweight than is needed for this, and would probably not be any less
   * code to implement.
   *
   * Persistence is not needed.
   */
  int head_offset;            /* subscript of oldest tracked time */
  TimestampTz head_timestamp; /* time corresponding to head xid */
  int count_used;             /* how many slots are in use */
  TransactionId xid_by_minute[FLEXIBLE_ARRAY_MEMBER];
} OldSnapshotControlData;

static volatile OldSnapshotControlData *oldSnapshotControl;

/*
 * CurrentSnapshot points to the only snapshot taken in transaction-snapshot
 * mode, and to the latest one taken in a read-committed transaction.
 * SecondarySnapshot is a snapshot that's always up-to-date as of the current
 * instant, even in transaction-snapshot mode.  It should only be used for
 * special-purpose code (say, RI checking.)  CatalogSnapshot points to an
 * MVCC snapshot intended to be used for catalog scans; we must invalidate it
 * whenever a system catalog change occurs.
 *
 * These SnapshotData structs are static to simplify memory allocation
 * (see the hack in GetSnapshotData to avoid repeated malloc/free).
 */
static SnapshotData CurrentSnapshotData = {SNAPSHOT_MVCC};
static SnapshotData SecondarySnapshotData = {SNAPSHOT_MVCC};
SnapshotData CatalogSnapshotData = {SNAPSHOT_MVCC};
SnapshotData SnapshotSelfData = {SNAPSHOT_SELF};
SnapshotData SnapshotAnyData = {SNAPSHOT_ANY};

/* Pointers to valid snapshots */
static Snapshot CurrentSnapshot = NULL;
static Snapshot SecondarySnapshot = NULL;
static Snapshot CatalogSnapshot = NULL;
static Snapshot HistoricSnapshot = NULL;

/*
 * These are updated by GetSnapshotData.  We initialize them this way
 * for the convenience of TransactionIdIsInProgress: even in bootstrap
 * mode, we don't want it to say that BootstrapTransactionId is in progress.
 *
 * RecentGlobalXmin and RecentGlobalDataXmin are initialized to
 * InvalidTransactionId, to ensure that no one tries to use a stale
 * value. Readers should ensure that it has been set to something else
 * before using it.
 */
TransactionId TransactionXmin = FirstNormalTransactionId;
TransactionId RecentXmin = FirstNormalTransactionId;
TransactionId RecentGlobalXmin = InvalidTransactionId;
TransactionId RecentGlobalDataXmin = InvalidTransactionId;

/* (table, ctid) => (cmin, cmax) mapping during timetravel */
static HTAB *tuplecid_data = NULL;

/*
 * Elements of the active snapshot stack.
 *
 * Each element here accounts for exactly one active_count on SnapshotData.
 *
 * NB: the code assumes that elements in this list are in non-increasing
 * order of as_level; also, the list must be NULL-terminated.
 */
typedef struct ActiveSnapshotElt
{
  Snapshot as_snap;
  int as_level;
  struct ActiveSnapshotElt *as_next;
} ActiveSnapshotElt;

/* Top of the stack of active snapshots */
static ActiveSnapshotElt *ActiveSnapshot = NULL;

/* Bottom of the stack of active snapshots */
static ActiveSnapshotElt *OldestActiveSnapshot = NULL;

/*
 * Currently registered Snapshots.  Ordered in a heap by xmin, so that we can
 * quickly find the one with lowest xmin, to advance our MyPgXact->xmin.
 */
static int
xmin_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg);

static pairingheap RegisteredSnapshots = {&xmin_cmp, NULL, NULL};

/* first GetTransactionSnapshot call in a transaction? */
bool FirstSnapshotSet = false;

/*
 * Remember the serializable transaction snapshot, if any.  We cannot trust
 * FirstSnapshotSet in combination with IsolationUsesXactSnapshot(), because
 * GUC may be reset before us, changing the value of IsolationUsesXactSnapshot.
 */
static Snapshot FirstXactSnapshot = NULL;

/* Define pathname of exported-snapshot files */
#define SNAPSHOT_EXPORT_DIR "pg_snapshots"

/* Structure holding info about exported snapshot. */
typedef struct ExportedSnapshot
{
  char *snapfile;
  Snapshot snapshot;
} ExportedSnapshot;

/* Current xact's exported snapshots (a list of ExportedSnapshot structs) */
static List *exportedSnapshots = NIL;

/* Prototypes for local functions */
static TimestampTz
AlignTimestampToMinuteBoundary(TimestampTz ts);
static Snapshot
CopySnapshot(Snapshot snapshot);
static void
FreeSnapshot(Snapshot snapshot);
static void
SnapshotResetXmin(void);

/*
 * Snapshot fields to be serialized.
 *
 * Only these fields need to be sent to the cooperating backend; the
 * remaining ones can (and must) be set by the receiver upon restore.
 */
typedef struct SerializedSnapshotData
{
  TransactionId xmin;
  TransactionId xmax;
  uint32 xcnt;
  int32 subxcnt;
  bool suboverflowed;
  bool takenDuringRecovery;
  CommandId curcid;
  TimestampTz whenTaken;
  XLogRecPtr lsn;
} SerializedSnapshotData;

Size
SnapMgrShmemSize(void)
{









}

/*
 * Initialize for managing old snapshot detection.
 */
void
SnapMgrInit(void)
{





















}

/*
 * GetTransactionSnapshot
 *		Get the appropriate snapshot for a new query in a transaction.
 *
 * Note that the return value may point at static storage that will be modified
 * by future calls and by CommandCounterIncrement().  Callers should call
 * RegisterSnapshot or PushActiveSnapshot on the returned snap if it is to be
 * used very long.
 */
Snapshot
GetTransactionSnapshot(void)
{










































































}

/*
 * GetLatestSnapshot
 *		Get a snapshot that is up-to-date as of the current instant,
 *		even if we are executing in transaction-snapshot mode.
 */
Snapshot
GetLatestSnapshot(void)
{
























}

/*
 * GetOldestSnapshot
 *
 *		Get the transaction's oldest known snapshot, as judged by the
 *LSN. Will return NULL if there are no active or registered snapshots.
 */
Snapshot
GetOldestSnapshot(void)
{




















}

/*
 * GetCatalogSnapshot
 *		Get a snapshot that is sufficiently up-to-date for scan of the
 *		system catalog with the specified OID.
 */
Snapshot
GetCatalogSnapshot(Oid relid)
{













}

/*
 * GetNonHistoricCatalogSnapshot
 *		Get a snapshot that is sufficiently up-to-date for scan of the
 *system catalog with the specified OID, even while historic snapshots are set
 *		up.
 */
Snapshot
GetNonHistoricCatalogSnapshot(Oid relid)
{

































}

/*
 * InvalidateCatalogSnapshot
 *		Mark the current catalog snapshot, if any, as invalid
 *
 * We could change this API to allow the caller to provide more fine-grained
 * invalidation details, so that a change to relation A wouldn't prevent us
 * from using our cached snapshot to scan relation B, but so far there's no
 * evidence that the CPU cycles we spent tracking such fine details would be
 * well-spent.
 */
void
InvalidateCatalogSnapshot(void)
{






}

/*
 * InvalidateCatalogSnapshotConditionally
 *		Drop catalog snapshot if it's the only one we have
 *
 * This is called when we are about to wait for client input, so we don't
 * want to continue holding the catalog snapshot if it might mean that the
 * global xmin horizon can't advance.  However, if there are other snapshots
 * still active or registered, the catalog snapshot isn't likely to be the
 * oldest one, so we might as well keep it.
 */
void
InvalidateCatalogSnapshotConditionally(void)
{




}

/*
 * SnapshotSetCommandId
 *		Propagate CommandCounterIncrement into the static snapshots, if
 *set
 */
void
SnapshotSetCommandId(CommandId curcid)
{














}

/*
 * SetTransactionSnapshot
 *		Set the transaction's snapshot from an imported MVCC snapshot.
 *
 * Note that this is very closely tied to GetTransactionSnapshot --- it
 * must take care of all the same considerations as the first-snapshot case
 * in GetTransactionSnapshot.
 */
static void
SetTransactionSnapshot(Snapshot sourcesnap, VirtualTransactionId *sourcevxid, int sourcepid, PGPROC *sourceproc)
{




















































































}

/*
 * CopySnapshot
 *		Copy the given snapshot.
 *
 * The copy is palloc'd in TopTransactionContext and has initial refcounts set
 * to 0.  The returned snapshot has the copied flag set.
 */
static Snapshot
CopySnapshot(Snapshot snapshot)
{
















































}

/*
 * FreeSnapshot
 *		Free the memory associated with a snapshot.
 */
static void
FreeSnapshot(Snapshot snapshot)
{





}

/*
 * PushActiveSnapshot
 *		Set the given snapshot as the current active snapshot
 *
 * If the passed snapshot is a statically-allocated one, or it is possibly
 * subject to a future command counter update, create a new long-lived copy
 * with active refcount=1.  Otherwise, only increment the refcount.
 */
void
PushActiveSnapshot(Snapshot snap)
{

}

/*
 * PushActiveSnapshotWithLevel
 *		Set the given snapshot as the current active snapshot
 *
 * Same as PushActiveSnapshot except that caller can specify the
 * transaction nesting level that "owns" the snapshot.  This level
 * must not be deeper than the current top of the snapshot stack.
 */
void
PushActiveSnapshotWithLevel(Snapshot snap, int snap_level)
{






























}

/*
 * PushCopiedSnapshot
 *		As above, except forcibly copy the presented snapshot.
 *
 * This should be used when the ActiveSnapshot has to be modifiable, for
 * example if the caller intends to call UpdateActiveSnapshotCommandId.
 * The new snapshot will be released when popped from the stack.
 */
void
PushCopiedSnapshot(Snapshot snapshot)
{

}

/*
 * UpdateActiveSnapshotCommandId
 *
 * Update the current CID of the active snapshot.  This can only be applied
 * to a snapshot that is not referenced elsewhere.
 */
void
UpdateActiveSnapshotCommandId(void)
{





















}

/*
 * PopActiveSnapshot
 *
 * Remove the topmost snapshot from the active snapshot stack, decrementing the
 * reference count, and free it if this was the last reference.
 */
void
PopActiveSnapshot(void)
{





















}

/*
 * GetActiveSnapshot
 *		Return the topmost snapshot in the Active stack.
 */
Snapshot
GetActiveSnapshot(void)
{



}

/*
 * ActiveSnapshotSet
 *		Return whether there is at least one snapshot in the Active
 *stack
 */
bool
ActiveSnapshotSet(void)
{

}

/*
 * RegisterSnapshot
 *		Register a snapshot as being in use by the current resource
 *owner
 *
 * If InvalidSnapshot is passed, it is not registered.
 */
Snapshot
RegisterSnapshot(Snapshot snapshot)
{






}

/*
 * RegisterSnapshotOnOwner
 *		As above, but use the specified resource owner
 */
Snapshot
RegisterSnapshotOnOwner(Snapshot snapshot, ResourceOwner owner)
{





















}

/*
 * UnregisterSnapshot
 *
 * Decrement the reference count of a snapshot, remove the corresponding
 * reference from CurrentResourceOwner, and free the snapshot if no more
 * references remain.
 */
void
UnregisterSnapshot(Snapshot snapshot)
{






}

/*
 * UnregisterSnapshotFromOwner
 *		As above, but use the specified resource owner
 */
void
UnregisterSnapshotFromOwner(Snapshot snapshot, ResourceOwner owner)
{





















}

/*
 * Comparison function for RegisteredSnapshots heap.  Snapshots are ordered
 * by xmin, so that the snapshot with smallest xmin is at the top.
 */
static int
xmin_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{















}

/*
 * Get current RecentGlobalXmin value, as a FullTransactionId.
 */
FullTransactionId
GetFullRecentGlobalXmin(void)
{


























}

/*
 * SnapshotResetXmin
 *
 * If there are no more snapshots, we can reset our PGXACT->xmin to InvalidXid.
 * Note we can do this without locking because we assume that storing an Xid
 * is atomic.
 *
 * Even if there are some remaining snapshots, we may be able to advance our
 * PGXACT->xmin to some degree.  This typically happens when a portal is
 * dropped.  For efficiency, we only consider recomputing PGXACT->xmin when
 * the active snapshot stack is empty; this allows us not to need to track
 * which active snapshot is oldest.
 *
 * Note: it's tempting to use GetOldestSnapshot() here so that we can include
 * active snapshots in the calculation.  However, that compares by LSN not
 * xmin so it's not entirely clear that it's the same thing.  Also, we'd be
 * critically dependent on the assumption that the bottommost active snapshot
 * stack entry has the oldest xmin.  (Current uses of GetOldestSnapshot() are
 * not actually critical, but this would be.)
 */
static void
SnapshotResetXmin(void)
{



















}

/*
 * AtSubCommit_Snapshot
 */
void
AtSubCommit_Snapshot(int level)
{














}

/*
 * AtSubAbort_Snapshot
 *		Clean up snapshots after a subtransaction abort
 */
void
AtSubAbort_Snapshot(int level)
{






























}

/*
 * AtEOXact_Snapshot
 *		Snapshot manager's cleanup function for end of transaction
 */
void
AtEOXact_Snapshot(bool isCommit, bool resetXmin)
{






























































































}

/*
 * ExportSnapshot
 *		Export the snapshot to a file so that other backends can import
 *it. Returns the token (the file name) that can be used to import this
 *		snapshot.
 */
char *
ExportSnapshot(Snapshot snapshot)
{













































































































































































}

/*
 * pg_export_snapshot
 *		SQL-callable wrapper for ExportSnapshot.
 */
Datum
pg_export_snapshot(PG_FUNCTION_ARGS)
{




}

/*
 * Parsing subroutines for ImportSnapshot: parse a line with the given
 * prefix followed by a value, and advance *s to the next line.  The
 * filename is provided for use in error messages.
 */
static int
parseIntFromText(const char *prefix, char **s, const char *filename)
{




















}

static TransactionId
parseXidFromText(const char *prefix, char **s, const char *filename)
{




















}

static void
parseVxidFromText(const char *prefix, char **s, const char *filename, VirtualTransactionId *vxid)
{


















}

/*
 * ImportSnapshot
 *		Import a previously exported snapshot.  The argument should be a
 *		filename in SNAPSHOT_EXPORT_DIR.  Load the snapshot from that
 *file. This is called by "SET TRANSACTION SNAPSHOT 'foo'".
 */
void
ImportSnapshot(const char *idstr)
{









































































































































































}

/*
 * XactHasExportedSnapshots
 *		Test whether current transaction has exported any snapshots.
 */
bool
XactHasExportedSnapshots(void)
{

}

/*
 * DeleteAllExportedSnapshotFiles
 *		Clean up any files that have been left behind by a crashed
 *backend that had exported snapshots before it died.
 *
 * This should be called during database startup or crash recovery.
 */
void
DeleteAllExportedSnapshotFiles(void)
{



























}

/*
 * ThereAreNoPriorRegisteredSnapshots
 *		Is the registered snapshot count less than or equal to one?
 *
 * Don't use this to settle important decisions.  While zero registrations and
 * no ActiveSnapshot would confirm a certain idleness, the system makes no
 * guarantees about the significance of one registered snapshot.
 */
bool
ThereAreNoPriorRegisteredSnapshots(void)
{






}

/*
 * Return a timestamp that is exactly on a minute boundary.
 *
 * If the argument is already aligned, return that value, otherwise move to
 * the next minute boundary following the given time.
 */
static TimestampTz
AlignTimestampToMinuteBoundary(TimestampTz ts)
{



}

/*
 * Get current timestamp for snapshots
 *
 * This is basically GetCurrentTimestamp(), but with a guarantee that
 * the result never moves backward.
 */
TimestampTz
GetSnapshotCurrentTimestamp(void)
{

















}

/*
 * Get timestamp through which vacuum may have processed based on last stored
 * value for threshold_timestamp.
 *
 * XXX: So far, we never trust that a 64-bit value can be read atomically; if
 * that ever changes, we could get rid of the spinlock here.
 */
TimestampTz
GetOldSnapshotThresholdTimestamp(void)
{







}

static void
SetOldSnapshotThresholdTimestamp(TimestampTz ts, TransactionId xlimit)
{




}

/*
 * TransactionIdLimitedForOldSnapshots
 *
 * Apply old snapshot limit, if any.  This is intended to be called for page
 * pruning and table vacuuming, to allow old_snapshot_threshold to override
 * the normal global xmin value.  Actual testing for snapshot too old will be
 * based on whether a snapshot timestamp is prior to the threshold timestamp
 * set in this function.
 */
TransactionId
TransactionIdLimitedForOldSnapshots(TransactionId recentXmin, Relation relation)
{






































































































}

/*
 * Take care of the circular buffer that maps time to xid.
 */
void
MaintainOldSnapshotTimeMapping(TimestampTz whenTaken, TransactionId xmin)
{











































































































































}

/*
 * Setup a snapshot that replaces normal catalog snapshots that allows catalog
 * access to behave just like it did at a certain point in the past.
 *
 * Needed for logical decoding.
 */
void
SetupHistoricSnapshot(Snapshot historic_snapshot, HTAB *tuplecids)
{







}

/*
 * Make catalog snapshots behave normally again.
 */
void
TeardownHistoricSnapshot(bool is_error)
{


}

bool
HistoricSnapshotActive(void)
{

}

HTAB *
HistoricSnapshotGetTupleCids(void)
{


}

/*
 * EstimateSnapshotSpace
 *		Returns the size needed to store the given snapshot.
 *
 * We are exporting only required fields from the Snapshot, stored in
 * SerializedSnapshotData.
 */
Size
EstimateSnapshotSpace(Snapshot snap)
{













}

/*
 * SerializeSnapshot
 *		Dumps the serialized snapshot (extracted from given snapshot)
 *onto the memory location at start_address.
 */
void
SerializeSnapshot(Snapshot snapshot, char *start_address)
{














































}

/*
 * RestoreSnapshot
 *		Restore a serialized snapshot from the specified address.
 *
 * The copy is palloc'd in TopTransactionContext and has initial refcounts set
 * to 0.  The returned snapshot has the copied flag set.
 */
Snapshot
RestoreSnapshot(char *start_address)
{














































}

/*
 * Install a restored snapshot as the transaction snapshot.
 *
 * The second argument is of type void * so that snapmgr.h need not include
 * the declaration for PGPROC.
 */
void
RestoreTransactionSnapshot(Snapshot snapshot, void *master_pgproc)
{

}

/*
 * XidInMVCCSnapshot
 *		Is the given XID still-in-progress according to the snapshot?
 *
 * Note: GetSnapshotData never stores either top xid or subxids of our own
 * backend into a snapshot, so these xids will not be reported as "running"
 * by this function.  This is OK for current uses, because we always check
 * TransactionIdIsCurrentTransactionId first, except when it's known the
 * XID could not be ours anyway.
 */
bool
XidInMVCCSnapshot(TransactionId xid, Snapshot snapshot)
{

























































































































}