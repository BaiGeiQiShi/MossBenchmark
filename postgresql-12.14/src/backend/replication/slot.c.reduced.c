/*-------------------------------------------------------------------------
 *
 * slot.c
 *	   Replication slot management.
 *
 *
 * Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/replication/slot.c
 *
 * NOTES
 *
 * Replication slots are used to keep state about replication streams
 * originating from this cluster.  Their primary purpose is to prevent the
 * premature removal of WAL or of old tuple versions in a manner that would
 * interfere with replication; they are also useful for monitoring purposes.
 * Slots need to be permanent (to allow restarts), crash-safe, and allocatable
 * on standbys (to support cascading setups).  The requirement that slots be
 * usable on standbys precludes storing them in the system catalogs.
 *
 * Each replication slot gets its own directory inside the $PGDATA/pg_replslot
 * directory. Inside that directory the state file will contain the slot's
 * own data. Additional data can be stored alongside that file if required.
 * While the server is running, the state data is also cached in memory for
 * efficiency.
 *
 * ReplicationSlotAllocationLock must be taken in exclusive mode to allocate
 * or free a slot. ReplicationSlotControlLock must be taken in shared mode
 * to iterate over the slots, and in exclusive mode to change the in_use flag
 * of a slot.  The remaining data in each slot is protected by its mutex.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <unistd.h>
#include <sys/stat.h>

#include "access/transam.h"
#include "access/xlog_internal.h"
#include "common/string.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/slot.h"
#include "storage/fd.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/builtins.h"

/*
 * Replication slot on-disk data structure.
 */
typedef struct ReplicationSlotOnDisk {
  /* first part of this struct needs to be version independent */

  /* data not covered by checksum */
  uint32 magic;
  pg_crc32c checksum;

  /* data covered by checksum */
  uint32 version;
  uint32 length;

  /*
   * The actual data in the slot that follows can differ based on the above
   * 'version'.
   */

  ReplicationSlotPersistentData slotdata;
} ReplicationSlotOnDisk;

/* size of version independent data */
#define ReplicationSlotOnDiskConstantSize                                      \

/* size of the part of the slot not covered by the checksum */
#define SnapBuildOnDiskNotChecksummedSize                                      \
  offsetof(ReplicationSlotOnDisk, version)
/* size of the part covered by the checksum */
#define SnapBuildOnDiskChecksummedSize                                         \
  sizeof(ReplicationSlotOnDisk) - SnapBuildOnDiskNotChecksummedSize
/* size of the slot data that is version dependent */
#define ReplicationSlotOnDiskV2Size                                            \





/* Control array for replication slot management */
ReplicationSlotCtlData *ReplicationSlotCtl = NULL;

/* My backend's replication slot in the shared memory array */
ReplicationSlot *MyReplicationSlot = NULL;

/* GUCs */
int max_replication_slots = 0; /* the maximum number of replication
                                * slots */

static void
ReplicationSlotDropAcquired(void);
static void
ReplicationSlotDropPtr(ReplicationSlot *slot);

/* internal persistency functions */
static void
RestoreSlotFromDisk(const char *name);
static void
CreateSlotOnDisk(ReplicationSlot *slot);
static void
SaveSlotToPath(ReplicationSlot *slot, const char *path, int elevel);

/*
 * Report shared-memory space needed by ReplicationSlotShmemInit.
 */
Size
ReplicationSlotsShmemSize(void)
{
  Size size = 0;

  if (max_replication_slots == 0) {



  size = offsetof(ReplicationSlotCtlData, replication_slots);
  size =
      add_size(size, mul_size(max_replication_slots, sizeof(ReplicationSlot)));

  return size;
}

/*
 * Allocate and initialize shared memory for replication slots.
 */
void
ReplicationSlotsShmemInit(void)
{
  bool found;

  if (max_replication_slots == 0) {



  ReplicationSlotCtl = (ReplicationSlotCtlData *)ShmemInitStruct(
      "ReplicationSlot Ctl", ReplicationSlotsShmemSize(), &found);

  LWLockRegisterTranche(LWTRANCHE_REPLICATION_SLOT_IO_IN_PROGRESS,
                        "replication_slot_io");

  if (!found) {
    int i;

    /* First time through, so initialize */
    MemSet(ReplicationSlotCtl, 0, ReplicationSlotsShmemSize());

    for (i = 0; i < max_replication_slots; i++) {
      ReplicationSlot *slot = &ReplicationSlotCtl->replication_slots[i];

      /* everything else is zeroed by the memset above */
      SpinLockInit(&slot->mutex);
      LWLockInitialize(&slot->io_in_progress_lock,
                       LWTRANCHE_REPLICATION_SLOT_IO_IN_PROGRESS);
      ConditionVariableInit(&slot->active_cv);
    }
  }
}

/*
 * Check whether the passed slot name is valid and report errors at elevel.
 *
 * Slot names may consist out of [a-z0-9_]{1,NAMEDATALEN-1} which should allow
 * the name to be used as a directory name on every supported OS.
 *
 * Returns whether the directory name is valid or not if elevel < ERROR.
 */
bool
ReplicationSlotValidateName(const char *name, int elevel)
{




























}

/*
 * Create a new replication slot and mark it as used by this backend.
 *
 * name: Name of the slot
 * db_specific: logical decoding is db specific; if the slot is going to
 *	   be used for that pass true, otherwise false.
 */
void
ReplicationSlotCreate(const char *name, bool db_specific,
                      ReplicationSlotPersistency persistency)
{




































































































}

/*
 * Find a previously created slot and mark it as used by this backend.
 */
void
ReplicationSlotAcquire(const char *name, bool nowait)
{















































































}

/*
 * Release the replication slot that this backend considers to own.
 *
 * This or another backend can re-acquire the slot later.
 * Resources this slot requires will be preserved.
 */
void
ReplicationSlotRelease(void)
{












































}

/*
 * Cleanup all temporary slots created in current session.
 */
void
ReplicationSlotCleanup(void)
{
  int i;

  Assert(MyReplicationSlot == NULL);

restart:;
  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++) {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];

    if (!s->in_use) {
      continue;
    }
















  LWLockRelease(ReplicationSlotControlLock);
}

/*
 * Permanently drop replication slot identified by the passed in name.
 */
void
ReplicationSlotDrop(const char *name, bool nowait)
{





}

/*
 * Permanently drop the currently acquired replication slot.
 */
static void
ReplicationSlotDropAcquired(void)
{








}

/*
 * Permanently drop the replication slot which will be released by the point
 * this function returns.
 */
static void
ReplicationSlotDropPtr(ReplicationSlot *slot)
{






















































































}

/*
 * Serialize the currently acquired slot's state from memory to disk, thereby
 * guaranteeing the current state will survive a crash.
 */
void
ReplicationSlotSave(void)
{






}

/*
 * Signal that it would be useful if the currently acquired slot would be
 * flushed out to disk.
 *
 * Note that the actual flush to disk can be delayed for a long time, if
 * required for correctness explicitly do a ReplicationSlotSave().
 */
void
ReplicationSlotMarkDirty(void)
{








}

/*
 * Convert a slot that's marked as RS_EPHEMERAL to a RS_PERSISTENT slot,
 * guaranteeing it will be there after an eventual crash.
 */
void
ReplicationSlotPersist(void)
{











}

/*
 * Compute the oldest xmin across all slots and store it in the ProcArray.
 *
 * If already_locked is true, ProcArrayLock has already been acquired
 * exclusively.
 */
void
ReplicationSlotsComputeRequiredXmin(bool already_locked)
{
  int i;
  TransactionId agg_xmin = InvalidTransactionId;
  TransactionId agg_catalog_xmin = InvalidTransactionId;

  Assert(ReplicationSlotCtl != NULL);

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

  for (i = 0; i < max_replication_slots; i++) {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];
    TransactionId effective_xmin;
    TransactionId effective_catalog_xmin;

    if (!s->in_use) {
      continue;
    }





















  LWLockRelease(ReplicationSlotControlLock);

  ProcArraySetReplicationSlotXmin(agg_xmin, agg_catalog_xmin, already_locked);
}

/*
 * Compute the oldest restart LSN across all slots and inform xlog module.
 */
void
ReplicationSlotsComputeRequiredLSN(void)
{
  int i;
  XLogRecPtr min_required = InvalidXLogRecPtr;

  Assert(ReplicationSlotCtl != NULL);

  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);
  for (i = 0; i < max_replication_slots; i++) {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];
    XLogRecPtr restart_lsn;

    if (!s->in_use) {
      continue;
    }










  LWLockRelease(ReplicationSlotControlLock);

  XLogSetReplicationSlotMinimumLSN(min_required);
}

/*
 * Compute the oldest WAL LSN required by *logical* decoding slots..
 *
 * Returns InvalidXLogRecPtr if logical decoding is disabled or no logical
 * slots exist.
 *
 * NB: this returns a value >= ReplicationSlotsComputeRequiredLSN(), since it
 * ignores physical replication slots.
 *
 * The results aren't required frequently, so we don't maintain a precomputed
 * value like we do for ComputeRequiredLSN() and ComputeRequiredXmin().
 */
XLogRecPtr
ReplicationSlotsComputeLogicalRestartLSN(void)
{
  XLogRecPtr result = InvalidXLogRecPtr;
  int i;

  if (max_replication_slots <= 0) {



  LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

  for (i = 0; i < max_replication_slots; i++) {
    ReplicationSlot *s;
    XLogRecPtr restart_lsn;

    s = &ReplicationSlotCtl->replication_slots[i];

    /* cannot change while ReplicationSlotCtlLock is held */
    if (!s->in_use) {
      continue;
    }
















  LWLockRelease(ReplicationSlotControlLock);

  return result;
}

/*
 * ReplicationSlotsCountDBSlots -- count the number of slots that refer to the
 * passed database oid.
 *
 * Returns true if there are any slots referencing the database. *nslots will
 * be set to the absolute number of slots in the database, *nactive to ones
 * currently active.
 */
bool
ReplicationSlotsCountDBSlots(Oid dboid, int *nslots, int *nactive)
{











































}

/*
 * ReplicationSlotsDropDBSlots -- Drop all db-specific slots relating to the
 * passed database oid. The caller should hold an exclusive lock on the
 * pg_database oid for the database to prevent creation of new slots on the db
 * or replay from existing slots.
 *
 * Another session that concurrently acquires an existing slot on the target DB
 * (most likely to drop it) may cause this function to ERROR. If that happens
 * it may have dropped some but not all slots.
 *
 * This routine isn't as efficient as it could be - but we don't drop
 * databases often, especially databases with lots of slots.
 */
void
ReplicationSlotsDropDBSlots(Oid dboid)
{




































































}

/*
 * Check whether the server's configuration supports using replication
 * slots.
 */
void
CheckSlotRequirements(void)
{

















}

/*
 * Reserve WAL for the currently active slot.
 *
 * Compute and set restart_lsn in a manner that's appropriate for the type of
 * the slot and concurrency safe.
 */
void
ReplicationSlotReserveWal(void)
{































































}

/*
 * Flush all replication slots to disk.
 *
 * This needn't actually be part of a checkpoint, but it's a convenient
 * location.
 */
void
CheckPointReplicationSlots(void)
{
  int i;

  elog(DEBUG1, "performing replication slot checkpoint");

  /*
   * Prevent any slot from being created/dropped while we're active. As we
   * explicitly do *not* want to block iterating over replication_slots or
   * acquiring a slot we cannot take the control lock - but that's OK,
   * because holding ReplicationSlotAllocationLock is strictly stronger, and
   * enough to guarantee that nobody can change the in_use bits on us.
   */
  LWLockAcquire(ReplicationSlotAllocationLock, LW_SHARED);

  for (i = 0; i < max_replication_slots; i++) {
    ReplicationSlot *s = &ReplicationSlotCtl->replication_slots[i];
    char path[MAXPGPATH];

    if (!s->in_use) {
      continue;
    }





  LWLockRelease(ReplicationSlotAllocationLock);
}

/*
 * Load all replication slots from disk into memory at server startup. This
 * needs to be run before we start crash recovery.
 */
void
StartupReplicationSlots(void)
{
  DIR *replication_dir;
  struct dirent *replication_de;

  elog(DEBUG1, "starting up replication slots");

  /* restore all slots by iterating over all on-disk entries */
  replication_dir = AllocateDir("pg_replslot");
  while ((replication_de = ReadDir(replication_dir, "pg_replslot")) != NULL) {
    struct stat statbuf;
    char path[MAXPGPATH + 12];

    if (strcmp(replication_de->d_name, ".") == 0 ||
        strcmp(replication_de->d_name, "..") == 0) {
      continue;
    }





















  FreeDir(replication_dir);

  /* currently no slots exist, we're done. */
  if (max_replication_slots <= 0) {



  /* Now that we have recovered all the data, compute replication xmin */
  ReplicationSlotsComputeRequiredXmin(false);
  ReplicationSlotsComputeRequiredLSN();
}

/* ----
 * Manipulation of on-disk state of replication slots
 *
 * NB: none of the routines below should take any notice whether a slot is the
 * current one or not, that's all handled a layer above.
 * ----
 */
static void
CreateSlotOnDisk(ReplicationSlot *slot)
{





















































}

/*
 * Shared functionality between saving and creating a replication slot.
 */
static void
SaveSlotToPath(ReplicationSlot *slot, const char *dir, int elevel)
{





































































































































}

/*
 * Load a single slot from disk into memory.
 */
static void
RestoreSlotFromDisk(const char *name)
{







































































































































































































}
