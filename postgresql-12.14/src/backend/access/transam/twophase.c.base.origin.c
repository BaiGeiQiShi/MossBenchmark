/*-------------------------------------------------------------------------
 *
 * twophase.c
 *		Two-phase commit support functions.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/backend/access/transam/twophase.c
 *
 * NOTES
 *		Each global transaction is associated with a global transaction
 *		identifier (GID). The client assigns a GID to a postgres
 *		transaction with the PREPARE TRANSACTION command.
 *
 *		We keep all active global transactions in a shared memory array.
 *		When the PREPARE TRANSACTION command is issued, the GID is
 *		reserved for the transaction in the array. This is done before
 *		a WAL entry is made, because the reservation checks for
 *duplicate GIDs and aborts the transaction if there already is a global
 *		transaction in prepared state with the same GID.
 *
 *		A global transaction (gxact) also has dummy PGXACT and PGPROC;
 *this is what keeps the XID considered running by TransactionIdIsInProgress. It
 *is also convenient as a PGPROC to hook the gxact's locks to.
 *
 *		Information to recover prepared transactions in case of crash is
 *		now stored in WAL for the common case. In some cases there will
 *be an extended period between preparing a GXACT and commit/abort, in which
 *case we need to separately record prepared transaction data in permanent
 *storage. This includes locking information, pending notifications etc. All
 *that state information is written to the per-transaction state file in the
 *pg_twophase directory. All prepared transactions will be written prior to
 *shutdown.
 *
 *		Life track of state data is following:
 *
 *		* On PREPARE TRANSACTION backend writes state data only to the
 *WAL and stores pointer to the start of the WAL record in
 *		  gxact->prepare_start_lsn.
 *		* If COMMIT occurs before checkpoint then backend reads data
 *from WAL using prepare_start_lsn.
 *		* On checkpoint state data copied to files in pg_twophase
 *directory and fsynced
 *		* If COMMIT happens after checkpoint then backend reads state
 *data from files
 *
 *		During replay and replication, TwoPhaseState also holds
 *information about active prepared transactions that haven't been moved to disk
 *yet.
 *
 *		Replay of twophase records happens by the following rules:
 *
 *		* At the beginning of recovery, pg_twophase is scanned once,
 *filling TwoPhaseState with entries marked with gxact->inredo and
 *		  gxact->ondisk.  Two-phase file data older than the XID horizon
 *of the redo position are discarded.
 *		* On PREPARE redo, the transaction is added to
 *TwoPhaseState->prepXacts. gxact->inredo is set to true for such entries.
 *		* On Checkpoint we iterate through TwoPhaseState->prepXacts
 *entries that have gxact->inredo set and are behind the redo_horizon. We save
 *them to disk and then switch gxact->ondisk to true.
 *		* On COMMIT/ABORT we delete the entry from
 *TwoPhaseState->prepXacts. If gxact->ondisk is true, the corresponding entry
 *from the disk is additionally deleted.
 *		* RecoverPreparedTransactions(),
 *StandbyRecoverPreparedTransactions() and PrescanPreparedTransactions() have
 *been modified to go through gxact->inredo entries that have not made it to
 *disk.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "access/commit_ts.h"
#include "access/htup_details.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "access/xlogreader.h"
#include "catalog/pg_type.h"
#include "catalog/storage.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "replication/origin.h"
#include "replication/syncrep.h"
#include "replication/walsender.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/md.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/timestamp.h"

/*
 * Directory where Two-phase commit files reside within PGDATA
 */
#define TWOPHASE_DIR "pg_twophase"

/* GUC variable, can't be changed after startup */
int max_prepared_xacts = 0;

/*
 * This struct describes one global transaction that is in prepared state
 * or attempting to become prepared.
 *
 * The lifecycle of a global transaction is:
 *
 * 1. After checking that the requested GID is not in use, set up an entry in
 * the TwoPhaseState->prepXacts array with the correct GID and valid = false,
 * and mark it as locked by my backend.
 *
 * 2. After successfully completing prepare, set valid = true and enter the
 * referenced PGPROC into the global ProcArray.
 *
 * 3. To begin COMMIT PREPARED or ROLLBACK PREPARED, check that the entry is
 * valid and not locked, then mark the entry as locked by storing my current
 * backend ID into locking_backend.  This prevents concurrent attempts to
 * commit or rollback the same prepared xact.
 *
 * 4. On completion of COMMIT PREPARED or ROLLBACK PREPARED, remove the entry
 * from the ProcArray and the TwoPhaseState->prepXacts array and return it to
 * the freelist.
 *
 * Note that if the preparing transaction fails between steps 1 and 2, the
 * entry must be removed so that the GID and the GlobalTransaction struct
 * can be reused.  See AtAbort_Twophase().
 *
 * typedef struct GlobalTransactionData *GlobalTransaction appears in
 * twophase.h
 */

typedef struct GlobalTransactionData
{
  GlobalTransaction next;   /* list link for free list */
  int pgprocno;             /* ID of associated dummy PGPROC */
  BackendId dummyBackendId; /* similar to backend id for backends */
  TimestampTz prepared_at;  /* time of preparation */

  /*
   * Note that we need to keep track of two LSNs for each GXACT. We keep
   * track of the start LSN because this is the address we must use to read
   * state data back from WAL when committing a prepared GXACT. We keep
   * track of the end LSN because that is the LSN we need to wait for prior
   * to commit.
   */
  XLogRecPtr prepare_start_lsn; /* XLOG offset of prepare record start */
  XLogRecPtr prepare_end_lsn;   /* XLOG offset of prepare record end */
  TransactionId xid;            /* The GXACT id */

  Oid owner;                 /* ID of user that executed the xact */
  BackendId locking_backend; /* backend currently working on the xact */
  bool valid;                /* true if PGPROC entry is in proc array */
  bool ondisk;               /* true if prepare state file is on disk */
  bool inredo;               /* true if entry was added via xlog_redo */
  char gid[GIDSIZE];         /* The GID assigned to the prepared xact */
} GlobalTransactionData;

/*
 * Two Phase Commit shared state.  Access to this struct is protected
 * by TwoPhaseStateLock.
 */
typedef struct TwoPhaseStateData
{
  /* Head of linked list of free GlobalTransactionData structs */
  GlobalTransaction freeGXacts;

  /* Number of valid prepXacts entries. */
  int numPrepXacts;

  /* There are max_prepared_xacts items in this array */
  GlobalTransaction prepXacts[FLEXIBLE_ARRAY_MEMBER];
} TwoPhaseStateData;

static TwoPhaseStateData *TwoPhaseState;

/*
 * Global transaction entry currently locked by us, if any.  Note that any
 * access to the entry pointed to by this variable must be protected by
 * TwoPhaseStateLock, though obviously the pointer itself doesn't need to be
 * (since it's just local memory).
 */
static GlobalTransaction MyLockedGxact = NULL;

static bool twophaseExitRegistered = false;

static void
RecordTransactionCommitPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, int ninvalmsgs, SharedInvalidationMessage *invalmsgs, bool initfileinval, const char *gid);
static void
RecordTransactionAbortPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, const char *gid);
static void
ProcessRecords(char *bufptr, TransactionId xid, const TwoPhaseCallback callbacks[]);
static void
RemoveGXact(GlobalTransaction gxact);

static void
XlogReadTwoPhaseData(XLogRecPtr lsn, char **buf, int *len);
static char *
ProcessTwoPhaseBuffer(TransactionId xid, XLogRecPtr prepare_start_lsn, bool fromdisk, bool setParent, bool setNextXid);
static void
MarkAsPreparingGuts(GlobalTransaction gxact, TransactionId xid, const char *gid, TimestampTz prepared_at, Oid owner, Oid databaseid);
static void
RemoveTwoPhaseFile(TransactionId xid, bool giveWarning);
static void
RecreateTwoPhaseFile(TransactionId xid, void *content, int len);

/*
 * Initialization of shared memory
 */
Size
TwoPhaseShmemSize(void)
{









}

void
TwoPhaseShmemInit(void)
{












































}

/*
 * Exit hook to unlock the global transaction entry we're working on.
 */
static void
AtProcExit_Twophase(int code, Datum arg)
{


}

/*
 * Abort hook to unlock the global transaction entry we're working on.
 */
void
AtAbort_Twophase(void)
{





































}

/*
 * This is called after we have finished transferring state to the prepared
 * PGXACT entry.
 */
void
PostPrepare_Twophase(void)
{





}

/*
 * MarkAsPreparing
 *		Reserve the GID for the given transaction.
 */
GlobalTransaction
MarkAsPreparing(TransactionId xid, const char *gid, TimestampTz prepared_at, Oid owner, Oid databaseid)
{




















































}

/*
 * MarkAsPreparingGuts
 *
 * This uses a gxact struct and puts it into the active array.
 * NOTE: this is also used when reloading a gxact after a crash; so avoid
 * assuming that we can use very much backend context.
 *
 * Note: This function should be called with appropriate locks held.
 */
static void
MarkAsPreparingGuts(GlobalTransaction gxact, TransactionId xid, const char *gid, TimestampTz prepared_at, Oid owner, Oid databaseid)
{































































}

/*
 * GXactLoadSubxactData
 *
 * If the transaction being persisted had any subtransactions, this must
 * be called before MarkAsPrepared() to load information into the dummy
 * PGPROC.
 */
static void
GXactLoadSubxactData(GlobalTransaction gxact, int nsubxacts, TransactionId *children)
{














}

/*
 * MarkAsPrepared
 *		Mark the GXACT as fully valid, and enter it into the global
 *ProcArray.
 *
 * lock_held indicates whether caller already holds TwoPhaseStateLock.
 */
static void
MarkAsPrepared(GlobalTransaction gxact, bool lock_held)
{

















}

/*
 * LockGXact
 *		Locate the prepared transaction and mark it busy for COMMIT or
 *PREPARE.
 */
static GlobalTransaction
LockGXact(const char *gid, Oid user)
{































































}

/*
 * RemoveGXact
 *		Remove the prepared transaction from the shared memory array.
 *
 * NB: caller should have already removed it from ProcArray
 */
static void
RemoveGXact(GlobalTransaction gxact)
{





















}

/*
 * Returns an array of all prepared transactions for the user-level
 * function pg_prepared_xact.
 *
 * The returned array and all its elements are copies of internal data
 * structures, to minimize the time we need to hold the TwoPhaseStateLock.
 *
 * WARNING -- we return even those transactions that are not fully prepared
 * yet.  The caller should filter them out if he doesn't want them.
 *
 * The returned array is palloc'd.
 */
static int
GetPreparedTransactionList(GlobalTransaction *gxacts)
{

























}

/* Working status for pg_prepared_xact */
typedef struct
{
  GlobalTransaction array;
  int ngxacts;
  int currIdx;
} Working_State;

/*
 * pg_prepared_xact
 *		Produce a view with one row per prepared transaction.
 *
 * This function is here so we don't have to export the
 * GlobalTransactionData struct definition.
 */
Datum
pg_prepared_xact(PG_FUNCTION_ARGS)
{












































































}

/*
 * TwoPhaseGetGXact
 *		Get the GlobalTransaction struct for a prepared transaction
 *		specified by XID
 *
 * If lock_held is set to true, TwoPhaseStateLock will not be taken, so the
 * caller had better hold it.
 */
static GlobalTransaction
TwoPhaseGetGXact(TransactionId xid, bool lock_held)
{
















































}

/*
 * TwoPhaseGetXidByVirtualXID
 *		Lookup VXID among xacts prepared since last startup.
 *
 * (This won't find recovered xacts.)  If more than one matches, return any
 * and set "have_more" to true.  To witness multiple matches, a single
 * BackendId must consume 2^32 LXIDs, with no intervening database restart.
 */
TransactionId
TwoPhaseGetXidByVirtualXID(VirtualTransactionId vxid, bool *have_more)
{



































}

/*
 * TwoPhaseGetDummyBackendId
 *		Get the dummy backend ID for prepared transaction specified by
 *XID
 *
 * Dummy backend IDs are similar to real backend IDs of real backends.
 * They start at MaxBackends + 1, and are unique across all currently active
 * real backends and prepared transactions.  If lock_held is set to true,
 * TwoPhaseStateLock will not be taken, so the caller had better hold it.
 */
BackendId
TwoPhaseGetDummyBackendId(TransactionId xid, bool lock_held)
{



}

/*
 * TwoPhaseGetDummyProc
 *		Get the PGPROC that represents a prepared transaction specified
 *by XID
 *
 * If lock_held is set to true, TwoPhaseStateLock will not be taken, so the
 * caller had better hold it.
 */
PGPROC *
TwoPhaseGetDummyProc(TransactionId xid, bool lock_held)
{



}

/************************************************************************/
/* State file support
 */
/************************************************************************/

#define TwoPhaseFilePath(path, xid) snprintf(path, MAXPGPATH, TWOPHASE_DIR "/%08X", xid)

/*
 * 2PC state file format:
 *
 *	1. TwoPhaseFileHeader
 *	2. TransactionId[] (subtransactions)
 *	3. RelFileNode[] (files to be deleted at commit)
 *	4. RelFileNode[] (files to be deleted at abort)
 *	5. SharedInvalidationMessage[] (inval messages to be sent at commit)
 *	6. TwoPhaseRecordOnDisk
 *	7. ...
 *	8. TwoPhaseRecordOnDisk (end sentinel, rmid == TWOPHASE_RM_END_ID)
 *	9. checksum (CRC-32C)
 *
 * Each segment except the final checksum is MAXALIGN'd.
 */

/*
 * Header for a 2PC state file
 */
#define TWOPHASE_MAGIC 0x57F94534 /* format identifier */

typedef struct TwoPhaseFileHeader
{
  uint32 magic;                 /* format identifier */
  uint32 total_len;             /* actual file length */
  TransactionId xid;            /* original transaction XID */
  Oid database;                 /* OID of database it was in */
  TimestampTz prepared_at;      /* time of preparation */
  Oid owner;                    /* user running the transaction */
  int32 nsubxacts;              /* number of following subxact XIDs */
  int32 ncommitrels;            /* number of delete-on-commit rels */
  int32 nabortrels;             /* number of delete-on-abort rels */
  int32 ninvalmsgs;             /* number of cache invalidation messages */
  bool initfileinval;           /* does relcache init file need invalidation? */
  uint16 gidlen;                /* length of the GID - GID follows the header */
  XLogRecPtr origin_lsn;        /* lsn of this record at origin node */
  TimestampTz origin_timestamp; /* time of prepare at origin node */
} TwoPhaseFileHeader;

/*
 * Header for each record in a state file
 *
 * NOTE: len counts only the rmgr data, not the TwoPhaseRecordOnDisk header.
 * The rmgr data will be stored starting on a MAXALIGN boundary.
 */
typedef struct TwoPhaseRecordOnDisk
{
  uint32 len;          /* length of rmgr data */
  TwoPhaseRmgrId rmid; /* resource manager for this record */
  uint16 info;         /* flag bits for use by rmgr */
} TwoPhaseRecordOnDisk;

/*
 * During prepare, the state file is assembled in memory before writing it
 * to WAL and the actual state file.  We use a chain of StateFileChunk blocks
 * for that.
 */
typedef struct StateFileChunk
{
  char *data;
  uint32 len;
  struct StateFileChunk *next;
} StateFileChunk;

static struct xllist
{
  StateFileChunk *head; /* first data block in the chain */
  StateFileChunk *tail; /* last block in chain */
  uint32 num_chunks;
  uint32 bytes_free; /* free bytes left in tail block */
  uint32 total_len;  /* total data bytes in chain */
} records;

/*
 * Append a block of data to records data structure.
 *
 * NB: each block is padded to a MAXALIGN multiple.  This must be
 * accounted for when the file is later read!
 *
 * The data is copied, so the caller is free to modify it afterwards.
 */
static void
save_state_data(const void *data, uint32 len)
{


















}

/*
 * Start preparing a state file.
 *
 * Initializes data structure and inserts the 2PC file header record.
 */
void
StartPrepare(GlobalTransaction gxact)
{































































}

/*
 * Finish preparing state data and writing it to WAL.
 */
void
EndPrepare(GlobalTransaction gxact)
{






















































































































}

/*
 * Register a 2PC record to be written to state file.
 */
void
RegisterTwoPhaseRecord(TwoPhaseRmgrId rmid, uint16 info, const void *data, uint32 len)
{










}

/*
 * Read and validate the state file for xid.
 *
 * If it looks OK (has a valid magic number and CRC), return the palloc'd
 * contents of the file, issuing an error when finding corrupted data.  If
 * missing_ok is true, which indicates that missing files can be safely
 * ignored, then return NULL.  This state can be reached when doing recovery.
 */
static char *
ReadTwoPhaseFile(TransactionId xid, bool missing_ok)
{





























































































}

/*
 * ParsePrepareRecord
 */
void
ParsePrepareRecord(uint8 info, char *xlrec, xl_xact_parsed_prepare *parsed)
{





























}

/*
 * Reads 2PC data from xlog. During checkpoint this data will be moved to
 * twophase files and ReadTwoPhaseFile should be used instead.
 *
 * Note clearly that this function can access WAL during normal operation,
 * similarly to the way WALSender or Logical Decoding would do.  While
 * accessing WAL, read_local_xlog_page() may change ThisTimeLineID,
 * particularly if this routine is called for the end-of-recovery checkpoint
 * in the checkpointer itself, so save the current timeline number value
 * and restore it once done.
 */
static void
XlogReadTwoPhaseData(XLogRecPtr lsn, char **buf, int *len)
{














































}

/*
 * Confirms an xid is prepared, during recovery
 */
bool
StandbyTransactionIdIsPrepared(TransactionId xid)
{
























}

/*
 * FinishPreparedTransaction: execute COMMIT PREPARED or ROLLBACK PREPARED
 */
void
FinishPreparedTransaction(const char *gid, bool isCommit)
{











































































































































































}

/*
 * Scan 2PC state data in memory and call the indicated callbacks for each 2PC
 * record.
 */
static void
ProcessRecords(char *bufptr, TransactionId xid, const TwoPhaseCallback callbacks[])
{



















}

/*
 * Remove the 2PC file for the specified XID.
 *
 * If giveWarning is false, do not complain about file-not-present;
 * this is an expected case during WAL replay.
 */
static void
RemoveTwoPhaseFile(TransactionId xid, bool giveWarning)
{










}

/*
 * Recreates a state file. This is used in WAL replay and during
 * checkpoint creation.
 *
 * Note: content and len don't include CRC.
 */
static void
RecreateTwoPhaseFile(TransactionId xid, void *content, int len)
{























































}

/*
 * CheckPointTwoPhase -- handle 2PC component of checkpointing.
 *
 * We must fsync the state file of any GXACT that is valid or has been
 * generated during redo and has a PREPARE LSN <= the checkpoint's redo
 * horizon.  (If the gxact isn't valid yet, has not been generated in
 * redo, or has a later LSN, this checkpoint is not responsible for
 * fsyncing it.)
 *
 * This is deliberately run as late as possible in the checkpoint sequence,
 * because GXACTs ordinarily have short lifespans, and so it is quite
 * possible that GXACTs that were valid at checkpoint start will no longer
 * exist if we wait a little bit. With typical checkpoint settings this
 * will be about 3 minutes for an online checkpoint, so as a result we
 * expect that there will be no GXACTs that need to be copied to disk.
 *
 * If a GXACT remains valid across multiple checkpoints, it will already
 * be on disk so we don't bother to repeat that write.
 */
void
CheckPointTwoPhase(XLogRecPtr redo_horizon)
{

































































}

/*
 * restoreTwoPhaseData
 *
 * Scan pg_twophase and fill TwoPhaseState depending on the on-disk data.
 * This is called once at the beginning of recovery, saving any extra
 * lookups in the future.  Two-phase files that are newer than the
 * minimum XID horizon are discarded on the way.
 */
void
restoreTwoPhaseData(void)
{

























}

/*
 * PrescanPreparedTransactions
 *
 * Scan the shared memory entries of TwoPhaseState and determine the range
 * of valid XIDs present.  This is run during database startup, after we
 * have completed reading WAL.  ShmemVariableCache->nextFullXid has been set to
 * one more than the highest XID for which evidence exists in WAL.
 *
 * We throw away any prepared xacts with main XID beyond nextFullXid --- if any
 * are present, it suggests that the DBA has done a PITR recovery to an
 * earlier point in time without cleaning out pg_twophase.  We dare not
 * try to recover such prepared xacts since they likely depend on database
 * state that doesn't exist now.
 *
 * However, we will advance nextFullXid beyond any subxact XIDs belonging to
 * valid prepared xacts.  We need to do this since subxact commit doesn't
 * write a WAL entry, and so there might be no evidence in WAL of those
 * subxact XIDs.
 *
 * On corrupted two-phase files, fail immediately.  Keeping around broken
 * entries and let replay continue causes harm on the system, and a new
 * backup should be rolled in.
 *
 * Our other responsibility is to determine and return the oldest valid XID
 * among the prepared xacts (if none, return ShmemVariableCache->nextFullXid).
 * This is needed to synchronize pg_subtrans startup properly.
 *
 * If xids_p and nxids_p are not NULL, pointer to a palloc'd array of all
 * top-level xids is stored in *xids_p. The number of entries in the array
 * is returned in *nxids_p.
 */
TransactionId
PrescanPreparedTransactions(TransactionId **xids_p, int *nxids_p)
{
































































}

/*
 * StandbyRecoverPreparedTransactions
 *
 * Scan the shared memory entries of TwoPhaseState and setup all the required
 * information to allow standby queries to treat prepared transactions as still
 * active.
 *
 * This is never called at the end of recovery - we use
 * RecoverPreparedTransactions() at that point.
 *
 * The lack of calls to SubTransSetParent() calls here is by design;
 * those calls are made by RecoverPreparedTransactions() at the end of recovery
 * for those xacts that need this.
 */
void
StandbyRecoverPreparedTransactions(void)
{




















}

/*
 * RecoverPreparedTransactions
 *
 * Scan the shared memory entries of TwoPhaseState and reload the state for
 * each prepared transaction (reacquire locks, etc).
 *
 * This is run at the end of recovery, but before we allow backends to write
 * WAL.
 *
 * At the end of recovery the way we take snapshots will change. We now need
 * to mark all running transactions with their full SubTransSetParent() info
 * to allow normal snapshots to work correctly if snapshots overflow.
 * We do this here because by definition prepared transactions are the only
 * type of write transaction still running, so this is necessary and
 * complete.
 */
void
RecoverPreparedTransactions(void)
{




















































































}

/*
 * ProcessTwoPhaseBuffer
 *
 * Given a transaction id, read it either from disk or read it directly
 * via shmem xlog record pointer using the provided "prepare_start_lsn".
 *
 * If setParent is true, set up subtransaction parent linkages.
 *
 * If setNextXid is true, set ShmemVariableCache->nextFullXid to the newest
 * value scanned.
 */
static char *
ProcessTwoPhaseBuffer(TransactionId xid, XLogRecPtr prepare_start_lsn, bool fromdisk, bool setParent, bool setNextXid)
{































































































}

/*
 *	RecordTransactionCommitPrepared
 *
 * This is basically the same as RecordTransactionCommit (q.v. if you change
 * this function): in particular, we must set the delayChkpt flag to avoid a
 * race condition.
 *
 * We know the transaction made at least one XLOG entry (its PREPARE),
 * so it is never possible to optimize out the commit record.
 */
static void
RecordTransactionCommitPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, int ninvalmsgs, SharedInvalidationMessage *invalmsgs, bool initfileinval, const char *gid)
{




































































}

/*
 *	RecordTransactionAbortPrepared
 *
 * This is basically the same as RecordTransactionAbort.
 *
 * We know the transaction made at least one XLOG entry (its PREPARE),
 * so it is never possible to optimize out the abort record.
 */
static void
RecordTransactionAbortPrepared(TransactionId xid, int nchildren, TransactionId *children, int nrels, RelFileNode *rels, const char *gid)
{






































}

/*
 * PrepareRedoAdd
 *
 * Store pointers to the start/end of the WAL record along with the xid in
 * a gxact entry in shared memory TwoPhaseState structure.  If caller
 * specifies InvalidXLogRecPtr as WAL location to fetch the two-phase
 * data, the entry is marked as located on disk.
 */
void
PrepareRedoAdd(char *buf, XLogRecPtr start_lsn, XLogRecPtr end_lsn, RepOriginId origin_id)
{




















































}

/*
 * PrepareRedoRemove
 *
 * Remove the corresponding gxact entry from TwoPhaseState. Also remove
 * the 2PC file if a prepared transaction was saved via an earlier checkpoint.
 *
 * Caller must hold TwoPhaseStateLock in exclusive mode, because TwoPhaseState
 * is updated.
 */
void
PrepareRedoRemove(TransactionId xid, bool giveWarning)
{






































}