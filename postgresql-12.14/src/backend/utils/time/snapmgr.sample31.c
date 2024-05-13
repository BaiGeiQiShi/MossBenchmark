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
  Size size;

  size = offsetof(OldSnapshotControlData, xid_by_minute);
  if (old_snapshot_threshold > 0)
  {

  }

  return size;
}

/*
 * Initialize for managing old snapshot detection.
 */
void
SnapMgrInit(void)
{
  bool found;

  /*
   * Create or attach to the OldSnapshotControlData structure.
   */
  oldSnapshotControl = (volatile OldSnapshotControlData *)ShmemInitStruct("OldSnapshotControlData", SnapMgrShmemSize(), &found);

  if (!found)
  {
    SpinLockInit(&oldSnapshotControl->mutex_current);
    oldSnapshotControl->current_timestamp = 0;
    SpinLockInit(&oldSnapshotControl->mutex_latest_xmin);
    oldSnapshotControl->latest_xmin = InvalidTransactionId;
    oldSnapshotControl->next_map_update = 0;
    SpinLockInit(&oldSnapshotControl->mutex_threshold);
    oldSnapshotControl->threshold_timestamp = 0;
    oldSnapshotControl->threshold_xid = InvalidTransactionId;
    oldSnapshotControl->head_offset = 0;
    oldSnapshotControl->head_timestamp = 0;
    oldSnapshotControl->count_used = 0;
  }
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
  /*
   * Return historic snapshot if doing logical decoding. We'll never need a
   * non-historic transaction snapshot in this (sub-)transaction, so there's
   * no need to be careful to set one up for later calls to
   * GetTransactionSnapshot().
   */
  if (HistoricSnapshotActive())
  {


  }

  /* First call in transaction? */
  if (!FirstSnapshotSet)
  {
    /*
     * Don't allow catalog snapshot to be older than xact snapshot.  Must
     * do this first to allow the empty-heap Assert to succeed.
     */
    InvalidateCatalogSnapshot();

    Assert(pairingheap_is_empty(&RegisteredSnapshots));
    Assert(FirstXactSnapshot == NULL);

    if (IsInParallelMode())
    {

    }

    /*
     * In transaction-snapshot mode, the first snapshot must live until
     * end of xact regardless of what the caller does with it, so we must
     * make a copy of it rather than returning CurrentSnapshotData
     * directly.  Furthermore, if we're running in serializable mode,
     * predicate.c needs to wrap the snapshot fetch in its own processing.
     */
    if (IsolationUsesXactSnapshot())
    {
      /* First, create the snapshot in CurrentSnapshotData */
      if (IsolationIsSerializable())
      {
        CurrentSnapshot = GetSerializableTransactionSnapshot(&CurrentSnapshotData);
      }
      else
      {
        CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);
      }
      /* Make a saved copy */
      CurrentSnapshot = CopySnapshot(CurrentSnapshot);
      FirstXactSnapshot = CurrentSnapshot;
      /* Mark it as "registered" in FirstXactSnapshot */
      FirstXactSnapshot->regd_count++;
      pairingheap_add(&RegisteredSnapshots, &FirstXactSnapshot->ph_node);
    }
    else
    {
      CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);
    }

    FirstSnapshotSet = true;
    return CurrentSnapshot;
  }

  if (IsolationUsesXactSnapshot())
  {
    return CurrentSnapshot;
  }

  /* Don't allow catalog snapshot to be older than xact snapshot. */
  InvalidateCatalogSnapshot();

  CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);

  return CurrentSnapshot;
}

/*
 * GetLatestSnapshot
 *		Get a snapshot that is up-to-date as of the current instant,
 *		even if we are executing in transaction-snapshot mode.
 */
Snapshot
GetLatestSnapshot(void)
{
  /*
   * We might be able to relax this, but nothing that could otherwise work
   * needs it.
   */
  if (IsInParallelMode())
  {

  }

  /*
   * So far there are no cases requiring support for GetLatestSnapshot()
   * during logical decoding, but it wouldn't be hard to add if required.
   */
  Assert(!HistoricSnapshotActive());

  /* If first call in transaction, go ahead and set the xact snapshot */
  if (!FirstSnapshotSet)
  {
    return GetTransactionSnapshot();
  }

  SecondarySnapshot = GetSnapshotData(&SecondarySnapshotData);

  return SecondarySnapshot;
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
  Snapshot OldestRegisteredSnapshot = NULL;
  XLogRecPtr RegisteredLSN = InvalidXLogRecPtr;

  if (!pairingheap_is_empty(&RegisteredSnapshots))
  {
    OldestRegisteredSnapshot = pairingheap_container(SnapshotData, ph_node, pairingheap_first(&RegisteredSnapshots));
    RegisteredLSN = OldestRegisteredSnapshot->lsn;
  }

  if (OldestActiveSnapshot != NULL)
  {
    XLogRecPtr ActiveLSN = OldestActiveSnapshot->as_snap->lsn;

    if (XLogRecPtrIsInvalid(RegisteredLSN) || RegisteredLSN > ActiveLSN)
    {
      return OldestActiveSnapshot->as_snap;
    }
  }


}

/*
 * GetCatalogSnapshot
 *		Get a snapshot that is sufficiently up-to-date for scan of the
 *		system catalog with the specified OID.
 */
Snapshot
GetCatalogSnapshot(Oid relid)
{
  /*
   * Return historic snapshot while we're doing logical decoding, so we can
   * see the appropriate state of the catalog.
   *
   * This is the primary reason for needing to reset the system caches after
   * finishing decoding.
   */
  if (HistoricSnapshotActive())
  {

  }

  return GetNonHistoricCatalogSnapshot(relid);
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
  /*
   * If the caller is trying to scan a relation that has no syscache, no
   * catcache invalidations will be sent when it is updated.  For a few key
   * relations, snapshot invalidations are sent instead.  If we're trying to
   * scan a relation for which neither catcache nor snapshot invalidations
   * are sent, we must refresh the snapshot every time.
   */
  if (CatalogSnapshot && !RelationInvalidatesSnapshotsOnly(relid) && !RelationHasSysCache(relid))
  {
    InvalidateCatalogSnapshot();
  }

  if (CatalogSnapshot == NULL)
  {
    /* Get new snapshot. */
    CatalogSnapshot = GetSnapshotData(&CatalogSnapshotData);

    /*
     * Make sure the catalog snapshot will be accounted for in decisions
     * about advancing PGXACT->xmin.  We could apply RegisterSnapshot, but
     * that would result in making a physical copy, which is overkill; and
     * it would also create a dependency on some resource owner, which we
     * do not want for reasons explained at the head of this file. Instead
     * just shove the CatalogSnapshot into the pairing heap manually. This
     * has to be reversed in InvalidateCatalogSnapshot, of course.
     *
     * NB: it had better be impossible for this to throw error, since the
     * CatalogSnapshot pointer is already valid.
     */
    pairingheap_add(&RegisteredSnapshots, &CatalogSnapshot->ph_node);
  }

  return CatalogSnapshot;
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
  if (CatalogSnapshot)
  {
    pairingheap_remove(&RegisteredSnapshots, &CatalogSnapshot->ph_node);
    CatalogSnapshot = NULL;
    SnapshotResetXmin();
  }
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
  if (CatalogSnapshot && ActiveSnapshot == NULL && pairingheap_is_singular(&RegisteredSnapshots))
  {
    InvalidateCatalogSnapshot();
  }
}

/*
 * SnapshotSetCommandId
 *		Propagate CommandCounterIncrement into the static snapshots, if
 *set
 */
void
SnapshotSetCommandId(CommandId curcid)
{
  if (!FirstSnapshotSet)
  {
    return;
  }

  if (CurrentSnapshot)
  {
    CurrentSnapshot->curcid = curcid;
  }
  if (SecondarySnapshot)
  {
    SecondarySnapshot->curcid = curcid;
  }
  /* Should we do the same with CatalogSnapshot? */
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
  /* Caller should have checked this already */
  Assert(!FirstSnapshotSet);

  /* Better do this to ensure following Assert succeeds. */
  InvalidateCatalogSnapshot();

  Assert(pairingheap_is_empty(&RegisteredSnapshots));
  Assert(FirstXactSnapshot == NULL);
  Assert(!HistoricSnapshotActive());

  /*
   * Even though we are not going to use the snapshot it computes, we must
   * call GetSnapshotData, for two reasons: (1) to be sure that
   * CurrentSnapshotData's XID arrays have been allocated, and (2) to update
   * RecentXmin and RecentGlobalXmin.  (We could alternatively include those
   * two variables in exported snapshot files, but it seems better to have
   * snapshot importers compute reasonably up-to-date values for them.)
   */
  CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);

  /*
   * Now copy appropriate fields from the source snapshot.
   */
  CurrentSnapshot->xmin = sourcesnap->xmin;
  CurrentSnapshot->xmax = sourcesnap->xmax;
  CurrentSnapshot->xcnt = sourcesnap->xcnt;
  Assert(sourcesnap->xcnt <= GetMaxSnapshotXidCount());
  if (sourcesnap->xcnt > 0)
  {

  }
  CurrentSnapshot->subxcnt = sourcesnap->subxcnt;
  Assert(sourcesnap->subxcnt <= GetMaxSnapshotSubxidCount());
  if (sourcesnap->subxcnt > 0)
  {

  }
  CurrentSnapshot->suboverflowed = sourcesnap->suboverflowed;
  CurrentSnapshot->takenDuringRecovery = sourcesnap->takenDuringRecovery;
  /* NB: curcid should NOT be copied, it's a local matter */

  /*
   * Now we have to fix what GetSnapshotData did with MyPgXact->xmin and
   * TransactionXmin.  There is a race condition: to make sure we are not
   * causing the global xmin to go backwards, we have to test that the
   * source transaction is still running, and that has to be done
   * atomically. So let procarray.c do it.
   *
   * Note: in serializable mode, predicate.c will do this a second time. It
   * doesn't seem worth contorting the logic here to avoid two calls,
   * especially since it's not clear that predicate.c *must* do this.
   */
  if (sourceproc != NULL)
  {
    if (!ProcArrayInstallRestoredXmin(CurrentSnapshot->xmin, sourceproc))
    {

    }
  }





  /*
   * In transaction-snapshot mode, the first snapshot must live until end of
   * xact, so we must make a copy of it.  Furthermore, if we're running in
   * serializable mode, predicate.c needs to do its own processing.
   */
  if (IsolationUsesXactSnapshot())
  {
    if (IsolationIsSerializable())
    {
      SetSerializableTransactionSnapshot(CurrentSnapshot, sourcevxid, sourcepid);
    }
    /* Make a saved copy */
    CurrentSnapshot = CopySnapshot(CurrentSnapshot);
    FirstXactSnapshot = CurrentSnapshot;
    /* Mark it as "registered" in FirstXactSnapshot */
    FirstXactSnapshot->regd_count++;
    pairingheap_add(&RegisteredSnapshots, &FirstXactSnapshot->ph_node);
  }

  FirstSnapshotSet = true;
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
  Snapshot newsnap;
  Size subxipoff;
  Size size;

  Assert(snapshot != InvalidSnapshot);

  /* We allocate any XID arrays needed in the same palloc block. */
  size = subxipoff = sizeof(SnapshotData) + snapshot->xcnt * sizeof(TransactionId);
  if (snapshot->subxcnt > 0)
  {

  }

  newsnap = (Snapshot)MemoryContextAlloc(TopTransactionContext, size);
  memcpy(newsnap, snapshot, sizeof(SnapshotData));

  newsnap->regd_count = 0;
  newsnap->active_count = 0;
  newsnap->copied = true;

  /* setup XID array */
  if (snapshot->xcnt > 0)
  {
    newsnap->xip = (TransactionId *)(newsnap + 1);
    memcpy(newsnap->xip, snapshot->xip, snapshot->xcnt * sizeof(TransactionId));
  }
  else
  {
    newsnap->xip = NULL;
  }

  /*
   * Setup subXID array. Don't bother to copy it if it had overflowed,
   * though, because it's not used anywhere in that case. Except if it's a
   * snapshot taken during recovery; all the top-level XIDs are in subxip as
   * well in that case, so we mustn't lose them.
   */
  if (snapshot->subxcnt > 0 && (!snapshot->suboverflowed || snapshot->takenDuringRecovery))
  {


  }
  else
  {
    newsnap->subxip = NULL;
  }

  return newsnap;
}

/*
 * FreeSnapshot
 *		Free the memory associated with a snapshot.
 */
static void
FreeSnapshot(Snapshot snapshot)
{
  Assert(snapshot->regd_count == 0);
  Assert(snapshot->active_count == 0);
  Assert(snapshot->copied);

  pfree(snapshot);
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
  PushActiveSnapshotWithLevel(snap, GetCurrentTransactionNestLevel());
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
  ActiveSnapshotElt *newactive;

  Assert(snap != InvalidSnapshot);
  Assert(ActiveSnapshot == NULL || snap_level >= ActiveSnapshot->as_level);

  newactive = MemoryContextAlloc(TopTransactionContext, sizeof(ActiveSnapshotElt));

  /*
   * Checking SecondarySnapshot is probably useless here, but it seems
   * better to be sure.
   */
  if (snap == CurrentSnapshot || snap == SecondarySnapshot || !snap->copied)
  {
    newactive->as_snap = CopySnapshot(snap);
  }
  else
  {
    newactive->as_snap = snap;
  }

  newactive->as_next = ActiveSnapshot;
  newactive->as_level = snap_level;

  newactive->as_snap->active_count++;

  ActiveSnapshot = newactive;
  if (OldestActiveSnapshot == NULL)
  {
    OldestActiveSnapshot = ActiveSnapshot;
  }
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
  PushActiveSnapshot(CopySnapshot(snapshot));
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
  CommandId save_curcid, curcid;

  Assert(ActiveSnapshot != NULL);
  Assert(ActiveSnapshot->as_snap->active_count == 1);
  Assert(ActiveSnapshot->as_snap->regd_count == 0);

  /*
   * Don't allow modification of the active snapshot during parallel
   * operation.  We share the snapshot to worker backends at the beginning
   * of parallel operation, so any change to the snapshot can lead to
   * inconsistencies.  We have other defenses against
   * CommandCounterIncrement, but there are a few places that call this
   * directly, so we put an additional guard here.
   */
  save_curcid = ActiveSnapshot->as_snap->curcid;
  curcid = GetCurrentCommandId(false);
  if (IsInParallelMode() && save_curcid != curcid)
  {

  }
  ActiveSnapshot->as_snap->curcid = curcid;
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
  ActiveSnapshotElt *newstack;

  newstack = ActiveSnapshot->as_next;

  Assert(ActiveSnapshot->as_snap->active_count > 0);

  ActiveSnapshot->as_snap->active_count--;

  if (ActiveSnapshot->as_snap->active_count == 0 && ActiveSnapshot->as_snap->regd_count == 0)
  {
    FreeSnapshot(ActiveSnapshot->as_snap);
  }

  pfree(ActiveSnapshot);
  ActiveSnapshot = newstack;
  if (ActiveSnapshot == NULL)
  {
    OldestActiveSnapshot = NULL;
  }

  SnapshotResetXmin();
}

/*
 * GetActiveSnapshot
 *		Return the topmost snapshot in the Active stack.
 */
Snapshot
GetActiveSnapshot(void)
{
  Assert(ActiveSnapshot != NULL);

  return ActiveSnapshot->as_snap;
}

/*
 * ActiveSnapshotSet
 *		Return whether there is at least one snapshot in the Active
 *stack
 */
bool
ActiveSnapshotSet(void)
{
  return ActiveSnapshot != NULL;
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
  if (snapshot == InvalidSnapshot)
  {
    return InvalidSnapshot;
  }

  return RegisterSnapshotOnOwner(snapshot, CurrentResourceOwner);
}

/*
 * RegisterSnapshotOnOwner
 *		As above, but use the specified resource owner
 */
Snapshot
RegisterSnapshotOnOwner(Snapshot snapshot, ResourceOwner owner)
{
  Snapshot snap;

  if (snapshot == InvalidSnapshot)
  {

  }

  /* Static snapshot?  Create a persistent copy */
  snap = snapshot->copied ? snapshot : CopySnapshot(snapshot);

  /* and tell resowner.c about it */
  ResourceOwnerEnlargeSnapshots(owner);
  snap->regd_count++;
  ResourceOwnerRememberSnapshot(owner, snap);

  if (snap->regd_count == 1)
  {
    pairingheap_add(&RegisteredSnapshots, &snap->ph_node);
  }

  return snap;
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
  if (snapshot == NULL)
  {
    return;
  }

  UnregisterSnapshotFromOwner(snapshot, CurrentResourceOwner);
}

/*
 * UnregisterSnapshotFromOwner
 *		As above, but use the specified resource owner
 */
void
UnregisterSnapshotFromOwner(Snapshot snapshot, ResourceOwner owner)
{
  if (snapshot == NULL)
  {

  }

  Assert(snapshot->regd_count > 0);
  Assert(!pairingheap_is_empty(&RegisteredSnapshots));

  ResourceOwnerForgetSnapshot(owner, snapshot);

  snapshot->regd_count--;
  if (snapshot->regd_count == 0)
  {
    pairingheap_remove(&RegisteredSnapshots, &snapshot->ph_node);
  }

  if (snapshot->regd_count == 0 && snapshot->active_count == 0)
  {
    FreeSnapshot(snapshot);
    SnapshotResetXmin();
  }
}

/*
 * Comparison function for RegisteredSnapshots heap.  Snapshots are ordered
 * by xmin, so that the snapshot with smallest xmin is at the top.
 */
static int
xmin_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{
  const SnapshotData *asnap = pairingheap_const_container(SnapshotData, ph_node, a);
  const SnapshotData *bsnap = pairingheap_const_container(SnapshotData, ph_node, b);

  if (TransactionIdPrecedes(asnap->xmin, bsnap->xmin))
  {
    return 1;
  }
  else if (TransactionIdFollows(asnap->xmin, bsnap->xmin))
  {

  }
  else
  {
    return 0;
  }
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
  Snapshot minSnapshot;

  if (ActiveSnapshot != NULL)
  {
    return;
  }

  if (pairingheap_is_empty(&RegisteredSnapshots))
  {
    MyPgXact->xmin = InvalidTransactionId;
    return;
  }

  minSnapshot = pairingheap_container(SnapshotData, ph_node, pairingheap_first(&RegisteredSnapshots));

  if (TransactionIdPrecedes(MyPgXact->xmin, minSnapshot->xmin))
  {
    MyPgXact->xmin = minSnapshot->xmin;
  }
}

/*
 * AtSubCommit_Snapshot
 */
void
AtSubCommit_Snapshot(int level)
{
  ActiveSnapshotElt *active;

  /*
   * Relabel the active snapshots set in this subtransaction as though they
   * are owned by the parent subxact.
   */
  for (active = ActiveSnapshot; active != NULL; active = active->as_next)
  {
    if (active->as_level < level)
    {
      break;
    }

  }
}

/*
 * AtSubAbort_Snapshot
 *		Clean up snapshots after a subtransaction abort
 */
void
AtSubAbort_Snapshot(int level)
{
  /* Forget the active snapshots set by this subtransaction */
  while (ActiveSnapshot && ActiveSnapshot->as_level >= level)
  {
    ActiveSnapshotElt *next;

    next = ActiveSnapshot->as_next;

    /*
     * Decrement the snapshot's active count.  If it's still registered or
     * marked as active by an outer subtransaction, we can't free it yet.
     */
    Assert(ActiveSnapshot->as_snap->active_count >= 1);
    ActiveSnapshot->as_snap->active_count -= 1;

    if (ActiveSnapshot->as_snap->active_count == 0 && ActiveSnapshot->as_snap->regd_count == 0)
    {
      FreeSnapshot(ActiveSnapshot->as_snap);
    }

    /* and free the stack element */
    pfree(ActiveSnapshot);

    ActiveSnapshot = next;
    if (ActiveSnapshot == NULL)
    {
      OldestActiveSnapshot = NULL;
    }
  }

  SnapshotResetXmin();
}

/*
 * AtEOXact_Snapshot
 *		Snapshot manager's cleanup function for end of transaction
 */
void
AtEOXact_Snapshot(bool isCommit, bool resetXmin)
{
  /*
   * In transaction-snapshot mode we must release our privately-managed
   * reference to the transaction snapshot.  We must remove it from
   * RegisteredSnapshots to keep the check below happy.  But we don't bother
   * to do FreeSnapshot, for two reasons: the memory will go away with
   * TopTransactionContext anyway, and if someone has left the snapshot
   * stacked as active, we don't want the code below to be chasing through a
   * dangling pointer.
   */
  if (FirstXactSnapshot != NULL)
  {
    Assert(FirstXactSnapshot->regd_count > 0);
    Assert(!pairingheap_is_empty(&RegisteredSnapshots));
    pairingheap_remove(&RegisteredSnapshots, &FirstXactSnapshot->ph_node);
  }
  FirstXactSnapshot = NULL;

  /*
   * If we exported any snapshots, clean them up.
   */
  if (exportedSnapshots != NIL)
  {
    ListCell *lc;

    /*
     * Get rid of the files.  Unlink failure is only a WARNING because (1)
     * it's too late to abort the transaction, and (2) leaving a leaked
     * file around has little real consequence anyway.
     *
     * We also need to remove the snapshots from RegisteredSnapshots to
     * prevent a warning below.
     *
     * As with the FirstXactSnapshot, we don't need to free resources of
     * the snapshot iself as it will go away with the memory context.
     */













  }

  /* Drop catalog snapshot if any */
  InvalidateCatalogSnapshot();

  /* On commit, complain about leftover snapshots */
  if (isCommit)
  {
    ActiveSnapshotElt *active;

    if (!pairingheap_is_empty(&RegisteredSnapshots))
    {

    }

    /* complain about unpopped active snapshots */
    for (active = ActiveSnapshot; active != NULL; active = active->as_next)
    {

    }
  }

  /*
   * And reset our state.  We don't need to free the memory explicitly --
   * it'll go away with TopTransactionContext.
   */
  ActiveSnapshot = NULL;
  OldestActiveSnapshot = NULL;
  pairingheap_reset(&RegisteredSnapshots);

  CurrentSnapshot = NULL;
  SecondarySnapshot = NULL;

  FirstSnapshotSet = false;

  /*
   * During normal commit processing, we call ProcArrayEndTransaction() to
   * reset the PgXact->xmin. That call happens prior to the call to
   * AtEOXact_Snapshot(), so we need not touch xmin here at all.
   */
  if (resetXmin)
  {
    SnapshotResetXmin();
  }

  Assert(resetXmin || MyPgXact->xmin == 0);
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
  return (exportedSnapshots != NIL);
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
  if (pairingheap_is_empty(&RegisteredSnapshots) || pairingheap_is_singular(&RegisteredSnapshots))
  {
    return true;
  }


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
  if (TransactionIdIsNormal(recentXmin) && old_snapshot_threshold >= 0 && RelationAllowsEarlyPruning(relation))
  {
    TimestampTz ts = GetSnapshotCurrentTimestamp();
    TransactionId xlimit = recentXmin;
    TransactionId latest_xmin;
    TimestampTz update_ts;
    bool same_ts_as_threshold = false;






    /*
     * Zero threshold always overrides to latest xmin, if valid.  Without
     * some heuristic it will find its own snapshot too old on, for
     * example, a simple UPDATE -- which would make it useless for most
     * testing, but there is no principled way to ensure that it doesn't
     * fail in this way.  Use a five-second delay to try to get useful
     * testing behavior, but this may need adjustment.
     */















    /* Check for fast exit without LW locking. */












































    /*
     * Failsafe protection against vacuuming work of active transaction.
     *
     * This is not an assertion because we avoid the spinlock for
     * performance, leaving open the possibility that xlimit could advance
     * and be more current; but it seems prudent to apply this limit.  It
     * might make pruning a tiny bit less aggressive than it could be, but
     * protects against data loss bugs.
     */









  }

  return recentXmin;
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
  return HistoricSnapshot != NULL;
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
  Size size;

  Assert(snap != InvalidSnapshot);
  Assert(snap->snapshot_type == SNAPSHOT_MVCC);

  /* We allocate any XID arrays needed in the same palloc block. */
  size = add_size(sizeof(SerializedSnapshotData), mul_size(snap->xcnt, sizeof(TransactionId)));
  if (snap->subxcnt > 0 && (!snap->suboverflowed || snap->takenDuringRecovery))
  {

  }

  return size;
}

/*
 * SerializeSnapshot
 *		Dumps the serialized snapshot (extracted from given snapshot)
 *onto the memory location at start_address.
 */
void
SerializeSnapshot(Snapshot snapshot, char *start_address)
{
  SerializedSnapshotData serialized_snapshot;

  Assert(snapshot->subxcnt >= 0);

  /* Copy all required fields */
  serialized_snapshot.xmin = snapshot->xmin;
  serialized_snapshot.xmax = snapshot->xmax;
  serialized_snapshot.xcnt = snapshot->xcnt;
  serialized_snapshot.subxcnt = snapshot->subxcnt;
  serialized_snapshot.suboverflowed = snapshot->suboverflowed;
  serialized_snapshot.takenDuringRecovery = snapshot->takenDuringRecovery;
  serialized_snapshot.curcid = snapshot->curcid;
  serialized_snapshot.whenTaken = snapshot->whenTaken;
  serialized_snapshot.lsn = snapshot->lsn;

  /*
   * Ignore the SubXID array if it has overflowed, unless the snapshot was
   * taken during recovery - in that case, top-level XIDs are in subxip as
   * well, and we mustn't lose them.
   */
  if (serialized_snapshot.suboverflowed && !snapshot->takenDuringRecovery)
  {

  }

  /* Copy struct to possibly-unaligned buffer */
  memcpy(start_address, &serialized_snapshot, sizeof(SerializedSnapshotData));

  /* Copy XID array */
  if (snapshot->xcnt > 0)
  {

  }

  /*
   * Copy SubXID array. Don't bother to copy it if it had overflowed,
   * though, because it's not used anywhere in that case. Except if it's a
   * snapshot taken during recovery; all the top-level XIDs are in subxip as
   * well in that case, so we mustn't lose them.
   */
  if (serialized_snapshot.subxcnt > 0)
  {
    Size subxipoff = sizeof(SerializedSnapshotData) + snapshot->xcnt * sizeof(TransactionId);


  }
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
  SerializedSnapshotData serialized_snapshot;
  Size size;
  Snapshot snapshot;
  TransactionId *serialized_xids;

  memcpy(&serialized_snapshot, start_address, sizeof(SerializedSnapshotData));
  serialized_xids = (TransactionId *)(start_address + sizeof(SerializedSnapshotData));

  /* We allocate any XID arrays needed in the same palloc block. */
  size = sizeof(SnapshotData) + serialized_snapshot.xcnt * sizeof(TransactionId) + serialized_snapshot.subxcnt * sizeof(TransactionId);

  /* Copy all required fields */
  snapshot = (Snapshot)MemoryContextAlloc(TopTransactionContext, size);
  snapshot->snapshot_type = SNAPSHOT_MVCC;
  snapshot->xmin = serialized_snapshot.xmin;
  snapshot->xmax = serialized_snapshot.xmax;
  snapshot->xip = NULL;
  snapshot->xcnt = serialized_snapshot.xcnt;
  snapshot->subxip = NULL;
  snapshot->subxcnt = serialized_snapshot.subxcnt;
  snapshot->suboverflowed = serialized_snapshot.suboverflowed;
  snapshot->takenDuringRecovery = serialized_snapshot.takenDuringRecovery;
  snapshot->curcid = serialized_snapshot.curcid;
  snapshot->whenTaken = serialized_snapshot.whenTaken;
  snapshot->lsn = serialized_snapshot.lsn;

  /* Copy XIDs, if present. */
  if (serialized_snapshot.xcnt > 0)
  {


  }

  /* Copy SubXIDs, if present. */
  if (serialized_snapshot.subxcnt > 0)
  {


  }

  /* Set the copied flag so that the caller will set refcounts correctly. */
  snapshot->regd_count = 0;
  snapshot->active_count = 0;
  snapshot->copied = true;

  return snapshot;
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
  SetTransactionSnapshot(snapshot, NULL, InvalidPid, master_pgproc);
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
  uint32 i;

  /*
   * Make a quick range check to eliminate most XIDs without looking at the
   * xip arrays.  Note that this is OK even if we convert a subxact XID to
   * its parent below, because a subxact with XID < xmin has surely also got
   * a parent with XID < xmin, while one with XID >= xmax must belong to a
   * parent that was not yet committed at the time of this snapshot.
   */

  /* Any xid < xmin is not in-progress */
  if (TransactionIdPrecedes(xid, snapshot->xmin))
  {
    return false;
  }
  /* Any xid >= xmax is in-progress */
  if (TransactionIdFollowsOrEquals(xid, snapshot->xmax))
  {
    return true;
  }

  /*
   * Snapshot information is stored slightly differently in snapshots taken
   * during recovery.
   */
  if (!snapshot->takenDuringRecovery)
  {
    /*
     * If the snapshot contains full subxact data, the fastest way to
     * check things is just to compare the given XID against both subxact
     * XIDs and top-level XIDs.  If the snapshot overflowed, we have to
     * use pg_subtrans to convert a subxact XID to its parent XID, but
     * then we need only look at top-level XIDs not subxacts.
     */
    if (!snapshot->suboverflowed)
    {
      /* we have full data, so search subxip */
      int32 j;

      for (j = 0; j < snapshot->subxcnt; j++)
      {




      }

      /* not there, fall through to search xip[] */
    }
    else
    {
      /*
       * Snapshot overflowed, so convert xid to top-level.  This is safe
       * because we eliminated too-old XIDs above.
       */


      /*
       * If xid was indeed a subxact, we might now have an xid < xmin,
       * so recheck to avoid an array scan.  No point in rechecking
       * xmax.
       */




    }

    for (i = 0; i < snapshot->xcnt; i++)
    {
      if (TransactionIdEquals(xid, snapshot->xip[i]))
      {
        return true;
      }
    }
  }
  else
  {
    int32 j;

    /*
     * In recovery we store all xids in the subxact array because it is by
     * far the bigger array, and we mostly don't know which xids are
     * top-level and which are subxacts. The xip array is empty.
     *
     * We start by searching subtrans, if we overflowed.
     */



















    /*
     * We now have either a top-level xid higher than xmin or an
     * indeterminate xid. We don't know whether it's top level or subxact
     * but it doesn't matter. If it's present, the xid is visible.
     */







  }

  return false;
}