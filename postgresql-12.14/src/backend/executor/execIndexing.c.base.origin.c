/*-------------------------------------------------------------------------
 *
 * execIndexing.c
 *	  routines for inserting index tuples and enforcing unique and
 *	  exclusion constraints.
 *
 * ExecInsertIndexTuples() is the main entry point.  It's called after
 * inserting a tuple to the heap, and it inserts corresponding index tuples
 * into all indexes.  At the same time, it enforces any unique and
 * exclusion constraints:
 *
 * Unique Indexes
 * --------------
 *
 * Enforcing a unique constraint is straightforward.  When the index AM
 * inserts the tuple to the index, it also checks that there are no
 * conflicting tuples in the index already.  It does so atomically, so that
 * even if two backends try to insert the same key concurrently, only one
 * of them will succeed.  All the logic to ensure atomicity, and to wait
 * for in-progress transactions to finish, is handled by the index AM.
 *
 * If a unique constraint is deferred, we request the index AM to not
 * throw an error if a conflict is found.  Instead, we make note that there
 * was a conflict and return the list of indexes with conflicts to the
 * caller.  The caller must re-check them later, by calling index_insert()
 * with the UNIQUE_CHECK_EXISTING option.
 *
 * Exclusion Constraints
 * ---------------------
 *
 * Exclusion constraints are different from unique indexes in that when the
 * tuple is inserted to the index, the index AM does not check for
 * duplicate keys at the same time.  After the insertion, we perform a
 * separate scan on the index to check for conflicting tuples, and if one
 * is found, we throw an error and the transaction is aborted.  If the
 * conflicting tuple's inserter or deleter is in-progress, we wait for it
 * to finish first.
 *
 * There is a chance of deadlock, if two backends insert a tuple at the
 * same time, and then perform the scan to check for conflicts.  They will
 * find each other's tuple, and both try to wait for each other.  The
 * deadlock detector will detect that, and abort one of the transactions.
 * That's fairly harmless, as one of them was bound to abort with a
 * "duplicate key error" anyway, although you get a different error
 * message.
 *
 * If an exclusion constraint is deferred, we still perform the conflict
 * checking scan immediately after inserting the index tuple.  But instead
 * of throwing an error if a conflict is found, we return that information
 * to the caller.  The caller must re-check them later by calling
 * check_exclusion_constraint().
 *
 * Speculative insertion
 * ---------------------
 *
 * Speculative insertion is a two-phase mechanism used to implement
 * INSERT ... ON CONFLICT DO UPDATE/NOTHING.  The tuple is first inserted
 * to the heap and update the indexes as usual, but if a constraint is
 * violated, we can still back out the insertion without aborting the whole
 * transaction.  In an INSERT ... ON CONFLICT statement, if a conflict is
 * detected, the inserted tuple is backed out and the ON CONFLICT action is
 * executed instead.
 *
 * Insertion to a unique index works as usual: the index AM checks for
 * duplicate keys atomically with the insertion.  But instead of throwing
 * an error on a conflict, the speculatively inserted heap tuple is backed
 * out.
 *
 * Exclusion constraints are slightly more complicated.  As mentioned
 * earlier, there is a risk of deadlock when two backends insert the same
 * key concurrently.  That was not a problem for regular insertions, when
 * one of the transactions has to be aborted anyway, but with a speculative
 * insertion we cannot let a deadlock happen, because we only want to back
 * out the speculatively inserted tuple on conflict, not abort the whole
 * transaction.
 *
 * When a backend detects that the speculative insertion conflicts with
 * another in-progress tuple, it has two options:
 *
 * 1. back out the speculatively inserted tuple, then wait for the other
 *	  transaction, and retry. Or,
 * 2. wait for the other transaction, with the speculatively inserted tuple
 *	  still in place.
 *
 * If two backends insert at the same time, and both try to wait for each
 * other, they will deadlock.  So option 2 is not acceptable.  Option 1
 * avoids the deadlock, but it is prone to a livelock instead.  Both
 * transactions will wake up immediately as the other transaction backs
 * out.  Then they both retry, and conflict with each other again, lather,
 * rinse, repeat.
 *
 * To avoid the livelock, one of the backends must back out first, and then
 * wait, while the other one waits without backing out.  It doesn't matter
 * which one backs out, so we employ an arbitrary rule that the transaction
 * with the higher XID backs out.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execIndexing.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/index.h"
#include "executor/executor.h"
#include "nodes/nodeFuncs.h"
#include "storage/lmgr.h"
#include "utils/snapmgr.h"

/* waitMode argument to check_exclusion_or_unique_constraint() */
typedef enum
{
  CEOUC_WAIT,
  CEOUC_NOWAIT,
  CEOUC_LIVELOCK_PREVENTING_WAIT
} CEOUC_WAIT_MODE;

static bool
check_exclusion_or_unique_constraint(Relation heap, Relation index, IndexInfo *indexInfo, ItemPointer tupleid, Datum *values, bool *isnull, EState *estate, bool newIndex, CEOUC_WAIT_MODE waitMode, bool errorOK, ItemPointer conflictTid);

static bool
index_recheck_constraint(Relation index, Oid *constr_procs, Datum *existing_values, bool *existing_isnull, Datum *new_values);

/* ----------------------------------------------------------------
 *		ExecOpenIndices
 *
 *		Find the indices associated with a result relation, open them,
 *		and save information about them in the result ResultRelInfo.
 *
 *		At entry, caller has already opened and locked
 *		resultRelInfo->ri_RelationDesc.
 * ----------------------------------------------------------------
 */
void
ExecOpenIndices(ResultRelInfo *resultRelInfo, bool speculative)
{





































































}

/* ----------------------------------------------------------------
 *		ExecCloseIndices
 *
 *		Close the index relations stored in resultRelInfo
 * ----------------------------------------------------------------
 */
void
ExecCloseIndices(ResultRelInfo *resultRelInfo)
{






















}

/* ----------------------------------------------------------------
 *		ExecInsertIndexTuples
 *
 *		This routine takes care of inserting index tuples
 *		into all the relations indexing the result relation
 *		when a heap tuple is inserted into the result relation.
 *
 *		Unique and exclusion constraints are enforced at the same
 *		time.  This returns a list of index OIDs for any unique or
 *		exclusion constraints that are deferred and that had
 *		potential (unconfirmed) conflicts.  (if noDupErr == true,
 *		the same is done for non-deferred constraints, but report
 *		if conflict was speculative or deferred conflict to caller)
 *
 *		If 'arbiterIndexes' is nonempty, noDupErr applies only to
 *		those indexes.  NIL means noDupErr applies to all indexes.
 *
 *		CAUTION: this must not be called for a HOT update.
 *		We can't defend against that here for lack of info.
 *		Should we change the API to make it safer?
 * ----------------------------------------------------------------
 */
List *
ExecInsertIndexTuples(TupleTableSlot *slot, EState *estate, bool noDupErr, bool *specConflict, List *arbiterIndexes)
{
























































































































































































}

/* ----------------------------------------------------------------
 *		ExecCheckIndexConstraints
 *
 *		This routine checks if a tuple violates any unique or
 *		exclusion constraints.  Returns true if there is no conflict.
 *		Otherwise returns false, and the TID of the conflicting
 *		tuple is returned in *conflictTid.
 *
 *		If 'arbiterIndexes' is given, only those indexes are checked.
 *		NIL means all indexes.
 *
 *		Note that this doesn't lock the values in any way, so it's
 *		possible that a conflicting tuple is inserted immediately
 *		after this returns.  But this can be used for a pre-check
 *		before insertion.
 * ----------------------------------------------------------------
 */
bool
ExecCheckIndexConstraints(TupleTableSlot *slot, EState *estate, ItemPointer conflictTid, List *arbiterIndexes)
{




















































































































}

/*
 * Check for violation of an exclusion or unique constraint
 *
 * heap: the table containing the new tuple
 * index: the index supporting the constraint
 * indexInfo: info about the index, including the exclusion properties
 * tupleid: heap TID of the new tuple we have just inserted (invalid if we
 *		haven't inserted a new tuple yet)
 * values, isnull: the *index* column values computed for the new tuple
 * estate: an EState we can do evaluation in
 * newIndex: if true, we are trying to build a new index (this affects
 *		only the wording of error messages)
 * waitMode: whether to wait for concurrent inserters/deleters
 * violationOK: if true, don't throw error for violation
 * conflictTid: if not-NULL, the TID of the conflicting tuple is returned here
 *
 * Returns true if OK, false if actual or potential violation
 *
 * 'waitMode' determines what happens if a conflict is detected with a tuple
 * that was inserted or deleted by a transaction that's still running.
 * CEOUC_WAIT means that we wait for the transaction to commit, before
 * throwing an error or returning.  CEOUC_NOWAIT means that we report the
 * violation immediately; so the violation is only potential, and the caller
 * must recheck sometime later.  This behavior is convenient for deferred
 * exclusion checks; we need not bother queuing a deferred event if there is
 * definitely no conflict at insertion time.
 *
 * CEOUC_LIVELOCK_PREVENTING_WAIT is like CEOUC_NOWAIT, but we will sometimes
 * wait anyway, to prevent livelocking if two transactions try inserting at
 * the same time.  This is used with speculative insertions, for INSERT ON
 * CONFLICT statements. (See notes in file header)
 *
 * If violationOK is true, we just report the potential or actual violation to
 * the caller by returning 'false'.  Otherwise we throw a descriptive error
 * message here.  When violationOK is false, a false result is impossible.
 *
 * Note: The indexam is normally responsible for checking unique constraints,
 * so this normally only needs to be used for exclusion constraints.  But this
 * function is also called when doing a "pre-check" for conflicts on a unique
 * constraint, when doing speculative insertion.  Caller may use the returned
 * conflict TID to take further steps.
 */
static bool
check_exclusion_or_unique_constraint(Relation heap, Relation index, IndexInfo *indexInfo, ItemPointer tupleid, Datum *values, bool *isnull, EState *estate, bool newIndex, CEOUC_WAIT_MODE waitMode, bool violationOK, ItemPointer conflictTid)
{

















































































































































































}

/*
 * Check for violation of an exclusion constraint
 *
 * This is a dumbed down version of check_exclusion_or_unique_constraint
 * for external callers. They don't need all the special modes.
 */
void
check_exclusion_constraint(Relation heap, Relation index, IndexInfo *indexInfo, ItemPointer tupleid, Datum *values, bool *isnull, EState *estate, bool newIndex)
{

}

/*
 * Check existing tuple's index values to see if it really matches the
 * exclusion condition against the new_values.  Returns true if conflict.
 */
static bool
index_recheck_constraint(Relation index, Oid *constr_procs, Datum *existing_values, bool *existing_isnull, Datum *new_values)
{


















}