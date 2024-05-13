/*-------------------------------------------------------------------------
 *
 * origin.c
 *	  Logical replication progress tracking support.
 *
 * Copyright (c) 2013-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/replication/logical/origin.c
 *
 * NOTES
 *
 * This file provides the following:
 * * An infrastructure to name nodes in a replication setup
 * * A facility to efficiently store and persist replication progress in an
 *	 efficient and durable manner.
 *
 * Replication origin consist out of a descriptive, user defined, external
 * name and a short, thus space efficient, internal 2 byte one. This split
 * exists because replication origin have to be stored in WAL and shared
 * memory and long descriptors would be inefficient.  For now only use 2 bytes
 * for the internal id of a replication origin as it seems unlikely that there
 * soon will be more than 65k nodes in one replication setup; and using only
 * two bytes allow us to be more space efficient.
 *
 * Replication progress is tracked in a shared memory table
 * (ReplicationState) that's dumped to disk every checkpoint. Entries
 * ('slots') in this table are identified by the internal id. That's the case
 * because it allows to increase replication progress during crash
 * recovery. To allow doing so we store the original LSN (from the originating
 * system) of a transaction in the commit record. That allows to recover the
 * precise replayed state after crash recovery; without requiring synchronous
 * commits. Allowing logical replication to use asynchronous commit is
 * generally good for performance, but especially important as it allows a
 * single threaded replay process to keep up with a source that has multiple
 * backends generating changes concurrently.  For efficiency and simplicity
 * reasons a backend can setup one replication origin that's from then used as
 * the source of changes produced by the backend, until reset again.
 *
 * This infrastructure is intended to be used in cooperation with logical
 * decoding. When replaying from a remote system the configured origin is
 * provided to output plugins, allowing prevention of replication loops and
 * other filtering.
 *
 * There are several levels of locking at work:
 *
 * * To create and drop replication origins an exclusive lock on
 *	 pg_replication_slot is required for the duration. That allows us to
 *	 safely and conflict free assign new origins using a dirty snapshot.
 *
 * * When creating an in-memory replication progress slot the ReplicationOrigin
 *	 LWLock has to be held exclusively; when iterating over the replication
 *	 progress a shared lock has to be held, the same when advancing the
 *	 replication progress of an individual backend that has not setup as the
 *	 session's replication origin.
 *
 * * When manipulating or looking at the remote_lsn and local_lsn fields of a
 *	 replication progress slot that slot's lwlock has to be held. That's
 *	 primarily because we do not assume 8 byte writes (the LSN) is atomic on
 *	 all our platforms, but it also simplifies memory ordering concerns
 *	 between the remote and local lsn. We use a lwlock instead of a spinlock
 *	 so it's less harmful to hold the lock over a WAL write
 *	 (cf. AdvanceReplicationProgress).
 *
 * ---------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "funcapi.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"

#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "nodes/execnodes.h"

#include "replication/origin.h"
#include "replication/logical.h"
#include "pgstat.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/lmgr.h"
#include "storage/condition_variable.h"
#include "storage/copydir.h"

#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_lsn.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"

/*
 * Replay progress of a single remote node.
 */
typedef struct ReplicationState
{
  /*
   * Local identifier for the remote node.
   */
  RepOriginId roident;

  /*
   * Location of the latest commit from the remote side.
   */
  XLogRecPtr remote_lsn;

  /*
   * Remember the local lsn of the commit record so we can XLogFlush() to it
   * during a checkpoint so we know the commit record actually is safe on
   * disk.
   */
  XLogRecPtr local_lsn;

  /*
   * PID of backend that's acquired slot, or 0 if none.
   */
  int acquired_by;

  /*
   * Condition variable that's signalled when acquired_by changes.
   */
  ConditionVariable origin_cv;

  /*
   * Lock protecting remote_lsn and local_lsn.
   */
  LWLock lock;
} ReplicationState;

/*
 * On disk version of ReplicationState.
 */
typedef struct ReplicationStateOnDisk
{
  RepOriginId roident;
  XLogRecPtr remote_lsn;
} ReplicationStateOnDisk;

typedef struct ReplicationStateCtl
{
  /* Tranche to use for per-origin LWLocks */
  int tranche_id;
  /* Array of length max_replication_slots */
  ReplicationState states[FLEXIBLE_ARRAY_MEMBER];
} ReplicationStateCtl;

/* external variables */
RepOriginId replorigin_session_origin = InvalidRepOriginId; /* assumed identity */
XLogRecPtr replorigin_session_origin_lsn = InvalidXLogRecPtr;
TimestampTz replorigin_session_origin_timestamp = 0;

/*
 * Base address into a shared memory array of replication states of size
 * max_replication_slots.
 *
 * XXX: Should we use a separate variable to size this rather than
 * max_replication_slots?
 */
static ReplicationState *replication_states;

/*
 * Actual shared memory block (replication_states[] is now part of this).
 */
static ReplicationStateCtl *replication_states_ctl;

/*
 * Backend-local, cached element from ReplicationState for use in a backend
 * replaying remote commits, so we don't have to search ReplicationState for
 * the backends current RepOriginId.
 */
static ReplicationState *session_replication_state = NULL;

/* Magic for on disk files. */
#define REPLICATION_STATE_MAGIC ((uint32)0x1257DADE)

static void
replorigin_check_prerequisites(bool check_slots, bool recoveryOK)
{














}

/* ---------------------------------------------------------------------------
 * Functions for working with replication origins themselves.
 * ---------------------------------------------------------------------------
 */

/*
 * Check for a persistent replication origin identified by name.
 *
 * Returns InvalidOid if the node isn't known yet and missing_ok is true.
 */
RepOriginId
replorigin_by_name(char *roname, bool missing_ok)
{
  Form_pg_replication_origin ident;
  Oid roident = InvalidOid;
  HeapTuple tuple;
  Datum roname_d;

  roname_d = CStringGetTextDatum(roname);

  tuple = SearchSysCache1(REPLORIGNAME, roname_d);
  if (HeapTupleIsValid(tuple))
  {
    ident = (Form_pg_replication_origin)GETSTRUCT(tuple);
    roident = ident->roident;
    ReleaseSysCache(tuple);
  }





  return roident;
}

/*
 * Create a replication origin.
 *
 * Needs to be called in a transaction.
 */
RepOriginId
replorigin_create(char *roname)
{
  Oid roident;
  HeapTuple tuple = NULL;
  Relation rel;
  Datum roname_d;
  SnapshotData SnapshotDirty;
  SysScanDesc scan;
  ScanKeyData key;

  roname_d = CStringGetTextDatum(roname);

  Assert(IsTransactionState());

  /*
   * We need the numeric replication origin to be 16bit wide, so we cannot
   * rely on the normal oid allocation. Instead we simply scan
   * pg_replication_origin for the first unused id. That's not particularly
   * efficient, but this should be a fairly infrequent operation - we can
   * easily spend a bit more code on this when it turns out it needs to be
   * faster.
   *
   * We handle concurrency by taking an exclusive lock (allowing reads!)
   * over the table for the duration of the search. Because we use a "dirty
   * snapshot" we can read rows that other in-progress sessions have
   * written, even though they would be invisible with normal snapshots. Due
   * to the exclusive lock there's no danger that new rows can appear while
   * we're checking.
   */
  InitDirtySnapshot(SnapshotDirty);

  rel = table_open(ReplicationOriginRelationId, ExclusiveLock);

  for (roident = InvalidOid + 1; roident < PG_UINT16_MAX; roident++)
  {
    bool nulls[Natts_pg_replication_origin];
    Datum values[Natts_pg_replication_origin];
    bool collides;

    CHECK_FOR_INTERRUPTS();

    ScanKeyInit(&key, Anum_pg_replication_origin_roident, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roident));

    scan = systable_beginscan(rel, ReplicationOriginIdentIndex, true /* indexOK */, &SnapshotDirty, 1, &key);

    collides = HeapTupleIsValid(systable_getnext(scan));

    systable_endscan(scan);

    if (!collides)
    {
      /*
       * Ok, found an unused roident, insert the new row and do a CCI,
       * so our callers can look it up if they want to.
       */
      memset(&nulls, 0, sizeof(nulls));

      values[Anum_pg_replication_origin_roident - 1] = ObjectIdGetDatum(roident);
      values[Anum_pg_replication_origin_roname - 1] = roname_d;

      tuple = heap_form_tuple(RelationGetDescr(rel), values, nulls);
      CatalogTupleInsert(rel, tuple);
      CommandCounterIncrement();
      break;
    }
  }

  /* now release lock again,	*/
  table_close(rel, ExclusiveLock);

  if (tuple == NULL)
  {

  }

  heap_freetuple(tuple);
  return roident;
}

/*
 * Drop replication origin.
 *
 * Needs to be called in a transaction.
 */
void
replorigin_drop(RepOriginId roident, bool nowait)
{
  HeapTuple tuple;
  Relation rel;
  int i;

  Assert(IsTransactionState());

  /*
   * To interlock against concurrent drops, we hold ExclusiveLock on
   * pg_replication_origin throughout this function.
   */
  rel = table_open(ReplicationOriginRelationId, ExclusiveLock);

  /*
   * First, clean up the slot state info, if there is any matching slot.
   */
restart:;;
  tuple = NULL;
  LWLockAcquire(ReplicationOriginLock, LW_EXCLUSIVE);

  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationState *state = &replication_states[i];

    if (state->roident == roident)
    {
      /* found our slot, is it busy? */
























      /* first make a WAL log entry */
      {
        xl_replorigin_drop xlrec;





      }

      /* then clear the in-memory slot */




    }
  }
  LWLockRelease(ReplicationOriginLock);
  ConditionVariableCancelSleep();

  /*
   * Now, we can delete the catalog entry.
   */
  tuple = SearchSysCache1(REPLORIGIDENT, ObjectIdGetDatum(roident));
  if (!HeapTupleIsValid(tuple))
  {

  }

  CatalogTupleDelete(rel, &tuple->t_self);
  ReleaseSysCache(tuple);

  CommandCounterIncrement();

  /* now release lock again */
  table_close(rel, ExclusiveLock);
}

/*
 * Lookup replication origin via it's oid and return the name.
 *
 * The external name is palloc'd in the calling context.
 *
 * Returns true if the origin is known, false otherwise.
 */
bool
replorigin_by_oid(RepOriginId roident, bool missing_ok, char **roname)
{




























}

/* ---------------------------------------------------------------------------
 * Functions for handling replication progress.
 * ---------------------------------------------------------------------------
 */

Size
ReplicationOriginShmemSize(void)
{
  Size size = 0;

  /*
   * XXX: max_replication_slots is arguably the wrong thing to use, as here
   * we keep the replay state of *remote* transactions. But for now it seems
   * sufficient to reuse it, rather than introduce a separate GUC.
   */
  if (max_replication_slots == 0)
  {

  }

  size = add_size(size, offsetof(ReplicationStateCtl, states));

  size = add_size(size, mul_size(max_replication_slots, sizeof(ReplicationState)));
  return size;
}

void
ReplicationOriginShmemInit(void)
{
  bool found;

  if (max_replication_slots == 0)
  {

  }

  replication_states_ctl = (ReplicationStateCtl *)ShmemInitStruct("ReplicationOriginState", ReplicationOriginShmemSize(), &found);
  replication_states = replication_states_ctl->states;

  if (!found)
  {
    int i;

    MemSet(replication_states_ctl, 0, ReplicationOriginShmemSize());

    replication_states_ctl->tranche_id = LWTRANCHE_REPLICATION_ORIGIN;

    for (i = 0; i < max_replication_slots; i++)
    {
      LWLockInitialize(&replication_states[i].lock, replication_states_ctl->tranche_id);
      ConditionVariableInit(&replication_states[i].origin_cv);
    }
  }

  LWLockRegisterTranche(replication_states_ctl->tranche_id, "replication_origin");
}

/* ---------------------------------------------------------------------------
 * Perform a checkpoint of each replication origin's progress with respect to
 * the replayed remote_lsn. Make sure that all transactions we refer to in the
 * checkpoint (local_lsn) are actually on-disk. This might not yet be the case
 * if the transactions were originally committed asynchronously.
 *
 * We store checkpoints in the following format:
 * +-------+------------------------+------------------+-----+--------+
 * | MAGIC | ReplicationStateOnDisk | struct Replic... | ... | CRC32C | EOF
 * +-------+------------------------+------------------+-----+--------+
 *
 * So its just the magic, followed by the statically sized
 * ReplicationStateOnDisk structs. Note that the maximum number of
 * ReplicationState is determined by max_replication_slots.
 * ---------------------------------------------------------------------------
 */
void
CheckPointReplicationOrigin(void)
{
  const char *tmppath = "pg_logical/replorigin_checkpoint.tmp";
  const char *path = "pg_logical/replorigin_checkpoint";
  int tmpfd;
  int i;
  uint32 magic = REPLICATION_STATE_MAGIC;
  pg_crc32c crc;

  if (max_replication_slots == 0)
  {

  }

  INIT_CRC32C(crc);

  /* make sure no old temp file is remaining */
  if (unlink(tmppath) < 0 && errno != ENOENT)
  {

  }

  /*
   * no other backend can perform this at the same time, we're protected by
   * CheckpointLock.
   */
  tmpfd = OpenTransientFile(tmppath, O_CREAT | O_EXCL | O_WRONLY | PG_BINARY);
  if (tmpfd < 0)
  {

  }

  /* write magic */
  errno = 0;
  if ((write(tmpfd, &magic, sizeof(magic))) != sizeof(magic))
  {
    /* if write didn't set errno, assume problem is no disk space */





  }
  COMP_CRC32C(crc, &magic, sizeof(magic));

  /* prevent concurrent creations/drops */
  LWLockAcquire(ReplicationOriginLock, LW_SHARED);

  /* write actual data */
  for (i = 0; i < max_replication_slots; i++)
  {
    ReplicationStateOnDisk disk_state;
    ReplicationState *curstate = &replication_states[i];
    XLogRecPtr local_lsn;

    if (curstate->roident == InvalidRepOriginId)
    {
      continue;
    }

    /* zero, to avoid uninitialized padding bytes */











    /* make sure we only write out a commit that's persistent */














  }

  LWLockRelease(ReplicationOriginLock);

  /* write out the CRC */
  FIN_CRC32C(crc);
  errno = 0;
  if ((write(tmpfd, &crc, sizeof(crc))) != sizeof(crc))
  {
    /* if write didn't set errno, assume problem is no disk space */





  }

  if (CloseTransientFile(tmpfd))
  {

  }

  /* fsync, rename to permanent file, fsync file and directory */
  durable_rename(tmppath, path, PANIC);
}

/*
 * Recover replication replay status from checkpoint data saved earlier by
 * CheckPointReplicationOrigin.
 *
 * This only needs to be called at startup and *not* during every checkpoint
 * read during recovery (e.g. in HS or PITR from a base backup) afterwards. All
 * state thereafter can be recovered by looking at commit records.
 */
void
StartupReplicationOrigin(void)
{
  const char *path = "pg_logical/replorigin_checkpoint";
  int fd;
  int readBytes;
  uint32 magic = REPLICATION_STATE_MAGIC;
  int last_state = 0;
  pg_crc32c file_crc;
  pg_crc32c crc;

  /* don't want to overwrite already existing state */
#ifdef USE_ASSERT_CHECKING
  static bool already_started = false;

  Assert(!already_started);
  already_started = true;
#endif

  if (max_replication_slots == 0)
  {

  }

  INIT_CRC32C(crc);

  elog(DEBUG2, "starting up replication origin progress state");

  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);

  /*
   * might have had max_replication_slots == 0 last run, or we just brought
   * up a standby.
   */
  if (fd < 0 && errno == ENOENT)
  {
    return;
  }
  else if (fd < 0)
  {

  }

  /* verify magic, that is written even if nothing was active */
  readBytes = read(fd, &magic, sizeof(magic));
  if (readBytes != sizeof(magic))
  {








  }
  COMP_CRC32C(crc, &magic, sizeof(magic));

  if (magic != REPLICATION_STATE_MAGIC)
  {

  }

  /* we can skip locking here, no other access is possible */

  /* recover individual states, until there are no more to be found */
  while (true)
  {
    ReplicationStateOnDisk disk_state;

    readBytes = read(fd, &disk_state, sizeof(disk_state));

    /* no further data */
    if (readBytes == sizeof(crc))
    {
      /* not pretty, but simple ... */
      file_crc = *(pg_crc32c *)&disk_state;
      break;
    }


















    /* copy data to shared memory */





  }

  /* now check checksum */
  FIN_CRC32C(crc);
  if (file_crc != crc)
  {

  }

  if (CloseTransientFile(fd))
  {

  }
}

void
replorigin_redo(XLogReaderState *record)
{





































}

/*
 * Tell the replication origin progress machinery that a commit from 'node'
 * that originated at the LSN remote_commit on the remote node was replayed
 * successfully and that we don't need to do so again. In combination with
 * setting up replorigin_session_origin_lsn and replorigin_session_origin
 * that ensures we won't loose knowledge about that after a crash if the
 * transaction had a persistent effect (think of asynchronous commits).
 *
 * local_commit needs to be a local LSN of the commit so that we can make sure
 * upon a checkpoint that enough WAL has been persisted to disk.
 *
 * Needs to be called with a RowExclusiveLock on pg_replication_origin,
 * unless running in recovery.
 */
void
replorigin_advance(RepOriginId node, XLogRecPtr remote_commit, XLogRecPtr local_commit, bool go_backward, bool wal_log)
{

















































































































}

XLogRecPtr
replorigin_get_progress(RepOriginId node, bool flush)
{


































}

/*
 * Tear down a (possibly) configured session replication origin during process
 * exit.
 */
static void
ReplicationOriginExitCleanup(int code, Datum arg)
{


















}

/*
 * Setup a replication origin in the shared memory struct if it doesn't
 * already exists and cache access to the specific ReplicationSlot so the
 * array doesn't have to be searched when calling
 * replorigin_session_advance().
 *
 * Obviously only one such cached origin can exist per process and the current
 * cached value can only be set again after the previous value is torn down
 * with replorigin_session_reset().
 */
void
replorigin_session_setup(RepOriginId node)
{







































































}

/*
 * Reset replay state previously setup in this session.
 *
 * This function may only be called if an origin was setup with
 * replorigin_session_setup().
 */
void
replorigin_session_reset(void)
{


















}

/*
 * Do the same work replorigin_advance() does, just on the session's
 * configured origin.
 *
 * This is noticeably cheaper than using replorigin_advance().
 */
void
replorigin_session_advance(XLogRecPtr remote_commit, XLogRecPtr local_commit)
{













}

/*
 * Ask the machinery about the point up to which we successfully replayed
 * changes from an already setup replication origin.
 */
XLogRecPtr
replorigin_session_get_progress(bool flush)
{
















}

/* ---------------------------------------------------------------------------
 * SQL functions for working with replication origin.
 *
 * These mostly should be fairly short wrappers around more generic functions.
 * ---------------------------------------------------------------------------
 */

/*
 * Create replication origin for the passed in name, and return the assigned
 * oid.
 */
Datum
pg_replication_origin_create(PG_FUNCTION_ARGS)
{





























}

/*
 * Drop replication origin.
 */
Datum
pg_replication_origin_drop(PG_FUNCTION_ARGS)
{















}

/*
 * Return oid of a replication origin.
 */
Datum
pg_replication_origin_oid(PG_FUNCTION_ARGS)
{















}

/*
 * Setup a replication origin for this session.
 */
Datum
pg_replication_origin_session_setup(PG_FUNCTION_ARGS)
{














}

/*
 * Reset previously setup origin in this session
 */
Datum
pg_replication_origin_session_reset(PG_FUNCTION_ARGS)
{









}

/*
 * Has a replication origin been setup for this session.
 */
Datum
pg_replication_origin_session_is_setup(PG_FUNCTION_ARGS)
{



}

/*
 * Return the replication progress for origin setup in the current session.
 *
 * If 'flush' is set to true it is ensured that the returned value corresponds
 * to a local transaction that has been flushed. This is useful if asynchronous
 * commits are used when replaying replicated transactions.
 */
Datum
pg_replication_origin_session_progress(PG_FUNCTION_ARGS)
{


















}

Datum
pg_replication_origin_xact_setup(PG_FUNCTION_ARGS)
{













}

Datum
pg_replication_origin_xact_reset(PG_FUNCTION_ARGS)
{






}

Datum
pg_replication_origin_advance(PG_FUNCTION_ARGS)
{





















}

/*
 * Return the replication progress for an individual replication origin.
 *
 * If 'flush' is set to true it is ensured that the returned value corresponds
 * to a local transaction that has been flushed. This is useful if asynchronous
 * commits are used when replaying replicated transactions.
 */
Datum
pg_replication_origin_progress(PG_FUNCTION_ARGS)
{





















}

Datum
pg_show_replication_origin_status(PG_FUNCTION_ARGS)
{


































































































}