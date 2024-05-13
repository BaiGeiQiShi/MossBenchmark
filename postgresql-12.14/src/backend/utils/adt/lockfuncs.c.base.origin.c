/*-------------------------------------------------------------------------
 *
 * lockfuncs.c
 *		Functions for SQL access to various lock-manager capabilities.
 *
 * Copyright (c) 2002-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		src/backend/utils/adt/lockfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/predicate_internals.h"
#include "utils/array.h"
#include "utils/builtins.h"

/* This must match enum LockTagType! */
const char *const LockTagTypeNames[] = {"relation", "extend", "page", "tuple", "transactionid", "virtualxid", "speculative token", "object", "userlock", "advisory", "frozenid"};

/* This must match enum PredicateLockTargetType (predicate_internals.h) */
static const char *const PredicateLockTagTypeNames[] = {"relation", "page", "tuple"};

/* Working status for pg_lock_status */
typedef struct
{
  LockData *lockData;              /* state data from lmgr */
  int currIdx;                     /* current PROCLOCK index */
  PredicateLockData *predLockData; /* state data for pred locks */
  int predLockIdx;                 /* current index for pred lock */
} PG_Lock_Status;

/* Number of columns in pg_locks output */
#define NUM_LOCK_STATUS_COLUMNS 15

/*
 * VXIDGetDatum - Construct a text representation of a VXID
 *
 * This is currently only used in pg_lock_status, so we put it here.
 */
static Datum
VXIDGetDatum(BackendId bid, LocalTransactionId lxid)
{









}

/*
 * pg_lock_status - produce a view with one row per held or awaited lock mode
 */
Datum
pg_lock_status(PG_FUNCTION_ARGS)
{
































































































































































































































































































































}

/*
 * pg_blocking_pids - produce an array of the PIDs blocking given PID
 *
 * The reported PIDs are those that hold a lock conflicting with blocked_pid's
 * current request (hard block), or are requesting such a lock and are ahead
 * of blocked_pid in the lock's wait queue (soft block).
 *
 * In parallel-query cases, we report all PIDs blocking any member of the
 * given PID's lock group, and the reported PIDs are those of the blocking
 * PIDs' lock group leaders.  This allows callers to compare the result to
 * lists of clients' pg_backend_pid() results even during a parallel query.
 *
 * Parallel query makes it possible for there to be duplicate PIDs in the
 * result (either because multiple waiters are blocked by same PID, or
 * because multiple blockers have same group leader PID).  We do not bother
 * to eliminate such duplicates from the result.
 *
 * We need not consider predicate locks here, since those don't block anything.
 */
Datum
pg_blocking_pids(PG_FUNCTION_ARGS)
{



































































































}

/*
 * pg_safe_snapshot_blocking_pids - produce an array of the PIDs blocking
 * given PID from getting a safe snapshot
 *
 * XXX this does not consider parallel-query cases; not clear how big a
 * problem that is in practice
 */
Datum
pg_safe_snapshot_blocking_pids(PG_FUNCTION_ARGS)
{





























}

/*
 * pg_isolation_test_session_is_blocked - support function for isolationtester
 *
 * Check if specified PID is blocked by any of the PIDs listed in the second
 * argument.  Currently, this looks for blocking caused by waiting for
 * heavyweight locks or safe snapshots.  We ignore blockage caused by PIDs
 * not directly under the isolationtester's control, eg autovacuum.
 *
 * This is an undocumented function intended for use by the isolation tester,
 * and may change in future releases as required for testing purposes.
 */
Datum
pg_isolation_test_session_is_blocked(PG_FUNCTION_ARGS)
{

































































}

/*
 * Functions for manipulating advisory locks
 *
 * We make use of the locktag fields as follows:
 *
 *	field1: MyDatabaseId ... ensures locks are local to each database
 *	field2: first of 2 int4 keys, or high-order half of an int8 key
 *	field3: second of 2 int4 keys, or low-order half of an int8 key
 *	field4: 1 if using an int8 key, 2 if using 2 int4 keys
 */
#define SET_LOCKTAG_INT64(tag, key64) SET_LOCKTAG_ADVISORY(tag, MyDatabaseId, (uint32)((key64) >> 32), (uint32)(key64), 1)
#define SET_LOCKTAG_INT32(tag, key1, key2) SET_LOCKTAG_ADVISORY(tag, MyDatabaseId, key1, key2, 2)

static void
PreventAdvisoryLocksInParallelMode(void)
{




}

/*
 * pg_advisory_lock(int8) - acquire exclusive lock on an int8 key
 */
Datum
pg_advisory_lock_int8(PG_FUNCTION_ARGS)
{









}

/*
 * pg_advisory_xact_lock(int8) - acquire xact scoped
 * exclusive lock on an int8 key
 */
Datum
pg_advisory_xact_lock_int8(PG_FUNCTION_ARGS)
{









}

/*
 * pg_advisory_lock_shared(int8) - acquire share lock on an int8 key
 */
Datum
pg_advisory_lock_shared_int8(PG_FUNCTION_ARGS)
{









}

/*
 * pg_advisory_xact_lock_shared(int8) - acquire xact scoped
 * share lock on an int8 key
 */
Datum
pg_advisory_xact_lock_shared_int8(PG_FUNCTION_ARGS)
{









}

/*
 * pg_try_advisory_lock(int8) - acquire exclusive lock on an int8 key, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_int8(PG_FUNCTION_ARGS)
{










}

/*
 * pg_try_advisory_xact_lock(int8) - acquire xact scoped
 * exclusive lock on an int8 key, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_xact_lock_int8(PG_FUNCTION_ARGS)
{










}

/*
 * pg_try_advisory_lock_shared(int8) - acquire share lock on an int8 key, no
 * wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_shared_int8(PG_FUNCTION_ARGS)
{










}

/*
 * pg_try_advisory_xact_lock_shared(int8) - acquire xact scoped
 * share lock on an int8 key, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_xact_lock_shared_int8(PG_FUNCTION_ARGS)
{










}

/*
 * pg_advisory_unlock(int8) - release exclusive lock on an int8 key
 *
 * Returns true if successful, false if lock was not held
 */
Datum
pg_advisory_unlock_int8(PG_FUNCTION_ARGS)
{










}

/*
 * pg_advisory_unlock_shared(int8) - release share lock on an int8 key
 *
 * Returns true if successful, false if lock was not held
 */
Datum
pg_advisory_unlock_shared_int8(PG_FUNCTION_ARGS)
{










}

/*
 * pg_advisory_lock(int4, int4) - acquire exclusive lock on 2 int4 keys
 */
Datum
pg_advisory_lock_int4(PG_FUNCTION_ARGS)
{










}

/*
 * pg_advisory_xact_lock(int4, int4) - acquire xact scoped
 * exclusive lock on 2 int4 keys
 */
Datum
pg_advisory_xact_lock_int4(PG_FUNCTION_ARGS)
{










}

/*
 * pg_advisory_lock_shared(int4, int4) - acquire share lock on 2 int4 keys
 */
Datum
pg_advisory_lock_shared_int4(PG_FUNCTION_ARGS)
{










}

/*
 * pg_advisory_xact_lock_shared(int4, int4) - acquire xact scoped
 * share lock on 2 int4 keys
 */
Datum
pg_advisory_xact_lock_shared_int4(PG_FUNCTION_ARGS)
{










}

/*
 * pg_try_advisory_lock(int4, int4) - acquire exclusive lock on 2 int4 keys, no
 * wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_int4(PG_FUNCTION_ARGS)
{











}

/*
 * pg_try_advisory_xact_lock(int4, int4) - acquire xact scoped
 * exclusive lock on 2 int4 keys, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_xact_lock_int4(PG_FUNCTION_ARGS)
{











}

/*
 * pg_try_advisory_lock_shared(int4, int4) - acquire share lock on 2 int4 keys,
 * no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_lock_shared_int4(PG_FUNCTION_ARGS)
{











}

/*
 * pg_try_advisory_xact_lock_shared(int4, int4) - acquire xact scoped
 * share lock on 2 int4 keys, no wait
 *
 * Returns true if successful, false if lock not available
 */
Datum
pg_try_advisory_xact_lock_shared_int4(PG_FUNCTION_ARGS)
{











}

/*
 * pg_advisory_unlock(int4, int4) - release exclusive lock on 2 int4 keys
 *
 * Returns true if successful, false if lock was not held
 */
Datum
pg_advisory_unlock_int4(PG_FUNCTION_ARGS)
{











}

/*
 * pg_advisory_unlock_shared(int4, int4) - release share lock on 2 int4 keys
 *
 * Returns true if successful, false if lock was not held
 */
Datum
pg_advisory_unlock_shared_int4(PG_FUNCTION_ARGS)
{











}

/*
 * pg_advisory_unlock_all() - release all advisory locks
 */
Datum
pg_advisory_unlock_all(PG_FUNCTION_ARGS)
{



}