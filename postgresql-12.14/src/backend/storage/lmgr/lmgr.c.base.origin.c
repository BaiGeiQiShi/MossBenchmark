/*-------------------------------------------------------------------------
 *
 * lmgr.c
 *	  POSTGRES lock manager code
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/storage/lmgr/lmgr.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "commands/progress.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/lmgr.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "utils/inval.h"

/*
 * Per-backend counter for generating speculative insertion tokens.
 *
 * This may wrap around, but that's OK as it's only used for the short
 * duration between inserting a tuple and checking that there are no (unique)
 * constraint violations.  It's theoretically possible that a backend sees a
 * tuple that was speculatively inserted by another backend, but before it has
 * started waiting on the token, the other backend completes its insertion,
 * and then performs 2^32 unrelated insertions.  And after all that, the
 * first backend finally calls SpeculativeInsertionLockAcquire(), with the
 * intention of waiting for the first insertion to complete, but ends up
 * waiting for the latest unrelated insertion instead.  Even then, nothing
 * particularly bad happens: in the worst case they deadlock, causing one of
 * the transactions to abort.
 */
static uint32 speculativeInsertionToken = 0;

/*
 * Struct to hold context info for transaction lock waits.
 *
 * 'oper' is the operation that needs to wait for the other transaction; 'rel'
 * and 'ctid' specify the address of the tuple being waited for.
 */
typedef struct XactLockTableWaitInfo
{
  XLTW_Oper oper;
  Relation rel;
  ItemPointer ctid;
} XactLockTableWaitInfo;

static void
XactLockTableWaitErrorCb(void *arg);

/*
 * RelationInitLockInfo
 *		Initializes the lock information in a relation descriptor.
 *
 *		relcache.c must call this during creation of any reldesc.
 */
void
RelationInitLockInfo(Relation relation)
{













}

/*
 * SetLocktagRelationOid
 *		Set up a locktag for a relation, given only relation OID
 */
static inline void
SetLocktagRelationOid(LOCKTAG *tag, Oid relid)
{












}

/*
 *		LockRelationOid
 *
 * Lock a relation given only its OID.  This should generally be used
 * before attempting to open the relation's relcache entry.
 */
void
LockRelationOid(Oid relid, LOCKMODE lockmode)
{





























}

/*
 *		ConditionalLockRelationOid
 *
 * As above, but only lock if we can get the lock without blocking.
 * Returns true iff the lock was acquired.
 *
 * NOTE: we do not currently need conditional versions of all the
 * LockXXX routines in this file, but they could easily be added if needed.
 */
bool
ConditionalLockRelationOid(Oid relid, LOCKMODE lockmode)
{
























}

/*
 *		UnlockRelationId
 *
 * Unlock, given a LockRelId.  This is preferred over UnlockRelationOid
 * for speed reasons.
 */
void
UnlockRelationId(LockRelId *relid, LOCKMODE lockmode)
{





}

/*
 *		UnlockRelationOid
 *
 * Unlock, given only a relation Oid.  Use UnlockRelationId if you can.
 */
void
UnlockRelationOid(Oid relid, LOCKMODE lockmode)
{





}

/*
 *		LockRelation
 *
 * This is a convenience routine for acquiring an additional lock on an
 * already-open relation.  Never try to do "relation_open(foo, NoLock)"
 * and then lock with this.
 */
void
LockRelation(Relation relation, LOCKMODE lockmode)
{

















}

/*
 *		ConditionalLockRelation
 *
 * This is a convenience routine for acquiring an additional lock on an
 * already-open relation.  Never try to do "relation_open(foo, NoLock)"
 * and then lock with this.
 */
bool
ConditionalLockRelation(Relation relation, LOCKMODE lockmode)
{
























}

/*
 *		UnlockRelation
 *
 * This is a convenience routine for unlocking a relation without also
 * closing it.
 */
void
UnlockRelation(Relation relation, LOCKMODE lockmode)
{





}

/*
 *		CheckRelationLockedByMe
 *
 * Returns true if current transaction holds a lock on 'relation' of mode
 * 'lockmode'.  If 'orstronger' is true, a stronger lockmode is also OK.
 * ("Stronger" is defined as "numerically higher", which is a bit
 * semantically dubious but is OK for the purposes we use this for.)
 */
bool
CheckRelationLockedByMe(Relation relation, LOCKMODE lockmode, bool orstronger)
{



























}

/*
 *		LockHasWaitersRelation
 *
 * This is a function to check whether someone else is waiting for a
 * lock which we are currently holding.
 */
bool
LockHasWaitersRelation(Relation relation, LOCKMODE lockmode)
{





}

/*
 *		LockRelationIdForSession
 *
 * This routine grabs a session-level lock on the target relation.  The
 * session lock persists across transaction boundaries.  It will be removed
 * when UnlockRelationIdForSession() is called, or if an ereport(ERROR) occurs,
 * or if the backend exits.
 *
 * Note that one should also grab a transaction-level lock on the rel
 * in any transaction that actually uses the rel, to ensure that the
 * relcache entry is up to date.
 */
void
LockRelationIdForSession(LockRelId *relid, LOCKMODE lockmode)
{





}

/*
 *		UnlockRelationIdForSession
 */
void
UnlockRelationIdForSession(LockRelId *relid, LOCKMODE lockmode)
{





}

/*
 *		LockRelationForExtension
 *
 * This lock tag is used to interlock addition of pages to relations.
 * We need such locking because bufmgr/smgr definition of P_NEW is not
 * race-condition-proof.
 *
 * We assume the caller is already holding some type of regular lock on
 * the relation, so no AcceptInvalidationMessages call is needed here.
 */
void
LockRelationForExtension(Relation relation, LOCKMODE lockmode)
{





}

/*
 *		ConditionalLockRelationForExtension
 *
 * As above, but only lock if we can get the lock without blocking.
 * Returns true iff the lock was acquired.
 */
bool
ConditionalLockRelationForExtension(Relation relation, LOCKMODE lockmode)
{





}

/*
 *		RelationExtensionLockWaiterCount
 *
 * Count the number of processes waiting for the given relation extension lock.
 */
int
RelationExtensionLockWaiterCount(Relation relation)
{





}

/*
 *		UnlockRelationForExtension
 */
void
UnlockRelationForExtension(Relation relation, LOCKMODE lockmode)
{





}

/*
 *		LockDatabaseFrozenIds
 *
 * This allows one backend per database to execute vac_update_datfrozenxid().
 */
void
LockDatabaseFrozenIds(LOCKMODE lockmode)
{





}

/*
 *		LockPage
 *
 * Obtain a page-level lock.  This is currently used by some index access
 * methods to lock individual index pages.
 */
void
LockPage(Relation relation, BlockNumber blkno, LOCKMODE lockmode)
{





}

/*
 *		ConditionalLockPage
 *
 * As above, but only lock if we can get the lock without blocking.
 * Returns true iff the lock was acquired.
 */
bool
ConditionalLockPage(Relation relation, BlockNumber blkno, LOCKMODE lockmode)
{





}

/*
 *		UnlockPage
 */
void
UnlockPage(Relation relation, BlockNumber blkno, LOCKMODE lockmode)
{





}

/*
 *		LockTuple
 *
 * Obtain a tuple-level lock.  This is used in a less-than-intuitive fashion
 * because we can't afford to keep a separate lock in shared memory for every
 * tuple.  See heap_lock_tuple before using this!
 */
void
LockTuple(Relation relation, ItemPointer tid, LOCKMODE lockmode)
{





}

/*
 *		ConditionalLockTuple
 *
 * As above, but only lock if we can get the lock without blocking.
 * Returns true iff the lock was acquired.
 */
bool
ConditionalLockTuple(Relation relation, ItemPointer tid, LOCKMODE lockmode)
{





}

/*
 *		UnlockTuple
 */
void
UnlockTuple(Relation relation, ItemPointer tid, LOCKMODE lockmode)
{





}

/*
 *		XactLockTableInsert
 *
 * Insert a lock showing that the given transaction ID is running ---
 * this is done when an XID is acquired by a transaction or subtransaction.
 * The lock can then be used to wait for the transaction to finish.
 */
void
XactLockTableInsert(TransactionId xid)
{





}

/*
 *		XactLockTableDelete
 *
 * Delete the lock showing that the given transaction ID is running.
 * (This is never used for main transaction IDs; those locks are only
 * released implicitly at transaction end.  But we do use it for subtrans IDs.)
 */
void
XactLockTableDelete(TransactionId xid)
{





}

/*
 *		XactLockTableWait
 *
 * Wait for the specified transaction to commit or abort.  If an operation
 * is specified, an error context callback is set up.  If 'oper' is passed as
 * None, no error context callback is set up.
 *
 * Note that this does the right thing for subtransactions: if we wait on a
 * subtransaction, we will exit as soon as it aborts or its top parent commits.
 * It takes some extra work to ensure this, because to save on shared memory
 * the XID lock of a subtransaction is released when it ends, whether
 * successfully or unsuccessfully.  So we have to check if it's "still running"
 * and if so wait for its parent.
 */
void
XactLockTableWait(TransactionId xid, Relation rel, ItemPointer ctid, XLTW_Oper oper)
{



































































}

/*
 *		ConditionalXactLockTableWait
 *
 * As above, but only lock if we can get the lock without blocking.
 * Returns true if the lock was acquired.
 */
bool
ConditionalXactLockTableWait(TransactionId xid)
{
































}

/*
 *		SpeculativeInsertionLockAcquire
 *
 * Insert a lock showing that the given transaction ID is inserting a tuple,
 * but hasn't yet decided whether it's going to keep it.  The lock can then be
 * used to wait for the decision to go ahead with the insertion, or aborting
 * it.
 *
 * The token is used to distinguish multiple insertions by the same
 * transaction.  It is returned to caller.
 */
uint32
SpeculativeInsertionLockAcquire(TransactionId xid)
{

















}

/*
 *		SpeculativeInsertionLockRelease
 *
 * Delete the lock showing that the given transaction is speculatively
 * inserting a tuple.
 */
void
SpeculativeInsertionLockRelease(TransactionId xid)
{





}

/*
 *		SpeculativeInsertionWait
 *
 * Wait for the specified transaction to finish or abort the insertion of a
 * tuple.
 */
void
SpeculativeInsertionWait(TransactionId xid, uint32 token)
{









}

/*
 * XactLockTableWaitErrorContextCb
 *		Error context callback for transaction lock waits.
 */
static void
XactLockTableWaitErrorCb(void *arg)
{











































}

/*
 * WaitForLockersMultiple
 *		Wait until no transaction holds locks that conflict with the
 *given locktags at the given lockmode.
 *
 * To do this, obtain the current list of lockers, and wait on their VXIDs
 * until they are finished.
 *
 * Note we don't try to acquire the locks on the given locktags, only the
 * VXIDs and XIDs of their lock holders; if somebody grabs a conflicting lock
 * on the objects after we obtained our initial list of lockers, we will not
 * wait for them.
 */
void
WaitForLockersMultiple(List *locktags, LOCKMODE lockmode, bool progress)
{





































































}

/*
 * WaitForLockers
 *
 * Same as WaitForLockersMultiple, for a single lock tag.
 */
void
WaitForLockers(LOCKTAG heaplocktag, LOCKMODE lockmode, bool progress)
{





}

/*
 *		LockDatabaseObject
 *
 * Obtain a lock on a general object of the current database.  Don't use
 * this for shared objects (such as tablespaces).  It's unwise to apply it
 * to relations, also, since a lock taken this way will NOT conflict with
 * locks taken via LockRelation and friends.
 */
void
LockDatabaseObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{








}

/*
 *		UnlockDatabaseObject
 */
void
UnlockDatabaseObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{





}

/*
 *		LockSharedObject
 *
 * Obtain a lock on a shared-across-databases object.
 */
void
LockSharedObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{








}

/*
 *		UnlockSharedObject
 */
void
UnlockSharedObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{





}

/*
 *		LockSharedObjectForSession
 *
 * Obtain a session-level lock on a shared-across-databases object.
 * See LockRelationIdForSession for notes about session-level locks.
 */
void
LockSharedObjectForSession(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{





}

/*
 *		UnlockSharedObjectForSession
 */
void
UnlockSharedObjectForSession(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{





}

/*
 * Append a description of a lockable object to buf.
 *
 * Ideally we would print names for the numeric values, but that requires
 * getting locks on system tables, which might cause problems since this is
 * typically used to report deadlock situations.
 */
void
DescribeLockTag(StringInfo buf, const LOCKTAG *tag)
{








































}

/*
 * GetLockNameFromTagType
 *
 *	Given locktag type, return the corresponding lock name.
 */
const char *
GetLockNameFromTagType(uint16 locktag_type)
{





}