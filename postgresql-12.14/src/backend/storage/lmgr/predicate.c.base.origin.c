/*-------------------------------------------------------------------------
 *
 * predicate.c
 *	  POSTGRES predicate locking
 *	  to support full serializable transaction isolation
 *
 *
 * The approach taken is to implement Serializable Snapshot Isolation (SSI)
 * as initially described in this paper:
 *
 *	Michael J. Cahill, Uwe Röhm, and Alan D. Fekete. 2008.
 *	Serializable isolation for snapshot databases.
 *	In SIGMOD '08: Proceedings of the 2008 ACM SIGMOD
 *	international conference on Management of data,
 *	pages 729-738, New York, NY, USA. ACM.
 *	http://doi.acm.org/10.1145/1376616.1376690
 *
 * and further elaborated in Cahill's doctoral thesis:
 *
 *	Michael James Cahill. 2009.
 *	Serializable Isolation for Snapshot Databases.
 *	Sydney Digital Theses.
 *	University of Sydney, School of Information Technologies.
 *	http://hdl.handle.net/2123/5353
 *
 *
 * Predicate locks for Serializable Snapshot Isolation (SSI) are SIREAD
 * locks, which are so different from normal locks that a distinct set of
 * structures is required to handle them.  They are needed to detect
 * rw-conflicts when the read happens before the write.  (When the write
 * occurs first, the reading transaction can check for a conflict by
 * examining the MVCC data.)
 *
 * (1)	Besides tuples actually read, they must cover ranges of tuples
 *		which would have been read based on the predicate.  This will
 *		require modelling the predicates through locks against database
 *		objects such as pages, index ranges, or entire tables.
 *
 * (2)	They must be kept in RAM for quick access.  Because of this, it
 *		isn't possible to always maintain tuple-level granularity --
 *when the space allocated to store these approaches exhaustion, a request for a
 *lock may need to scan for situations where a single transaction holds many
 *fine-grained locks which can be coalesced into a single coarser-grained lock.
 *
 * (3)	They never block anything; they are more like flags than locks
 *		in that regard; although they refer to database objects and are
 *		used to identify rw-conflicts with normal write locks.
 *
 * (4)	While they are associated with a transaction, they must survive
 *		a successful COMMIT of that transaction, and remain until all
 *		overlapping transactions complete.  This even means that they
 *		must survive termination of the transaction's process.  If a
 *		top level transaction is rolled back, however, it is immediately
 *		flagged so that it can be ignored, and its SIREAD locks can be
 *		released any time after that.
 *
 * (5)	The only transactions which create SIREAD locks or check for
 *		conflicts with them are serializable transactions.
 *
 * (6)	When a write lock for a top level transaction is found to cover
 *		an existing SIREAD lock for the same transaction, the SIREAD
 *lock can be deleted.
 *
 * (7)	A write from a serializable transaction must ensure that an xact
 *		record exists for the transaction, with the same lifespan (until
 *		all concurrent transaction complete or the transaction is rolled
 *		back) so that rw-dependencies to that transaction can be
 *		detected.
 *
 * We use an optimization for read-only transactions. Under certain
 * circumstances, a read-only transaction's snapshot can be shown to
 * never have conflicts with other transactions.  This is referred to
 * as a "safe" snapshot (and one known not to be is "unsafe").
 * However, it can't be determined whether a snapshot is safe until
 * all concurrent read/write transactions complete.
 *
 * Once a read-only transaction is known to have a safe snapshot, it
 * can release its predicate locks and exempt itself from further
 * predicate lock tracking. READ ONLY DEFERRABLE transactions run only
 * on safe snapshots, waiting as necessary for one to be available.
 *
 *
 * Lightweight locks to manage access to the predicate locking shared
 * memory objects must be taken in this order, and should be released in
 * reverse order:
 *
 *	SerializableFinishedListLock
 *		- Protects the list of transactions which have completed but
 *which may yet matter because they overlap still-active transactions.
 *
 *	SerializablePredicateLockListLock
 *		- Protects the linked list of locks held by a transaction.  Note
 *			that the locks themselves are also covered by the
 *partition locks of their respective lock targets; this lock only affects the
 *linked list connecting the locks related to a transaction.
 *		- All transactions share this single lock (with no
 *partitioning).
 *		- There is never a need for a process other than the one running
 *			an active transaction to walk the list of locks held by
 *that transaction, except parallel query workers sharing the leader's
 *			transaction.  In the parallel case, an extra per-sxact
 *lock is taken; see below.
 *		- It is relatively infrequent that another process needs to
 *			modify the list for a transaction, but it does happen
 *for such things as index page splits for pages with predicate locks and
 *			freeing of predicate locked pages by a vacuum process.
 *When removing a lock in such cases, the lock itself contains the pointers
 *needed to remove it from the list.  When adding a lock in such cases, the lock
 *can be added using the anchor in the transaction structure.  Neither requires
 *walking the list.
 *		- Cleaning up the list for a terminated transaction is sometimes
 *			not done on a retail basis, in which case no lock is
 *required.
 *		- Due to the above, a process accessing its active transaction's
 *			list always uses a shared lock, regardless of whether it
 *is walking or maintaining the list.  This improves concurrency for the common
 *access patterns.
 *		- A process which needs to alter the list of a transaction other
 *			than its own active transaction must acquire an
 *exclusive lock.
 *
 *	SERIALIZABLEXACT's member 'predicateLockListLock'
 *		- Protects the linked list of locks held by a transaction.  Only
 *			needed for parallel mode, where multiple backends share
 *the same SERIALIZABLEXACT object.  Not needed if
 *			SerializablePredicateLockListLock is held exclusively.
 *
 *	PredicateLockHashPartitionLock(hashcode)
 *		- The same lock protects a target, all locks on that target, and
 *			the linked list of locks on the target.
 *		- When more than one is needed, acquire in ascending address
 *order.
 *		- When all are needed (rare), acquire in ascending index order
 *with PredicateLockHashPartitionLockByIndex(index).
 *
 *	SerializableXactHashLock
 *		- Protects both PredXact and SerializableXidHash.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/lmgr/predicate.c
 *
 *-------------------------------------------------------------------------
 */
/*
 * INTERFACE ROUTINES
 *
 * housekeeping for setting up shared memory predicate lock structures
 *		InitPredicateLocks(void)
 *		PredicateLockShmemSize(void)
 *
 * predicate lock reporting
 *		GetPredicateLockStatusData(void)
 *		PageIsPredicateLocked(Relation relation, BlockNumber blkno)
 *
 * predicate lock maintenance
 *		GetSerializableTransactionSnapshot(Snapshot snapshot)
 *		SetSerializableTransactionSnapshot(Snapshot snapshot,
 *										   VirtualTransactionId
 **sourcevxid) RegisterPredicateLockingXid(void) PredicateLockRelation(Relation
 *relation, Snapshot snapshot) PredicateLockPage(Relation relation, BlockNumber
 *blkno, Snapshot snapshot) PredicateLockTuple(Relation relation, HeapTuple
 *tuple, Snapshot snapshot) PredicateLockPageSplit(Relation relation,
 *BlockNumber oldblkno, BlockNumber newblkno) PredicateLockPageCombine(Relation
 *relation, BlockNumber oldblkno, BlockNumber newblkno)
 *		TransferPredicateLocksToHeapRelation(Relation relation)
 *		ReleasePredicateLocks(bool isCommit, bool isReadOnlySafe)
 *
 * conflict detection (may also trigger rollback)
 *		CheckForSerializableConflictOut(bool visible, Relation relation,
 *										HeapTupleData
 **tup, Buffer buffer, Snapshot snapshot)
 *		CheckForSerializableConflictIn(Relation relation, HeapTupleData
 **tup, Buffer buffer) CheckTableForSerializableConflictIn(Relation relation)
 *
 * final rollback checking
 *		PreCommit_CheckForSerializationFailure(void)
 *
 * two-phase commit support
 *		AtPrepare_PredicateLocks(void);
 *		PostPrepare_PredicateLocks(TransactionId xid);
 *		PredicateLockTwoPhaseFinish(TransactionId xid, bool isCommit);
 *		predicatelock_twophase_recover(TransactionId xid, uint16 info,
 *									   void
 **recdata, uint32 len);
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/slru.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/predicate.h"
#include "storage/predicate_internals.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

/* Uncomment the next line to test the graceful degradation code. */
/* #define TEST_OLDSERXID */

/*
 * Test the most selective fields first, for performance.
 *
 * a is covered by b if all of the following hold:
 *	1) a.database = b.database
 *	2) a.relation = b.relation
 *	3) b.offset is invalid (b is page-granularity or higher)
 *	4) either of the following:
 *		4a) a.offset is valid (a is tuple-granularity) and a.page =
 *b.page or 4b) a.offset is invalid and b.page is invalid (a is page-granularity
 *and b is relation-granularity
 */
#define TargetTagIsCoveredBy(covered_target, covering_target)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  ((GET_PREDICATELOCKTARGETTAG_RELATION(covered_target) == /* (2) */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
       GET_PREDICATELOCKTARGETTAG_RELATION(covering_target)) &&                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      (GET_PREDICATELOCKTARGETTAG_OFFSET(covering_target) == InvalidOffsetNumber)     /* (3) */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      && (((GET_PREDICATELOCKTARGETTAG_OFFSET(covered_target) != InvalidOffsetNumber) /* (4a) */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
              && (GET_PREDICATELOCKTARGETTAG_PAGE(covering_target) == GET_PREDICATELOCKTARGETTAG_PAGE(covered_target))) ||                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
             ((GET_PREDICATELOCKTARGETTAG_PAGE(covering_target) == InvalidBlockNumber) /* (4b) */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
                 && (GET_PREDICATELOCKTARGETTAG_PAGE(covered_target) != InvalidBlockNumber))) &&                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      (GET_PREDICATELOCKTARGETTAG_DB(covered_target) == /* (1) */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
          GET_PREDICATELOCKTARGETTAG_DB(covering_target)))

/*
 * The predicate locking target and lock shared hash tables are partitioned to
 * reduce contention.  To determine which partition a given target belongs to,
 * compute the tag's hash code with PredicateLockTargetTagHashCode(), then
 * apply one of these macros.
 * NB: NUM_PREDICATELOCK_PARTITIONS must be a power of 2!
 */
#define PredicateLockHashPartition(hashcode) ((hashcode) % NUM_PREDICATELOCK_PARTITIONS)
#define PredicateLockHashPartitionLock(hashcode) (&MainLWLockArray[PREDICATELOCK_MANAGER_LWLOCK_OFFSET + PredicateLockHashPartition(hashcode)].lock)
#define PredicateLockHashPartitionLockByIndex(i) (&MainLWLockArray[PREDICATELOCK_MANAGER_LWLOCK_OFFSET + (i)].lock)

#define NPREDICATELOCKTARGETENTS() mul_size(max_predicate_locks_per_xact, add_size(MaxBackends, max_prepared_xacts))

#define SxactIsOnFinishedList(sxact) (!SHMQueueIsDetached(&((sxact)->finishedLink)))

/*
 * Note that a sxact is marked "prepared" once it has passed
 * PreCommit_CheckForSerializationFailure, even if it isn't using
 * 2PC. This is the point at which it can no longer be aborted.
 *
 * The PREPARED flag remains set after commit, so SxactIsCommitted
 * implies SxactIsPrepared.
 */
#define SxactIsCommitted(sxact) (((sxact)->flags & SXACT_FLAG_COMMITTED) != 0)
#define SxactIsPrepared(sxact) (((sxact)->flags & SXACT_FLAG_PREPARED) != 0)
#define SxactIsRolledBack(sxact) (((sxact)->flags & SXACT_FLAG_ROLLED_BACK) != 0)
#define SxactIsDoomed(sxact) (((sxact)->flags & SXACT_FLAG_DOOMED) != 0)
#define SxactIsReadOnly(sxact) (((sxact)->flags & SXACT_FLAG_READ_ONLY) != 0)
#define SxactHasSummaryConflictIn(sxact) (((sxact)->flags & SXACT_FLAG_SUMMARY_CONFLICT_IN) != 0)
#define SxactHasSummaryConflictOut(sxact) (((sxact)->flags & SXACT_FLAG_SUMMARY_CONFLICT_OUT) != 0)
/*
 * The following macro actually means that the specified transaction has a
 * conflict out *to a transaction which committed ahead of it*.  It's hard
 * to get that into a name of a reasonable length.
 */
#define SxactHasConflictOut(sxact) (((sxact)->flags & SXACT_FLAG_CONFLICT_OUT) != 0)
#define SxactIsDeferrableWaiting(sxact) (((sxact)->flags & SXACT_FLAG_DEFERRABLE_WAITING) != 0)
#define SxactIsROSafe(sxact) (((sxact)->flags & SXACT_FLAG_RO_SAFE) != 0)
#define SxactIsROUnsafe(sxact) (((sxact)->flags & SXACT_FLAG_RO_UNSAFE) != 0)
#define SxactIsPartiallyReleased(sxact) (((sxact)->flags & SXACT_FLAG_PARTIALLY_RELEASED) != 0)

/*
 * Compute the hash code associated with a PREDICATELOCKTARGETTAG.
 *
 * To avoid unnecessary recomputations of the hash code, we try to do this
 * just once per function, and then pass it around as needed.  Aside from
 * passing the hashcode to hash_search_with_hash_value(), we can extract
 * the lock partition number from the hashcode.
 */
#define PredicateLockTargetTagHashCode(predicatelocktargettag) get_hash_value(PredicateLockTargetHash, predicatelocktargettag)

/*
 * Given a predicate lock tag, and the hash for its target,
 * compute the lock hash.
 *
 * To make the hash code also depend on the transaction, we xor the sxid
 * struct's address into the hash code, left-shifted so that the
 * partition-number bits don't change.  Since this is only a hash, we
 * don't care if we lose high-order bits of the address; use an
 * intermediate variable to suppress cast-pointer-to-int warnings.
 */
#define PredicateLockHashCodeFromTargetHashCode(predicatelocktag, targethash) ((targethash) ^ ((uint32)PointerGetDatum((predicatelocktag)->myXact)) << LOG2_NUM_PREDICATELOCK_PARTITIONS)

/*
 * The SLRU buffer area through which we access the old xids.
 */
static SlruCtlData OldSerXidSlruCtlData;

#define OldSerXidSlruCtl (&OldSerXidSlruCtlData)

#define OLDSERXID_PAGESIZE BLCKSZ
#define OLDSERXID_ENTRYSIZE sizeof(SerCommitSeqNo)
#define OLDSERXID_ENTRIESPERPAGE (OLDSERXID_PAGESIZE / OLDSERXID_ENTRYSIZE)

/*
 * Set maximum pages based on the number needed to track all transactions.
 */
#define OLDSERXID_MAX_PAGE (MaxTransactionId / OLDSERXID_ENTRIESPERPAGE)

#define OldSerXidNextPage(page) (((page) >= OLDSERXID_MAX_PAGE) ? 0 : (page) + 1)

#define OldSerXidValue(slotno, xid) (*((SerCommitSeqNo *)(OldSerXidSlruCtl->shared->page_buffer[slotno] + ((((uint32)(xid)) % OLDSERXID_ENTRIESPERPAGE) * OLDSERXID_ENTRYSIZE))))

#define OldSerXidPage(xid) (((uint32)(xid)) / OLDSERXID_ENTRIESPERPAGE)

typedef struct OldSerXidControlData
{
  int headPage;          /* newest initialized page */
  TransactionId headXid; /* newest valid Xid in the SLRU */
  TransactionId tailXid; /* oldest xmin we might be interested in */
} OldSerXidControlData;

typedef struct OldSerXidControlData *OldSerXidControl;

static OldSerXidControl oldSerXidControl;

/*
 * When the oldest committed transaction on the "finished" list is moved to
 * SLRU, its predicate locks will be moved to this "dummy" transaction,
 * collapsing duplicate targets.  When a duplicate is found, the later
 * commitSeqNo is used.
 */
static SERIALIZABLEXACT *OldCommittedSxact;

/*
 * These configuration variables are used to set the predicate lock table size
 * and to control promotion of predicate locks to coarser granularity in an
 * attempt to degrade performance (mostly as false positive serialization
 * failure) gracefully in the face of memory pressurel
 */
int max_predicate_locks_per_xact;     /* set by guc.c */
int max_predicate_locks_per_relation; /* set by guc.c */
int max_predicate_locks_per_page;     /* set by guc.c */

/*
 * This provides a list of objects in order to track transactions
 * participating in predicate locking.  Entries in the list are fixed size,
 * and reside in shared memory.  The memory address of an entry must remain
 * fixed during its lifetime.  The list will be protected from concurrent
 * update externally; no provision is made in this code to manage that.  The
 * number of entries in the list, and the size allowed for each entry is
 * fixed upon creation.
 */
static PredXactList PredXact;

/*
 * This provides a pool of RWConflict data elements to use in conflict lists
 * between transactions.
 */
static RWConflictPoolHeader RWConflictPool;

/*
 * The predicate locking hash tables are in shared memory.
 * Each backend keeps pointers to them.
 */
static HTAB *SerializableXidHash;
static HTAB *PredicateLockTargetHash;
static HTAB *PredicateLockHash;
static SHM_QUEUE *FinishedSerializableTransactions;

/*
 * Tag for a dummy entry in PredicateLockTargetHash. By temporarily removing
 * this entry, you can ensure that there's enough scratch space available for
 * inserting one entry in the hash table. This is an otherwise-invalid tag.
 */
static const PREDICATELOCKTARGETTAG ScratchTargetTag = {0, 0, 0, 0};
static uint32 ScratchTargetTagHash;
static LWLock *ScratchPartitionLock;

/*
 * The local hash table used to determine when to combine multiple fine-
 * grained locks into a single courser-grained lock.
 */
static HTAB *LocalPredicateLockHash = NULL;

/*
 * Keep a pointer to the currently-running serializable transaction (if any)
 * for quick reference. Also, remember if we have written anything that could
 * cause a rw-conflict.
 */
static SERIALIZABLEXACT *MySerializableXact = InvalidSerializableXact;
static bool MyXactDidWrite = false;

/*
 * The SXACT_FLAG_RO_UNSAFE optimization might lead us to release
 * MySerializableXact early.  If that happens in a parallel query, the leader
 * needs to defer the destruction of the SERIALIZABLEXACT until end of
 * transaction, because the workers still have a reference to it.  In that
 * case, the leader stores it here.
 */
static SERIALIZABLEXACT *SavedSerializableXact = InvalidSerializableXact;

/* local functions */

static SERIALIZABLEXACT *
CreatePredXact(void);
static void
ReleasePredXact(SERIALIZABLEXACT *sxact);
static SERIALIZABLEXACT *
FirstPredXact(void);
static SERIALIZABLEXACT *
NextPredXact(SERIALIZABLEXACT *sxact);

static bool
RWConflictExists(const SERIALIZABLEXACT *reader, const SERIALIZABLEXACT *writer);
static void
SetRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer);
static void
SetPossibleUnsafeConflict(SERIALIZABLEXACT *roXact, SERIALIZABLEXACT *activeXact);
static void
ReleaseRWConflict(RWConflict conflict);
static void
FlagSxactUnsafe(SERIALIZABLEXACT *sxact);

static bool
OldSerXidPagePrecedesLogically(int page1, int page2);
static void
OldSerXidInit(void);
static void
OldSerXidAdd(TransactionId xid, SerCommitSeqNo minConflictCommitSeqNo);
static SerCommitSeqNo
OldSerXidGetMinConflictCommitSeqNo(TransactionId xid);
static void
OldSerXidSetActiveSerXmin(TransactionId xid);

static uint32
predicatelock_hash(const void *key, Size keysize);
static void
SummarizeOldestCommittedSxact(void);
static Snapshot
GetSafeSnapshot(Snapshot snapshot);
static Snapshot
GetSerializableTransactionSnapshotInt(Snapshot snapshot, VirtualTransactionId *sourcevxid, int sourcepid);
static bool
PredicateLockExists(const PREDICATELOCKTARGETTAG *targettag);
static bool
GetParentPredicateLockTag(const PREDICATELOCKTARGETTAG *tag, PREDICATELOCKTARGETTAG *parent);
static bool
CoarserLockCovers(const PREDICATELOCKTARGETTAG *newtargettag);
static void
RemoveScratchTarget(bool lockheld);
static void
RestoreScratchTarget(bool lockheld);
static void
RemoveTargetIfNoLongerUsed(PREDICATELOCKTARGET *target, uint32 targettaghash);
static void
DeleteChildTargetLocks(const PREDICATELOCKTARGETTAG *newtargettag);
static int
MaxPredicateChildLocks(const PREDICATELOCKTARGETTAG *tag);
static bool
CheckAndPromotePredicateLockRequest(const PREDICATELOCKTARGETTAG *reqtag);
static void
DecrementParentLocks(const PREDICATELOCKTARGETTAG *targettag);
static void
CreatePredicateLock(const PREDICATELOCKTARGETTAG *targettag, uint32 targettaghash, SERIALIZABLEXACT *sxact);
static void
DeleteLockTarget(PREDICATELOCKTARGET *target, uint32 targettaghash);
static bool
TransferPredicateLocksToNewTarget(PREDICATELOCKTARGETTAG oldtargettag, PREDICATELOCKTARGETTAG newtargettag, bool removeOld);
static void
PredicateLockAcquire(const PREDICATELOCKTARGETTAG *targettag);
static void
DropAllPredicateLocksFromTable(Relation relation, bool transfer);
static void
SetNewSxactGlobalXmin(void);
static void
ClearOldPredicateLocks(void);
static void
ReleaseOneSerializableXact(SERIALIZABLEXACT *sxact, bool partial, bool summarize);
static bool
XidIsConcurrent(TransactionId xid);
static void
CheckTargetForConflictsIn(PREDICATELOCKTARGETTAG *targettag);
static void
FlagRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer);
static void
OnConflict_CheckForSerializationFailure(const SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer);
static void
CreateLocalPredicateLockHash(void);
static void
ReleasePredicateLocksLocal(void);

/*------------------------------------------------------------------------*/

/*
 * Does this relation participate in predicate locking? Temporary and system
 * relations are exempt, as are materialized views.
 */
static inline bool
PredicateLockingNeededForRelation(Relation relation)
{

}

/*
 * When a public interface method is called for a read, this is the test to
 * see if we should do a quick return.
 *
 * Note: this function has side-effects! If this transaction has been flagged
 * as RO-safe since the last call, we release all predicate locks and reset
 * MySerializableXact. That makes subsequent calls to return quickly.
 *
 * This is marked as 'inline' to eliminate the function call overhead in the
 * common case that serialization is not needed.
 */
static inline bool
SerializationNeededForRead(Relation relation, Snapshot snapshot)
{









































}

/*
 * Like SerializationNeededForRead(), but called on writes.
 * The logic is the same, but there is no snapshot and we can't be RO-safe.
 */
static inline bool
SerializationNeededForWrite(Relation relation)
{













}

/*------------------------------------------------------------------------*/

/*
 * These functions are a simple implementation of a list for this specific
 * type of struct.  If there is ever a generalized shared memory list, we
 * should probably switch to that.
 */
static SERIALIZABLEXACT *
CreatePredXact(void)
{











}

static void
ReleasePredXact(SERIALIZABLEXACT *sxact)
{







}

static SERIALIZABLEXACT *
FirstPredXact(void)
{









}

static SERIALIZABLEXACT *
NextPredXact(SERIALIZABLEXACT *sxact)
{












}

/*------------------------------------------------------------------------*/

/*
 * These functions manage primitive access to the RWConflict pool and lists.
 */
static bool
RWConflictExists(const SERIALIZABLEXACT *reader, const SERIALIZABLEXACT *writer)
{























}

static void
SetRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer)
{

















}

static void
SetPossibleUnsafeConflict(SERIALIZABLEXACT *roXact, SERIALIZABLEXACT *activeXact)
{


















}

static void
ReleaseRWConflict(RWConflict conflict)
{



}

static void
FlagSxactUnsafe(SERIALIZABLEXACT *sxact)
{























}

/*------------------------------------------------------------------------*/

/*
 * Decide whether an OldSerXid page number is "older" for truncation purposes.
 * Analogous to CLOGPagePrecedes().
 */
static bool
OldSerXidPagePrecedesLogically(int page1, int page2)
{









}

#ifdef USE_ASSERT_CHECKING
static void
OldSerXidPagePrecedesLogicallyUnitTests(void)
{
  int per_page = OLDSERXID_ENTRIESPERPAGE, offset = per_page / 2;
  int newestPage, oldestPage, headPage, targetPage;
  TransactionId newestXact, oldestXact;

  /* GetNewTransactionId() has assigned the last XID it can safely use. */
  newestPage = 2 * SLRU_PAGES_PER_SEGMENT - 1; /* nothing special */
  newestXact = newestPage * per_page + offset;
  Assert(newestXact / per_page == newestPage);
  oldestXact = newestXact + 1;
  oldestXact -= 1U << 31;
  oldestPage = oldestXact / per_page;

  /*
   * In this scenario, the SLRU headPage pertains to the last ~1000 XIDs
   * assigned.  oldestXact finishes, ~2B XIDs having elapsed since it
   * started.  Further transactions cause us to summarize oldestXact to
   * tailPage.  Function must return false so OldSerXidAdd() doesn't zero
   * tailPage (which may contain entries for other old, recently-finished
   * XIDs) and half the SLRU.  Reaching this requires burning ~2B XIDs in
   * single-user mode, a negligible possibility.
   */
  headPage = newestPage;
  targetPage = oldestPage;
  Assert(!OldSerXidPagePrecedesLogically(headPage, targetPage));

  /*
   * In this scenario, the SLRU headPage pertains to oldestXact.  We're
   * summarizing an XID near newestXact.  (Assume few other XIDs used
   * SERIALIZABLE, hence the minimal headPage advancement.  Assume
   * oldestXact was long-running and only recently reached the SLRU.)
   * Function must return true to make OldSerXidAdd() create targetPage.
   *
   * Today's implementation mishandles this case, but it doesn't matter
   * enough to fix.  Verify that the defect affects just one page by
   * asserting correct treatment of its prior page.  Reaching this case
   * requires burning ~2B XIDs in single-user mode, a negligible
   * possibility.  Moreover, if it does happen, the consequence would be
   * mild, namely a new transaction failing in SimpleLruReadPage().
   */
  headPage = oldestPage;
  targetPage = newestPage;
  Assert(OldSerXidPagePrecedesLogically(headPage, targetPage - 1));
#if 0
	Assert(OldSerXidPagePrecedesLogically(headPage, targetPage));
#endif
}
#endif

/*
 * Initialize for the tracking of old serializable committed xids.
 */
static void
OldSerXidInit(void)
{





























}

/*
 * Record a committed read write serializable xid and the minimum
 * commitSeqNo of any transactions to which this xid had a rw-conflict out.
 * An invalid seqNo means that there were no conflicts out from xid.
 */
static void
OldSerXidAdd(TransactionId xid, SerCommitSeqNo minConflictCommitSeqNo)
{

































































}

/*
 * Get the minimum commitSeqNo for any conflict out for the given xid.  For
 * a transaction which exists but has no conflict out, InvalidSerCommitSeqNo
 * will be returned.
 */
static SerCommitSeqNo
OldSerXidGetMinConflictCommitSeqNo(TransactionId xid)
{
































}

/*
 * Call this whenever there is a new xmin for active serializable
 * transactions.  We don't need to keep information on transactions which
 * precede that.  InvalidTransactionId means none active, so everything in
 * the SLRU can be discarded.
 */
static void
OldSerXidSetActiveSerXmin(TransactionId xid)
{






































}

/*
 * Perform a checkpoint --- either during shutdown, or on-the-fly
 *
 * We don't have any data that needs to survive a restart, but this is a
 * convenient place to truncate the SLRU.
 */
void
CheckPointPredicate(void)
{



































































}

/*------------------------------------------------------------------------*/

/*
 * InitPredicateLocks -- Initialize the predicate locking data structures.
 *
 * This is called from CreateSharedMemoryAndSemaphores(), which see for
 * more comments.  In the normal postmaster case, the shared hash tables
 * are created here.  Backends inherit the pointers
 * to the shared tables via fork().  In the EXEC_BACKEND case, each
 * backend re-executes this code to obtain pointers to the already existing
 * shared hash tables.
 */
void
InitPredicateLocks(void)
{











































































































































































}

/*
 * Estimate shared-memory space used for predicate lock table
 */
Size
PredicateLockShmemSize(void)
{







































}

/*
 * Compute the hash code associated with a PREDICATELOCKTAG.
 *
 * Because we want to use just one set of partition locks for both the
 * PREDICATELOCKTARGET and PREDICATELOCK hash tables, we have to make sure
 * that PREDICATELOCKs fall into the same partition number as their
 * associated PREDICATELOCKTARGETs.  dynahash.c expects the partition number
 * to be the low-order bits of the hash code, and therefore a
 * PREDICATELOCKTAG's hash code must have the same low-order bits as the
 * associated PREDICATELOCKTARGETTAG's hash code.  We achieve this with this
 * specialized hash function.
 */
static uint32
predicatelock_hash(const void *key, Size keysize)
{









}

/*
 * GetPredicateLockStatusData
 *		Return a table containing the internal state of the predicate
 *		lock manager for use in pg_lock_status.
 *
 * Like GetLockStatusData, this function tries to hold the partition LWLocks
 * for as short a time as possible by returning two arrays that simply
 * contain the PREDICATELOCKTARGETTAG and SERIALIZABLEXACT for each lock
 * table entry. Multiple copies of the same PREDICATELOCKTARGETTAG and
 * SERIALIZABLEXACT will likely appear.
 */
PredicateLockData *
GetPredicateLockStatusData(void)
{














































}

/*
 * Free up shared memory structures by pushing the oldest sxact (the one at
 * the front of the SummarizeOldestCommittedSxact queue) into summary form.
 * Each call will free exactly one SERIALIZABLEXACT structure and may also
 * free one or more of these structures: SERIALIZABLEXID, PREDICATELOCK,
 * PREDICATELOCKTARGET, RWConflictData.
 */
static void
SummarizeOldestCommittedSxact(void)
{





































}

/*
 * GetSafeSnapshot
 *		Obtain and register a snapshot for a READ ONLY DEFERRABLE
 *		transaction. Ensures that the snapshot is "safe", i.e. a
 *		read-only transaction running on it can execute serializably
 *		without further checks. This requires waiting for concurrent
 *		transactions to complete, and retrying with a new snapshot if
 *		one of them could possibly create a conflict.
 *
 *		As with GetSerializableTransactionSnapshot (which this is a
 *subroutine for), the passed-in Snapshot pointer should reference a static data
 *		area that can safely be passed to GetSnapshotData.
 */
static Snapshot
GetSafeSnapshot(Snapshot origSnapshot)
{






















































}

/*
 * GetSafeSnapshotBlockingPids
 *		If the specified process is currently blocked in
 *GetSafeSnapshot, write the process IDs of all processes that it is blocked by
 *		into the caller-supplied buffer output[].  The list is truncated
 *at output_size, and the number of PIDs written into the buffer is returned.
 *Returns zero if the given PID is not currently blocked in GetSafeSnapshot.
 */
int
GetSafeSnapshotBlockingPids(int blocked_pid, int *output, int output_size)
{
































}

/*
 * Acquire a snapshot that can be used for the current transaction.
 *
 * Make sure we have a SERIALIZABLEXACT reference in MySerializableXact.
 * It should be current for this process and be contained in PredXact.
 *
 * The passed-in Snapshot pointer should reference a static data area that
 * can safely be passed to GetSnapshotData.  The return value is actually
 * always this same pointer; no new snapshot data structure is allocated
 * within this function.
 */
Snapshot
GetSerializableTransactionSnapshot(Snapshot snapshot)
{
























}

/*
 * Import a snapshot to be used for the current transaction.
 *
 * This is nearly the same as GetSerializableTransactionSnapshot, except that
 * we don't take a new snapshot, but rather use the data we're handed.
 *
 * The caller must have verified that the snapshot came from a serializable
 * transaction; and if we're read-write, the source transaction must not be
 * read-only.
 */
void
SetSerializableTransactionSnapshot(Snapshot snapshot, VirtualTransactionId *sourcevxid, int sourcepid)
{



























}

/*
 * Guts of GetSerializableTransactionSnapshot
 *
 * If sourcexid is valid, this is actually an import operation and we should
 * skip calling GetSnapshotData, because the snapshot contents are already
 * loaded up.  HOWEVER: to avoid race conditions, we must check that the
 * source xact is still running after we acquire SerializableXactHashLock.
 * We do that by calling ProcArrayInstallImportedXmin.
 */
static Snapshot
GetSerializableTransactionSnapshotInt(Snapshot snapshot, VirtualTransactionId *sourcevxid, int sourcepid)
{



















































































































































}

static void
CreateLocalPredicateLockHash(void)
{








}

/*
 * Register the top level XID in SerializableXidHash.
 * Also store it for easy reference in MySerializableXact.
 */
void
RegisterPredicateLockingXid(TransactionId xid)
{






























}

/*
 * Check whether there are any predicate locks held by any transaction
 * for the page at the given block number.
 *
 * Note that the transaction may be completed but not yet subject to
 * cleanup due to overlapping serializable transactions.  This must
 * return valid information regardless of transaction isolation level.
 *
 * Also note that this doesn't check for a conflicting relation lock,
 * just a lock specifically on the given page.
 *
 * One use is to support proper behavior during GiST index vacuum.
 */
bool
PageIsPredicateLocked(Relation relation, BlockNumber blkno)
{














}

/*
 * Check whether a particular lock is held by this transaction.
 *
 * Important note: this function may return false even if the lock is
 * being held, because it uses the local lock table which is not
 * updated if another transaction modifies our lock list (e.g. to
 * split an index page). It can also return true when a coarser
 * granularity lock that covers this target is being held. Be careful
 * to only use this function in circumstances where such errors are
 * acceptable!
 */
static bool
PredicateLockExists(const PREDICATELOCKTARGETTAG *targettag)
{















}

/*
 * Return the parent lock tag in the lock hierarchy: the next coarser
 * lock that covers the provided tag.
 *
 * Returns true and sets *parent to the parent tag if one exists,
 * returns false if none exists.
 */
static bool
GetParentPredicateLockTag(const PREDICATELOCKTARGETTAG *tag, PREDICATELOCKTARGETTAG *parent)
{





















}

/*
 * Check whether the lock we are considering is already covered by a
 * coarser lock for our transaction.
 *
 * Like PredicateLockExists, this function might return a false
 * negative, but it will never return a false positive.
 */
static bool
CoarserLockCovers(const PREDICATELOCKTARGETTAG *newtargettag)
{
















}

/*
 * Remove the dummy entry from the predicate lock target hash, to free up some
 * scratch space. The caller must be holding SerializablePredicateLockListLock,
 * and must restore the entry with RestoreScratchTarget() before releasing the
 * lock.
 *
 * If lockheld is true, the caller is already holding the partition lock
 * of the partition containing the scratch entry.
 */
static void
RemoveScratchTarget(bool lockheld)
{














}

/*
 * Re-insert the dummy entry in predicate lock target hash.
 */
static void
RestoreScratchTarget(bool lockheld)
{














}

/*
 * Check whether the list of related predicate locks is empty for a
 * predicate lock target, and remove the target if it is.
 */
static void
RemoveTargetIfNoLongerUsed(PREDICATELOCKTARGET *target, uint32 targettaghash)
{













}

/*
 * Delete child target locks owned by this process.
 * This implementation is assuming that the usage of each target tag field
 * is uniform.  No need to make this hard if we don't have to.
 *
 * We acquire an LWLock in the case of parallel mode, because worker
 * backends have access to the leader's SERIALIZABLEXACT.  Otherwise,
 * we aren't acquiring LWLocks for the predicate lock or lock
 * target structures associated with this transaction unless we're going
 * to modify them, because no other process is permitted to modify our
 * locks.
 */
static void
DeleteChildTargetLocks(const PREDICATELOCKTARGETTAG *newtargettag)
{
























































}

/*
 * Returns the promotion limit for a given predicate lock target.  This is the
 * max number of descendant locks allowed before promoting to the specified
 * tag. Note that the limit includes non-direct descendants (e.g., both tuples
 * and pages for a relation lock).
 *
 * Currently the default limit is 2 for a page lock, and half of the value of
 * max_pred_locks_per_transaction - 1 for a relation lock, to match behavior
 * of earlier releases when upgrading.
 *
 * TODO SSI: We should probably add additional GUCs to allow a maximum ratio
 * of page and tuple locks based on the pages in a relation, and the maximum
 * ratio of tuple locks to tuples in a page.  This would provide more
 * generally "balanced" allocation of locks to where they are most useful,
 * while still allowing the absolute numbers to prevent one relation from
 * tying up all predicate lock resources.
 */
static int
MaxPredicateChildLocks(const PREDICATELOCKTARGETTAG *tag)
{





















}

/*
 * For all ancestors of a newly-acquired predicate lock, increment
 * their child count in the parent hash table. If any of them have
 * more descendants than their promotion threshold, acquire the
 * coarsest such lock.
 *
 * Returns true if a parent lock was acquired and false otherwise.
 */
static bool
CheckAndPromotePredicateLockRequest(const PREDICATELOCKTARGETTAG *reqtag)
{














































}

/*
 * When releasing a lock, decrement the child count on all ancestor
 * locks.
 *
 * This is called only when releasing a lock via
 * DeleteChildTargetLocks (i.e. when a lock becomes redundant because
 * we've acquired its parent, possibly due to promotion) or when a new
 * MVCC write lock makes the predicate lock unnecessary. There's no
 * point in calling it when locks are released at transaction end, as
 * this information is no longer needed.
 */
static void
DecrementParentLocks(const PREDICATELOCKTARGETTAG *targettag)
{










































}

/*
 * Indicate that a predicate lock on the given target is held by the
 * specified transaction. Has no effect if the lock is already held.
 *
 * This updates the lock table and the sxact's lock list, and creates
 * the lock target if necessary, but does *not* do anything related to
 * granularity promotion or the local lock table. See
 * PredicateLockAcquire for that.
 */
static void
CreatePredicateLock(const PREDICATELOCKTARGETTAG *targettag, uint32 targettaghash, SERIALIZABLEXACT *sxact)
{
















































}

/*
 * Acquire a predicate lock on the specified target for the current
 * connection if not already held. This updates the local lock table
 * and uses it to implement granularity promotion. It will consolidate
 * multiple locks into a coarser lock if warranted, and will release
 * any finer-grained locks covered by the new one.
 */
static void
PredicateLockAcquire(const PREDICATELOCKTARGETTAG *targettag)
{


















































}

/*
 *		PredicateLockRelation
 *
 * Gets a predicate lock at the relation level.
 * Skip if not in full serializable transaction isolation level.
 * Skip if this is a temporary table.
 * Clear any finer-grained predicate locks this session has on the relation.
 */
void
PredicateLockRelation(Relation relation, Snapshot snapshot)
{









}

/*
 *		PredicateLockPage
 *
 * Gets a predicate lock at the page level.
 * Skip if not in full serializable transaction isolation level.
 * Skip if this is a temporary table.
 * Skip if a coarser predicate lock already covers this page.
 * Clear any finer-grained predicate locks this session has on the relation.
 */
void
PredicateLockPage(Relation relation, BlockNumber blkno, Snapshot snapshot)
{









}

/*
 *		PredicateLockTuple
 *
 * Gets a predicate lock at the tuple level.
 * Skip if not in full serializable transaction isolation level.
 * Skip if this is a temporary table.
 */
void
PredicateLockTuple(Relation relation, HeapTuple tuple, Snapshot snapshot)
{

















































}

/*
 *		DeleteLockTarget
 *
 * Remove a predicate lock target along with any locks held for it.
 *
 * Caller must hold SerializablePredicateLockListLock and the
 * appropriate hash partition lock for the target.
 */
static void
DeleteLockTarget(PREDICATELOCKTARGET *target, uint32 targettaghash)
{



























}

/*
 *		TransferPredicateLocksToNewTarget
 *
 * Move or copy all the predicate locks for a lock target, for use by
 * index page splits/combines and other things that create or replace
 * lock targets. If 'removeOld' is true, the old locks and the target
 * will be removed.
 *
 * Returns true on success, or false if we ran out of shared memory to
 * allocate the new target or locks. Guaranteed to always succeed if
 * removeOld is set (by using the scratch entry in PredicateLockTargetHash
 * for scratch space).
 *
 * Warning: the "removeOld" option should be used only with care,
 * because this function does not (indeed, can not) update other
 * backends' LocalPredicateLockHash. If we are only adding new
 * entries, this is not a problem: the local lock table is used only
 * as a hint, so missing entries for locks that are held are
 * OK. Having entries for locks that are no longer held, as can happen
 * when using "removeOld", is not in general OK. We can only use it
 * safely when replacing a lock with a coarser-granularity lock that
 * covers it, or if we are absolutely certain that no one will need to
 * refer to that lock in the future.
 *
 * Caller must hold SerializablePredicateLockListLock exclusively.
 */
static bool
TransferPredicateLocksToNewTarget(PREDICATELOCKTARGETTAG oldtargettag, PREDICATELOCKTARGETTAG newtargettag, bool removeOld)
{





































































































































































}

/*
 * Drop all predicate locks of any granularity from the specified relation,
 * which can be a heap relation or an index relation.  If 'transfer' is true,
 * acquire a relation lock on the heap for any transactions with any lock(s)
 * on the specified relation.
 *
 * This requires grabbing a lot of LW locks and scanning the entire lock
 * target table for matches.  That makes this more expensive than most
 * predicate lock management functions, but it will only be called for DDL
 * type commands that are expensive anyway, and there are fast returns when
 * no serializable transactions are active or the relation is temporary.
 *
 * We don't use the TransferPredicateLocksToNewTarget function because it
 * acquires its own locks on the partitions of the two targets involved,
 * and we'll already be holding all partition locks.
 *
 * We can't throw an error from here, because the call could be from a
 * transaction which is not serializable.
 *
 * NOTE: This is currently only called with transfer set to true, but that may
 * change.  If we decide to clean up the locks from a table on commit of a
 * transaction which executed DROP TABLE, the false condition will be useful.
 */
static void
DropAllPredicateLocksFromTable(Relation relation, bool transfer)
{

























































































































































































}

/*
 * TransferPredicateLocksToHeapRelation
 *		For all transactions, transfer all predicate locks for the given
 *		relation to a single relation lock on the heap.
 */
void
TransferPredicateLocksToHeapRelation(Relation relation)
{

}

/*
 *		PredicateLockPageSplit
 *
 * Copies any predicate locks for the old page to the new page.
 * Skip if this is a temporary table or toast table.
 *
 * NOTE: A page split (or overflow) affects all serializable transactions,
 * even if it occurs in the context of another transaction isolation level.
 *
 * NOTE: This currently leaves the local copy of the locks without
 * information on the new lock which is in shared memory.  This could cause
 * problems if enough page splits occur on locked pages without the processes
 * which hold the locks getting in and noticing.
 */
void
PredicateLockPageSplit(Relation relation, BlockNumber oldblkno, BlockNumber newblkno)
{































































}

/*
 *		PredicateLockPageCombine
 *
 * Combines predicate locks for two existing pages.
 * Skip if this is a temporary table or toast table.
 *
 * NOTE: A page combine affects all serializable transactions, even if it
 * occurs in the context of another transaction isolation level.
 */
void
PredicateLockPageCombine(Relation relation, BlockNumber oldblkno, BlockNumber newblkno)
{












}

/*
 * Walk the list of in-progress serializable transactions and find the new
 * xmin.
 */
static void
SetNewSxactGlobalXmin(void)
{

























}

/*
 *		ReleasePredicateLocks
 *
 * Releases predicate locks based on completion of the current transaction,
 * whether committed or rolled back.  It can also be called for a read only
 * transaction when it becomes impossible for the transaction to become
 * part of a dangerous structure.
 *
 * We do nothing unless this is a serializable transaction.
 *
 * This method must ensure that shared memory hash tables are cleaned
 * up in some relatively timely fashion.
 *
 * If this transaction is committing and is holding any predicate locks,
 * it must be added to a list of completed serializable transactions still
 * holding locks.
 *
 * If isReadOnlySafe is true, then predicate locks are being released before
 * the end of the transaction because MySerializableXact has been determined
 * to be RO_SAFE.  In non-parallel mode we can release it completely, but it
 * in parallel mode we partially release the SERIALIZABLEXACT and keep it
 * around until the end of the transaction, allowing each backend to clear its
 * MySerializableXact variable and benefit from the optimization in its own
 * time.
 */
void
ReleasePredicateLocks(bool isCommit, bool isReadOnlySafe)
{













































































































































































































































































































































































}

static void
ReleasePredicateLocksLocal(void)
{









}

/*
 * Clear old predicate locks, belonging to committed transactions that are no
 * longer interesting to any in-progress transaction.
 */
static void
ClearOldPredicateLocks(void)
{



















































































































}

/*
 * This is the normal way to delete anything from any of the predicate
 * locking hash tables.  Given a transaction which we know can be deleted:
 * delete all predicate locks held by that transaction and any predicate
 * lock targets which are now unreferenced by a lock; delete all conflicts
 * for the transaction; delete all xid values for the transaction; then
 * delete the transaction.
 *
 * When the partial flag is set, we can release all predicate locks and
 * in-conflict information -- we've established that there are no longer
 * any overlapping read write transactions for which this transaction could
 * matter -- but keep the transaction entry itself and any outConflicts.
 *
 * When the summarize flag is set, we've run short of room for sxact data
 * and must summarize to the SLRU.  Predicate locks are transferred to a
 * dummy "old" transaction, with duplicate locks on a single target
 * collapsing to a single lock with the "latest" commitSeqNo from among
 * the conflicting locks..
 */
static void
ReleaseOneSerializableXact(SERIALIZABLEXACT *sxact, bool partial, bool summarize)
{







































































































































}

/*
 * Tests whether the given top level transaction is concurrent with
 * (overlaps) our current transaction.
 *
 * We need to identify the top level transaction for SSI, anyway, so pass
 * that to this function to save the overhead of checking the snapshot's
 * subxip array.
 */
static bool
XidIsConcurrent(TransactionId xid)
{



























}

/*
 * CheckForSerializableConflictOut
 *		We are reading a tuple which has been modified.  If it is
 *visible to us but has been deleted, that indicates a rw-conflict out.  If it's
 *		not visible and was created by a concurrent (overlapping)
 *		serializable transaction, that is also a rw-conflict out,
 *
 * We will determine the top level xid of the writing transaction with which
 * we may be in conflict, and check for overlap with our own transaction.
 * If the transactions overlap (i.e., they cannot see each other's writes),
 * then we have a conflict out.
 *
 * This function should be called just about anywhere in heapam.c where a
 * tuple has been read. The caller must hold at least a shared lock on the
 * buffer, because this function might set hint bits on the tuple. There is
 * currently no known reason to call this function from an index AM.
 */
void
CheckForSerializableConflictOut(bool visible, Relation relation, HeapTuple tuple, Buffer buffer, Snapshot snapshot)
{




































































































































































































}

/*
 * Check a particular target for rw-dependency conflict in. A subroutine of
 * CheckForSerializableConflictIn().
 */
static void
CheckTargetForConflictsIn(PREDICATELOCKTARGETTAG *targettag)
{













































































































































}

/*
 * CheckForSerializableConflictIn
 *		We are writing the given tuple.  If that indicates a rw-conflict
 *		in from another serializable transaction, take appropriate
 *action.
 *
 * Skip checking for any granularity for which a parameter is missing.
 *
 * A tuple update or delete is in conflict if we have a predicate lock
 * against the relation or page in which the tuple exists, or against the
 * tuple itself.
 */
void
CheckForSerializableConflictIn(Relation relation, HeapTuple tuple, Buffer buffer)
{










































}

/*
 * CheckTableForSerializableConflictIn
 *		The entire table is going through a DDL-style logical mass
 *delete like TRUNCATE or DROP TABLE.  If that causes a rw-conflict in from
 *		another serializable transaction, take appropriate action.
 *
 * While these operations do not operate entirely within the bounds of
 * snapshot isolation, they can occur inside a serializable transaction, and
 * will logically occur after any reads which saw rows which were destroyed
 * by these operations, so we do what we can to serialize properly under
 * SSI.
 *
 * The relation passed in must be a heap relation. Any predicate lock of any
 * granularity on the heap will cause a rw-conflict in to this transaction.
 * Predicate locks on indexes do not matter because they only exist to guard
 * against conflicting inserts into the index, and this is a mass *delete*.
 * When a table is truncated or dropped, the index will also be truncated
 * or dropped, and we'll deal with locks on the index when that happens.
 *
 * Dropping or truncating a table also needs to drop any existing predicate
 * locks on heap tuples or pages, because they're about to go away. This
 * should be done before altering the predicate locks because the transaction
 * could be rolled back because of a conflict, in which case the lock changes
 * are not needed. (At the moment, we don't actually bother to drop the
 * existing locks on a dropped or truncated table at the moment. That might
 * lead to some false positives, but it doesn't seem worth the trouble.)
 */
void
CheckTableForSerializableConflictIn(Relation relation)
{





















































































}

/*
 * Flag a rw-dependency between two serializable transactions.
 *
 * The caller is responsible for ensuring that we have a LW lock on
 * the transaction hash table.
 */
static void
FlagRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer)
{


















}

/*----------------------------------------------------------------------------
 * We are about to add a RW-edge to the dependency graph - check that we don't
 * introduce a dangerous structure by doing so, and abort one of the
 * transactions if so.
 *
 * A serialization failure can only occur if there is a dangerous structure
 * in the dependency graph:
 *
 *		Tin ------> Tpivot ------> Tout
 *			  rw			 rw
 *
 * Furthermore, Tout must commit first.
 *
 * One more optimization is that if Tin is declared READ ONLY (or commits
 * without writing), we can only have a problem if Tout committed before Tin
 * acquired its snapshot.
 *----------------------------------------------------------------------------
 */
static void
OnConflict_CheckForSerializationFailure(const SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer)
{
































































































































}

/*
 * PreCommit_CheckForSerializableConflicts
 *		Check for dangerous structures in a serializable transaction
 *		at commit.
 *
 * We're checking for a dangerous structure as each conflict is recorded.
 * The only way we could have a problem at commit is if this is the "out"
 * side of a pivot, and neither the "in" side nor the pivot has yet
 * committed.
 *
 * If a dangerous structure is found, the pivot (the near conflict) is
 * marked for death, because rolling back another transaction might mean
 * that we flail without ever making progress.  This transaction is
 * committing writes, so letting it commit ensures progress.  If we
 * canceled the far conflict, it might immediately fail again on retry.
 */
void
PreCommit_CheckForSerializationFailure(void)
{
























































}

/*------------------------------------------------------------------------*/

/*
 * Two-phase commit support
 */

/*
 * AtPrepare_Locks
 *		Do the preparatory work for a PREPARE: make 2PC state file
 *		records for all predicate locks currently held.
 */
void
AtPrepare_PredicateLocks(void)
{


























































}

/*
 * PostPrepare_Locks
 *		Clean up after successful PREPARE. Unlike the non-predicate
 *		lock manager, we do not need to transfer locks to a dummy
 *		PGPROC because our SERIALIZABLEXACT will stay around
 *		anyway. We only need to clean up our local state.
 */
void
PostPrepare_PredicateLocks(TransactionId xid)
{














}

/*
 * PredicateLockTwoPhaseFinish
 *		Release a prepared transaction's predicate locks once it
 *		commits or aborts.
 */
void
PredicateLockTwoPhaseFinish(TransactionId xid, bool isCommit)
{




















}

/*
 * Re-acquire a predicate lock belonging to a transaction that was prepared.
 */
void
predicatelock_twophase_recover(TransactionId xid, uint16 info, void *recdata, uint32 len)
{























































































































}

/*
 * Prepare to share the current SERIALIZABLEXACT with parallel workers.
 * Return a handle object that can be used by AttachSerializableXact() in a
 * parallel worker.
 */
SerializableXactHandle
ShareSerializableXact(void)
{

}

/*
 * Allow parallel workers to import the leader's SERIALIZABLEXACT.
 */
void
AttachSerializableXact(SerializableXactHandle handle)
{








}