                                                                            
   
          
                                     
   
                                                                         
                                                                        
   
   
                  
                                     
   
         
                                                       
                                                                
                                                                
                             
   
                                                               
                                                      
   
              
   
                                                                 
                                                   
                                     
   
                                                                            
   
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/transam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/spin.h"
#include "storage/standby.h"
#include "utils/memutils.h"
#include "utils/ps_status.h"
#include "utils/resowner_private.h"

                                                                    
int max_locks_per_xact;                   

#define NLOCKENTS() mul_size(max_locks_per_xact, add_size(MaxBackends, max_prepared_xacts))

   
                                                                        
   
                                                                       
   
static const LOCKMASK LockConflicts[] = {0,

                         
    LOCKBIT_ON(AccessExclusiveLock),

                      
    LOCKBIT_ON(ExclusiveLock) | LOCKBIT_ON(AccessExclusiveLock),

                          
    LOCKBIT_ON(ShareLock) | LOCKBIT_ON(ShareRowExclusiveLock) | LOCKBIT_ON(ExclusiveLock) | LOCKBIT_ON(AccessExclusiveLock),

                                  
    LOCKBIT_ON(ShareUpdateExclusiveLock) | LOCKBIT_ON(ShareLock) | LOCKBIT_ON(ShareRowExclusiveLock) | LOCKBIT_ON(ExclusiveLock) | LOCKBIT_ON(AccessExclusiveLock),

                   
    LOCKBIT_ON(RowExclusiveLock) | LOCKBIT_ON(ShareUpdateExclusiveLock) | LOCKBIT_ON(ShareRowExclusiveLock) | LOCKBIT_ON(ExclusiveLock) | LOCKBIT_ON(AccessExclusiveLock),

                               
    LOCKBIT_ON(RowExclusiveLock) | LOCKBIT_ON(ShareUpdateExclusiveLock) | LOCKBIT_ON(ShareLock) | LOCKBIT_ON(ShareRowExclusiveLock) | LOCKBIT_ON(ExclusiveLock) | LOCKBIT_ON(AccessExclusiveLock),

                       
    LOCKBIT_ON(RowShareLock) | LOCKBIT_ON(RowExclusiveLock) | LOCKBIT_ON(ShareUpdateExclusiveLock) | LOCKBIT_ON(ShareLock) | LOCKBIT_ON(ShareRowExclusiveLock) | LOCKBIT_ON(ExclusiveLock) | LOCKBIT_ON(AccessExclusiveLock),

                             
    LOCKBIT_ON(AccessShareLock) | LOCKBIT_ON(RowShareLock) | LOCKBIT_ON(RowExclusiveLock) | LOCKBIT_ON(ShareUpdateExclusiveLock) | LOCKBIT_ON(ShareLock) | LOCKBIT_ON(ShareRowExclusiveLock) | LOCKBIT_ON(ExclusiveLock) | LOCKBIT_ON(AccessExclusiveLock)

};

                                              
static const char *const lock_mode_names[] = {"INVALID", "AccessShareLock", "RowShareLock", "RowExclusiveLock", "ShareUpdateExclusiveLock", "ShareLock", "ShareRowExclusiveLock", "ExclusiveLock", "AccessExclusiveLock"};

#ifndef LOCK_DEBUG
static bool Dummy_trace = false;
#endif

static const LockMethodData default_lockmethod = {AccessExclusiveLock,                                     
    LockConflicts, lock_mode_names,
#ifdef LOCK_DEBUG
    &Trace_locks
#else
    &Dummy_trace
#endif
};

static const LockMethodData user_lockmethod = {AccessExclusiveLock,                                     
    LockConflicts, lock_mode_names,
#ifdef LOCK_DEBUG
    &Trace_userlocks
#else
    &Dummy_trace
#endif
};

   
                                                             
   
static const LockMethod LockMethods[] = {NULL, &default_lockmethod, &user_lockmethod};

                                                                      
typedef struct TwoPhaseLockRecord
{
  LOCKTAG locktag;
  LOCKMODE lockmode;
} TwoPhaseLockRecord;

   
                                                                            
                                                                           
                                                                           
                                                                  
   
static int FastPathLocalUseCount = 0;

                                              
#define FAST_PATH_BITS_PER_SLOT 3
#define FAST_PATH_LOCKNUMBER_OFFSET 1
#define FAST_PATH_MASK ((1 << FAST_PATH_BITS_PER_SLOT) - 1)
#define FAST_PATH_GET_BITS(proc, n) (((proc)->fpLockBits >> (FAST_PATH_BITS_PER_SLOT * n)) & FAST_PATH_MASK)
#define FAST_PATH_BIT_POSITION(n, l) (AssertMacro((l) >= FAST_PATH_LOCKNUMBER_OFFSET), AssertMacro((l) < FAST_PATH_BITS_PER_SLOT + FAST_PATH_LOCKNUMBER_OFFSET), AssertMacro((n) < FP_LOCK_SLOTS_PER_BACKEND), ((l)-FAST_PATH_LOCKNUMBER_OFFSET + FAST_PATH_BITS_PER_SLOT * (n)))
#define FAST_PATH_SET_LOCKMODE(proc, n, l) (proc)->fpLockBits |= UINT64CONST(1) << FAST_PATH_BIT_POSITION(n, l)
#define FAST_PATH_CLEAR_LOCKMODE(proc, n, l) (proc)->fpLockBits &= ~(UINT64CONST(1) << FAST_PATH_BIT_POSITION(n, l))
#define FAST_PATH_CHECK_LOCKMODE(proc, n, l) ((proc)->fpLockBits & (UINT64CONST(1) << FAST_PATH_BIT_POSITION(n, l)))

   
                                                                         
                                                                      
                                                                          
                                                              
                                                                            
                                                                               
   
#define EligibleForRelationFastPath(locktag, mode) ((locktag)->locktag_lockmethodid == DEFAULT_LOCKMETHOD && (locktag)->locktag_type == LOCKTAG_RELATION && (locktag)->locktag_field1 == MyDatabaseId && MyDatabaseId != InvalidOid && (mode) < ShareUpdateExclusiveLock)
#define ConflictsWithRelationFastPath(locktag, mode) ((locktag)->locktag_lockmethodid == DEFAULT_LOCKMETHOD && (locktag)->locktag_type == LOCKTAG_RELATION && (locktag)->locktag_field1 != InvalidOid && (mode) > ShareUpdateExclusiveLock)

static bool
FastPathGrantRelationLock(Oid relid, LOCKMODE lockmode);
static bool
FastPathUnGrantRelationLock(Oid relid, LOCKMODE lockmode);
static bool
FastPathTransferRelationLocks(LockMethod lockMethodTable, const LOCKTAG *locktag, uint32 hashcode);
static PROCLOCK *
FastPathGetRelationLockEntry(LOCALLOCK *locallock);

   
                                                                       
                                                                        
                                                                         
                                                                               
                                                                       
                                                                            
                                                                          
                              
   
                                                                               
                                                                              
                                                
   

#define FAST_PATH_STRONG_LOCK_HASH_BITS 10
#define FAST_PATH_STRONG_LOCK_HASH_PARTITIONS (1 << FAST_PATH_STRONG_LOCK_HASH_BITS)
#define FastPathStrongLockHashPartition(hashcode) ((hashcode) % FAST_PATH_STRONG_LOCK_HASH_PARTITIONS)

typedef struct
{
  slock_t mutex;
  uint32 count[FAST_PATH_STRONG_LOCK_HASH_PARTITIONS];
} FastPathStrongRelationLockData;

static volatile FastPathStrongRelationLockData *FastPathStrongRelationLocks;

   
                                                 
   
                                                                        
                                                                
   
static HTAB *LockMethodLockHash;
static HTAB *LockMethodProcLockHash;
static HTAB *LockMethodLocalHash;

                                     
static LOCALLOCK *StrongLockInProgress;
static LOCALLOCK *awaitedLock;
static ResourceOwner awaitedOwner;

#ifdef LOCK_DEBUG

         
                                                                         
   
                                                                          
                                                 
                                                                       
                                                  
                                                                          
                                                                        
   
                                              
                                                                
   
                                                               
            
   

int Trace_lock_oidmin = FirstNormalObjectId;
bool Trace_locks = false;
bool Trace_userlocks = false;
int Trace_lock_table = 0;
bool Debug_deadlocks = false;

inline static bool
LOCK_DEBUG_ENABLED(const LOCKTAG *tag)
{
  return (*(LockMethods[tag->locktag_lockmethodid]->trace_flag) && ((Oid)tag->locktag_field2 >= (Oid)Trace_lock_oidmin)) || (Trace_lock_table && (tag->locktag_field2 == Trace_lock_table));
}

inline static void
LOCK_PRINT(const char *where, const LOCK *lock, LOCKMODE type)
{
  if (LOCK_DEBUG_ENABLED(&lock->tag))
  {
    elog(LOG,
        "%s: lock(%p) id(%u,%u,%u,%u,%u,%u) grantMask(%x) "
        "req(%d,%d,%d,%d,%d,%d,%d)=%d "
        "grant(%d,%d,%d,%d,%d,%d,%d)=%d wait(%d) type(%s)",
        where, lock, lock->tag.locktag_field1, lock->tag.locktag_field2, lock->tag.locktag_field3, lock->tag.locktag_field4, lock->tag.locktag_type, lock->tag.locktag_lockmethodid, lock->grantMask, lock->requested[1], lock->requested[2], lock->requested[3], lock->requested[4], lock->requested[5], lock->requested[6], lock->requested[7], lock->nRequested, lock->granted[1], lock->granted[2], lock->granted[3], lock->granted[4], lock->granted[5], lock->granted[6], lock->granted[7], lock->nGranted, lock->waitProcs.size, LockMethods[LOCK_LOCKMETHOD(*lock)]->lockModeNames[type]);
  }
}

inline static void
PROCLOCK_PRINT(const char *where, const PROCLOCK *proclockP)
{
  if (LOCK_DEBUG_ENABLED(&proclockP->tag.myLock->tag))
  {
    elog(LOG, "%s: proclock(%p) lock(%p) method(%u) proc(%p) hold(%x)", where, proclockP, proclockP->tag.myLock, PROCLOCK_LOCKMETHOD(*(proclockP)), proclockP->tag.myProc, (int)proclockP->holdMask);
  }
}
#else                     

#define LOCK_PRINT(where, lock, type) ((void)0)
#define PROCLOCK_PRINT(where, proclockP) ((void)0)
#endif                     

static uint32
proclock_hash(const void *key, Size keysize);
static void
RemoveLocalLock(LOCALLOCK *locallock);
static PROCLOCK *
SetupLockInTable(LockMethod lockMethodTable, PGPROC *proc, const LOCKTAG *locktag, uint32 hashcode, LOCKMODE lockmode);
static void
GrantLockLocal(LOCALLOCK *locallock, ResourceOwner owner);
static void
BeginStrongLockAcquire(LOCALLOCK *locallock, uint32 fasthashcode);
static void
FinishStrongLockAcquire(void);
static void
WaitOnLock(LOCALLOCK *locallock, ResourceOwner owner);
static void
ReleaseLockIfHeld(LOCALLOCK *locallock, bool sessionLock);
static void
LockReassignOwner(LOCALLOCK *locallock, ResourceOwner parent);
static bool
UnGrantLock(LOCK *lock, LOCKMODE lockmode, PROCLOCK *proclock, LockMethod lockMethodTable);
static void
CleanUpLock(LOCK *lock, PROCLOCK *proclock, LockMethod lockMethodTable, uint32 hashcode, bool wakeupNeeded);
static void
LockRefindAndRelease(LockMethod lockMethodTable, PGPROC *proc, LOCKTAG *locktag, LOCKMODE lockmode, bool decrement_strong_lock_count);
static void
GetSingleProcBlockerStatusData(PGPROC *blocked_proc, BlockedProcsData *data);

   
                                                               
   
                                                                        
                                                                         
                                                                        
                                                                             
                                                                               
                                                                          
                                                                            
                                                              
   
void
InitLocks(void)
{
  HASHCTL info;
  long init_table_size, max_table_size;
  bool found;

     
                                                                       
                                                 
     
  max_table_size = NLOCKENTS();
  init_table_size = max_table_size / 2;

     
                                                                          
                  
     
  MemSet(&info, 0, sizeof(info));
  info.keysize = sizeof(LOCKTAG);
  info.entrysize = sizeof(LOCK);
  info.num_partitions = NUM_LOCK_PARTITIONS;

  LockMethodLockHash = ShmemInitHash("LOCK hash", init_table_size, max_table_size, &info, HASH_ELEM | HASH_BLOBS | HASH_PARTITION);

                                               
  max_table_size *= 2;
  init_table_size *= 2;

     
                                                            
                                      
     
  info.keysize = sizeof(PROCLOCKTAG);
  info.entrysize = sizeof(PROCLOCK);
  info.hash = proclock_hash;
  info.num_partitions = NUM_LOCK_PARTITIONS;

  LockMethodProcLockHash = ShmemInitHash("PROCLOCK hash", init_table_size, max_table_size, &info, HASH_ELEM | HASH_FUNCTION | HASH_PARTITION);

     
                                    
     
  FastPathStrongRelationLocks = ShmemInitStruct("Fast Path Strong Relation Lock Data", sizeof(FastPathStrongRelationLockData), &found);
  if (!found)
  {
    SpinLockInit(&FastPathStrongRelationLocks->mutex);
  }

     
                                                                             
                                            
     
                                                                           
                                                                             
                                                                         
                                                                        
     
  if (LockMethodLocalHash)
  {
    hash_destroy(LockMethodLocalHash);
  }

  info.keysize = sizeof(LOCALLOCKTAG);
  info.entrysize = sizeof(LOCALLOCK);

  LockMethodLocalHash = hash_create("LOCALLOCK hash", 16, &info, HASH_ELEM | HASH_BLOBS);
}

   
                                                            
   
LockMethod
GetLocksMethodTable(const LOCK *lock)
{
  LOCKMETHODID lockmethodid = LOCK_LOCKMETHOD(*lock);

  Assert(0 < lockmethodid && lockmethodid < lengthof(LockMethods));
  return LockMethods[lockmethodid];
}

   
                                                               
   
LockMethod
GetLockTagsMethodTable(const LOCKTAG *locktag)
{
  LOCKMETHODID lockmethodid = (LOCKMETHODID)locktag->locktag_lockmethodid;

  Assert(0 < lockmethodid && lockmethodid < lengthof(LockMethods));
  return LockMethods[lockmethodid];
}

   
                                                    
   
                                                                           
                                                                          
                                                                         
                                                
   
uint32
LockTagHashCode(const LOCKTAG *locktag)
{
  return get_hash_value(LockMethodLockHash, (const void *)locktag);
}

   
                                                        
   
                                                                       
                                                                      
                                                                  
                                                                       
                                                                        
                                                                          
                                             
   
static uint32
proclock_hash(const void *key, Size keysize)
{
  const PROCLOCKTAG *proclocktag = (const PROCLOCKTAG *)key;
  uint32 lockhash;
  Datum procptr;

  Assert(keysize == sizeof(PROCLOCKTAG));

                                                                       
  lockhash = LockTagHashCode(&proclocktag->myLock->tag);

     
                                                                      
                                                                   
                                                                        
                                                                  
                                                                     
     
  procptr = PointerGetDatum(proclocktag->myProc);
  lockhash ^= ((uint32)procptr) << LOG2_NUM_LOCK_PARTITIONS;

  return lockhash;
}

   
                                                                           
                            
   
                                                                   
   
static inline uint32
ProcLockHashCode(const PROCLOCKTAG *proclocktag, uint32 hashcode)
{
  uint32 lockhash = hashcode;
  Datum procptr;

     
                                      
     
  procptr = PointerGetDatum(proclocktag->myProc);
  lockhash ^= ((uint32)procptr) << LOG2_NUM_LOCK_PARTITIONS;

  return lockhash;
}

   
                                                             
   
bool
DoLockModesConflict(LOCKMODE mode1, LOCKMODE mode2)
{
  LockMethod lockMethodTable = LockMethods[DEFAULT_LOCKMETHOD];

  if (lockMethodTable->conflictTab[mode1] & LOCKBIT_ON(mode2))
  {
    return true;
  }

  return false;
}

   
                                                                            
                               
   
bool
LockHeldByMe(const LOCKTAG *locktag, LOCKMODE lockmode)
{
  LOCALLOCKTAG localtag;
  LOCALLOCK *locallock;

     
                                                                  
     
  MemSet(&localtag, 0, sizeof(localtag));                         
  localtag.lock = *locktag;
  localtag.mode = lockmode;

  locallock = (LOCALLOCK *)hash_search(LockMethodLocalHash, (void *)&localtag, HASH_FIND, NULL);

  return (locallock && locallock->nLocks > 0);
}

   
                                                                   
                                                       
   
bool
LockHasWaiters(const LOCKTAG *locktag, LOCKMODE lockmode, bool sessionLock)
{
  LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
  LockMethod lockMethodTable;
  LOCALLOCKTAG localtag;
  LOCALLOCK *locallock;
  LOCK *lock;
  PROCLOCK *proclock;
  LWLock *partitionLock;
  bool hasWaiters = false;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }
  lockMethodTable = LockMethods[lockmethodid];
  if (lockmode <= 0 || lockmode > lockMethodTable->numLockModes)
  {
    elog(ERROR, "unrecognized lock mode: %d", lockmode);
  }

#ifdef LOCK_DEBUG
  if (LOCK_DEBUG_ENABLED(locktag))
  {
    elog(LOG, "LockHasWaiters: lock [%u,%u] %s", locktag->locktag_field1, locktag->locktag_field2, lockMethodTable->lockModeNames[lockmode]);
  }
#endif

     
                                                         
     
  MemSet(&localtag, 0, sizeof(localtag));                         
  localtag.lock = *locktag;
  localtag.mode = lockmode;

  locallock = (LOCALLOCK *)hash_search(LockMethodLocalHash, (void *)&localtag, HASH_FIND, NULL);

     
                                                                             
     
  if (!locallock || locallock->nLocks <= 0)
  {
    elog(WARNING, "you don't own a lock of type %s", lockMethodTable->lockModeNames[lockmode]);
    return false;
  }

     
                                  
     
  partitionLock = LockHashPartitionLock(locallock->hashcode);

  LWLockAcquire(partitionLock, LW_SHARED);

     
                                                                        
                                                                           
                                           
     
  lock = locallock->lock;
  LOCK_PRINT("LockHasWaiters: found", lock, lockmode);
  proclock = locallock->proclock;
  PROCLOCK_PRINT("LockHasWaiters: found", proclock);

     
                                                                             
              
     
  if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
  {
    PROCLOCK_PRINT("LockHasWaiters: WRONGTYPE", proclock);
    LWLockRelease(partitionLock);
    elog(WARNING, "you don't own a lock of type %s", lockMethodTable->lockModeNames[lockmode]);
    RemoveLocalLock(locallock);
    return false;
  }

     
                      
     
  if ((lockMethodTable->conflictTab[lockmode] & lock->waitMask) != 0)
  {
    hasWaiters = true;
  }

  LWLockRelease(partitionLock);

  return hasWaiters;
}

   
                                                                     
                                   
   
           
                                                      
                                  
                                                                          
                                                 
   
                   
                                                                 
                                                 
                                                                     
                                                                       
   
                                                                          
                                                                         
                                                                        
   
                                                                   
   
                                                                    
                                      
   
LockAcquireResult
LockAcquire(const LOCKTAG *locktag, LOCKMODE lockmode, bool sessionLock, bool dontWait)
{
  return LockAcquireExtended(locktag, lockmode, sessionLock, dontWait, true, NULL);
}

   
                                                                 
   
                                                                          
                                                                             
                                                                              
                                                                             
                                                                             
                                                                            
                  
   
                                                                             
                                                                   
   
LockAcquireResult
LockAcquireExtended(const LOCKTAG *locktag, LOCKMODE lockmode, bool sessionLock, bool dontWait, bool reportMemoryError, LOCALLOCK **locallockp)
{
  LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
  LockMethod lockMethodTable;
  LOCALLOCKTAG localtag;
  LOCALLOCK *locallock;
  LOCK *lock;
  PROCLOCK *proclock;
  bool found;
  ResourceOwner owner;
  uint32 hashcode;
  LWLock *partitionLock;
  int status;
  bool log_lock = false;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }
  lockMethodTable = LockMethods[lockmethodid];
  if (lockmode <= 0 || lockmode > lockMethodTable->numLockModes)
  {
    elog(ERROR, "unrecognized lock mode: %d", lockmode);
  }

  if (RecoveryInProgress() && !InRecovery && (locktag->locktag_type == LOCKTAG_OBJECT || locktag->locktag_type == LOCKTAG_RELATION) && lockmode > RowExclusiveLock)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot acquire lock mode %s on database objects while recovery is in progress", lockMethodTable->lockModeNames[lockmode]), errhint("Only RowExclusiveLock or less can be acquired on database objects during recovery.")));
  }

#ifdef LOCK_DEBUG
  if (LOCK_DEBUG_ENABLED(locktag))
  {
    elog(LOG, "LockAcquire: lock [%u,%u] %s", locktag->locktag_field1, locktag->locktag_field2, lockMethodTable->lockModeNames[lockmode]);
  }
#endif

                               
  if (sessionLock)
  {
    owner = NULL;
  }
  else
  {
    owner = CurrentResourceOwner;
  }

     
                                                                 
     
  MemSet(&localtag, 0, sizeof(localtag));                         
  localtag.lock = *locktag;
  localtag.mode = lockmode;

  locallock = (LOCALLOCK *)hash_search(LockMethodLocalHash, (void *)&localtag, HASH_ENTER, &found);

     
                                                   
     
  if (!found)
  {
    locallock->lock = NULL;
    locallock->proclock = NULL;
    locallock->hashcode = LockTagHashCode(&(localtag.lock));
    locallock->nLocks = 0;
    locallock->holdsStrongLockCount = false;
    locallock->lockCleared = false;
    locallock->numLockOwners = 0;
    locallock->maxLockOwners = 8;
    locallock->lockOwners = NULL;                              
    locallock->lockOwners = (LOCALLOCKOWNER *)MemoryContextAlloc(TopMemoryContext, locallock->maxLockOwners * sizeof(LOCALLOCKOWNER));
  }
  else
  {
                                                           
    if (locallock->numLockOwners >= locallock->maxLockOwners)
    {
      int newsize = locallock->maxLockOwners * 2;

      locallock->lockOwners = (LOCALLOCKOWNER *)repalloc(locallock->lockOwners, newsize * sizeof(LOCALLOCKOWNER));
      locallock->maxLockOwners = newsize;
    }
  }
  hashcode = locallock->hashcode;

  if (locallockp)
  {
    *locallockp = locallock;
  }

     
                                                                          
     
                                                                          
                                                   
     
  if (locallock->nLocks > 0)
  {
    GrantLockLocal(locallock, owner);
    if (locallock->lockCleared)
    {
      return LOCKACQUIRE_ALREADY_CLEAR;
    }
    else
    {
      return LOCKACQUIRE_ALREADY_HELD;
    }
  }

     
                                                                          
                                   
     
                                                                            
                                                                             
            
     
                                                                           
                                                                             
                                                      
     
  if (lockmode >= AccessExclusiveLock && locktag->locktag_type == LOCKTAG_RELATION && !RecoveryInProgress() && XLogStandbyInfoActive())
  {
    LogAccessExclusiveLockPrepare();
    log_lock = true;
  }

     
                                                                          
                                                                        
                                                                             
                                                                          
                                                                             
                                                                          
                                                                             
                                                    
     
  if (EligibleForRelationFastPath(locktag, lockmode) && FastPathLocalUseCount < FP_LOCK_SLOTS_PER_BACKEND)
  {
    uint32 fasthashcode = FastPathStrongLockHashPartition(hashcode);
    bool acquired;

       
                                                                        
                                                        
                                                                         
                                                        
       
    LWLockAcquire(&MyProc->backendLock, LW_EXCLUSIVE);
    if (FastPathStrongRelationLocks->count[fasthashcode] != 0)
    {
      acquired = false;
    }
    else
    {
      acquired = FastPathGrantRelationLock(locktag->locktag_field2, lockmode);
    }
    LWLockRelease(&MyProc->backendLock);
    if (acquired)
    {
         
                                                                       
                                                                     
                                            
         
      locallock->lock = NULL;
      locallock->proclock = NULL;
      GrantLockLocal(locallock, owner);
      return LOCKACQUIRE_OK;
    }
  }

     
                                                                         
                                                                          
                                                                          
                                         
     
  if (ConflictsWithRelationFastPath(locktag, lockmode))
  {
    uint32 fasthashcode = FastPathStrongLockHashPartition(hashcode);

    BeginStrongLockAcquire(locallock, fasthashcode);
    if (!FastPathTransferRelationLocks(lockMethodTable, locktag, hashcode))
    {
      AbortStrongLockAcquire();
      if (locallock->nLocks == 0)
      {
        RemoveLocalLock(locallock);
      }
      if (locallockp)
      {
        *locallockp = NULL;
      }
      if (reportMemoryError)
      {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_locks_per_transaction.")));
      }
      else
      {
        return LOCKACQUIRE_NOT_AVAIL;
      }
    }
  }

     
                                                                             
                                                                             
                 
     
  partitionLock = LockHashPartitionLock(hashcode);

  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

     
                                                            
     
                                                                            
                                                                           
                                                                         
                                                                          
                                                              
     
  proclock = SetupLockInTable(lockMethodTable, MyProc, locktag, hashcode, lockmode);
  if (!proclock)
  {
    AbortStrongLockAcquire();
    LWLockRelease(partitionLock);
    if (locallock->nLocks == 0)
    {
      RemoveLocalLock(locallock);
    }
    if (locallockp)
    {
      *locallockp = NULL;
    }
    if (reportMemoryError)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_locks_per_transaction.")));
    }
    else
    {
      return LOCKACQUIRE_NOT_AVAIL;
    }
  }
  locallock->proclock = proclock;
  lock = proclock->tag.myLock;
  locallock->lock = lock;

     
                                                                            
                                                                         
                                               
     
  if (lockMethodTable->conflictTab[lockmode] & lock->waitMask)
  {
    status = STATUS_FOUND;
  }
  else
  {
    status = LockCheckConflicts(lockMethodTable, lockmode, lock, proclock);
  }

  if (status == STATUS_OK)
  {
                                                             
    GrantLock(lock, proclock, lockmode);
    GrantLockLocal(locallock, owner);
  }
  else
  {
    Assert(status == STATUS_FOUND);

       
                                                                      
                                                                           
                
       
    if (dontWait)
    {
      AbortStrongLockAcquire();
      if (proclock->holdMask == 0)
      {
        uint32 proclock_hashcode;

        proclock_hashcode = ProcLockHashCode(&proclock->tag, hashcode);
        SHMQueueDelete(&proclock->lockLink);
        SHMQueueDelete(&proclock->procLink);
        if (!hash_search_with_hash_value(LockMethodProcLockHash, (void *)&(proclock->tag), proclock_hashcode, HASH_REMOVE, NULL))
        {
          elog(PANIC, "proclock table corrupted");
        }
      }
      else
      {
        PROCLOCK_PRINT("LockAcquire: NOWAIT", proclock);
      }
      lock->nRequested--;
      lock->requested[lockmode]--;
      LOCK_PRINT("LockAcquire: conditional lock failed", lock, lockmode);
      Assert((lock->nRequested > 0) && (lock->requested[lockmode] >= 0));
      Assert(lock->nGranted <= lock->nRequested);
      LWLockRelease(partitionLock);
      if (locallock->nLocks == 0)
      {
        RemoveLocalLock(locallock);
      }
      if (locallockp)
      {
        *locallockp = NULL;
      }
      return LOCKACQUIRE_NOT_AVAIL;
    }

       
                                                                       
       
    MyProc->heldLocks = proclock->holdMask;

       
                                       
       

    TRACE_POSTGRESQL_LOCK_WAIT_START(locktag->locktag_field1, locktag->locktag_field2, locktag->locktag_field3, locktag->locktag_field4, locktag->locktag_type, lockmode);

    WaitOnLock(locallock, owner);

    TRACE_POSTGRESQL_LOCK_WAIT_DONE(locktag->locktag_field1, locktag->locktag_field2, locktag->locktag_field3, locktag->locktag_field4, locktag->locktag_type, lockmode);

       
                                                                     
                                                                       
                                                                         
       

       
                                                                     
                                             
       
    if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
    {
      AbortStrongLockAcquire();
      PROCLOCK_PRINT("LockAcquire: INCONSISTENT", proclock);
      LOCK_PRINT("LockAcquire: INCONSISTENT", lock, lockmode);
                             
      LWLockRelease(partitionLock);
      elog(ERROR, "LockAcquire failed");
    }
    PROCLOCK_PRINT("LockAcquire: granted", proclock);
    LOCK_PRINT("LockAcquire: granted", lock, lockmode);
  }

     
                                                                        
                                        
     
  FinishStrongLockAcquire();

  LWLockRelease(partitionLock);

     
                                                                             
                     
     
  if (log_lock)
  {
       
                                                                        
                                                                          
                                               
       
    LogAccessExclusiveLock(locktag->locktag_field1, locktag->locktag_field2);
  }

  return LOCKACQUIRE_OK;
}

   
                                                                     
            
   
                                                                           
                              
   
                                                                     
                 
   
static PROCLOCK *
SetupLockInTable(LockMethod lockMethodTable, PGPROC *proc, const LOCKTAG *locktag, uint32 hashcode, LOCKMODE lockmode)
{
  LOCK *lock;
  PROCLOCK *proclock;
  PROCLOCKTAG proclocktag;
  uint32 proclock_hashcode;
  bool found;

     
                                          
     
  lock = (LOCK *)hash_search_with_hash_value(LockMethodLockHash, (const void *)locktag, hashcode, HASH_ENTER_NULL, &found);
  if (!lock)
  {
    return NULL;
  }

     
                                              
     
  if (!found)
  {
    lock->grantMask = 0;
    lock->waitMask = 0;
    SHMQueueInit(&(lock->procLocks));
    ProcQueueInit(&(lock->waitProcs));
    lock->nRequested = 0;
    lock->nGranted = 0;
    MemSet(lock->requested, 0, sizeof(int) * MAX_LOCKMODES);
    MemSet(lock->granted, 0, sizeof(int) * MAX_LOCKMODES);
    LOCK_PRINT("LockAcquire: new", lock, lockmode);
  }
  else
  {
    LOCK_PRINT("LockAcquire: found", lock, lockmode);
    Assert((lock->nRequested >= 0) && (lock->requested[lockmode] >= 0));
    Assert((lock->nGranted >= 0) && (lock->granted[lockmode] >= 0));
    Assert(lock->nGranted <= lock->nRequested);
  }

     
                                                 
     
  proclocktag.myLock = lock;
  proclocktag.myProc = proc;

  proclock_hashcode = ProcLockHashCode(&proclocktag, hashcode);

     
                                                   
     
  proclock = (PROCLOCK *)hash_search_with_hash_value(LockMethodProcLockHash, (void *)&proclocktag, proclock_hashcode, HASH_ENTER_NULL, &found);
  if (!proclock)
  {
                                                 
    if (lock->nRequested == 0)
    {
         
                                                                        
                                                                       
                                                                    
                                                  
         
      Assert(SHMQueueEmpty(&(lock->procLocks)));
      if (!hash_search_with_hash_value(LockMethodLockHash, (void *)&(lock->tag), hashcode, HASH_REMOVE, NULL))
      {
        elog(PANIC, "lock table corrupted");
      }
    }
    return NULL;
  }

     
                                      
     
  if (!found)
  {
    uint32 partition = LockHashPartition(hashcode);

       
                                                                      
                                                                         
                                                                        
                                                                          
                                                                           
                                                                         
                                                                          
                                                          
       
    proclock->groupLeader = proc->lockGroupLeader != NULL ? proc->lockGroupLeader : proc;
    proclock->holdMask = 0;
    proclock->releaseMask = 0;
                                           
    SHMQueueInsertBefore(&lock->procLocks, &proclock->lockLink);
    SHMQueueInsertBefore(&(proc->myProcLocks[partition]), &proclock->procLink);
    PROCLOCK_PRINT("LockAcquire: new", proclock);
  }
  else
  {
    PROCLOCK_PRINT("LockAcquire: found", proclock);
    Assert((proclock->holdMask & ~lock->grantMask) == 0);

#ifdef CHECK_DEADLOCK_RISK

       
                                                                          
                                                                     
                                                                   
                                                                        
                             
       
                                                                           
                                                                           
                                                             
       
                                                                        
                                                            
       
    {
      int i;

      for (i = lockMethodTable->numLockModes; i > 0; i--)
      {
        if (proclock->holdMask & LOCKBIT_ON(i))
        {
          if (i >= (int)lockmode)
          {
            break;                                        
          }
          elog(LOG,
              "deadlock risk: raising lock level"
              " from %s to %s on object %u/%u/%u",
              lockMethodTable->lockModeNames[i], lockMethodTable->lockModeNames[lockmode], lock->tag.locktag_field1, lock->tag.locktag_field2, lock->tag.locktag_field3);
          break;
        }
      }
    }
#endif                          
  }

     
                                                                      
                                                                           
                                                            
     
  lock->nRequested++;
  lock->requested[lockmode]++;
  Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));

     
                                                                         
             
     
  if (proclock->holdMask & LOCKBIT_ON(lockmode))
  {
    elog(ERROR, "lock %s on object %u/%u/%u is already held", lockMethodTable->lockModeNames[lockmode], lock->tag.locktag_field1, lock->tag.locktag_field2, lock->tag.locktag_field3);
  }

  return proclock;
}

   
                                        
   
static void
RemoveLocalLock(LOCALLOCK *locallock)
{
  int i;

  for (i = locallock->numLockOwners - 1; i >= 0; i--)
  {
    if (locallock->lockOwners[i].owner != NULL)
    {
      ResourceOwnerForgetLock(locallock->lockOwners[i].owner, locallock);
    }
  }
  locallock->numLockOwners = 0;
  if (locallock->lockOwners != NULL)
  {
    pfree(locallock->lockOwners);
  }
  locallock->lockOwners = NULL;

  if (locallock->holdsStrongLockCount)
  {
    uint32 fasthashcode;

    fasthashcode = FastPathStrongLockHashPartition(locallock->hashcode);

    SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
    Assert(FastPathStrongRelationLocks->count[fasthashcode] > 0);
    FastPathStrongRelationLocks->count[fasthashcode]--;
    locallock->holdsStrongLockCount = false;
    SpinLockRelease(&FastPathStrongRelationLocks->mutex);
  }

  if (!hash_search(LockMethodLocalHash, (void *)&(locallock->tag), HASH_REMOVE, NULL))
  {
    elog(WARNING, "locallock table corrupted");
  }
}

   
                                                               
                               
   
                                                               
   
          
                                                                  
                                                                       
                                                                          
                                                                            
                                                                          
                                                                     
   
int
LockCheckConflicts(LockMethod lockMethodTable, LOCKMODE lockmode, LOCK *lock, PROCLOCK *proclock)
{
  int numLockModes = lockMethodTable->numLockModes;
  LOCKMASK myLocks;
  int conflictMask = lockMethodTable->conflictTab[lockmode];
  int conflictsRemaining[MAX_LOCKMODES];
  int totalConflictsRemaining = 0;
  int i;
  SHM_QUEUE *procLocks;
  PROCLOCK *otherproclock;

     
                                                                             
                          
     
                                                                    
                                                                           
                                                                          
                          
     
  if (!(conflictMask & lock->grantMask))
  {
    PROCLOCK_PRINT("LockCheckConflicts: no conflict", proclock);
    return STATUS_OK;
  }

     
                                                                          
                                                                             
                                                                          
     
  myLocks = proclock->holdMask;
  for (i = 1; i <= numLockModes; i++)
  {
    if ((conflictMask & LOCKBIT_ON(i)) == 0)
    {
      conflictsRemaining[i] = 0;
      continue;
    }
    conflictsRemaining[i] = lock->granted[i];
    if (myLocks & LOCKBIT_ON(i))
    {
      --conflictsRemaining[i];
    }
    totalConflictsRemaining += conflictsRemaining[i];
  }

                                                
  if (totalConflictsRemaining == 0)
  {
    PROCLOCK_PRINT("LockCheckConflicts: resolved (simple)", proclock);
    return STATUS_OK;
  }

                                                        
  if (proclock->groupLeader == MyProc && MyProc->lockGroupLeader == NULL)
  {
    Assert(proclock->tag.myProc == MyProc);
    PROCLOCK_PRINT("LockCheckConflicts: conflicting (simple)", proclock);
    return STATUS_FOUND;
  }

     
                                                                          
                                                                            
                                                                     
                                                                         
                                                                             
         
     
  procLocks = &(lock->procLocks);
  otherproclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, lockLink));
  while (otherproclock != NULL)
  {
    if (proclock != otherproclock && proclock->groupLeader == otherproclock->groupLeader && (otherproclock->holdMask & conflictMask) != 0)
    {
      int intersectMask = otherproclock->holdMask & conflictMask;

      for (i = 1; i <= numLockModes; i++)
      {
        if ((intersectMask & LOCKBIT_ON(i)) != 0)
        {
          if (conflictsRemaining[i] <= 0)
          {
            elog(PANIC, "proclocks held do not match lock");
          }
          conflictsRemaining[i]--;
          totalConflictsRemaining--;
        }
      }

      if (totalConflictsRemaining == 0)
      {
        PROCLOCK_PRINT("LockCheckConflicts: resolved (group)", proclock);
        return STATUS_OK;
      }
    }
    otherproclock = (PROCLOCK *)SHMQueueNext(procLocks, &otherproclock->lockLink, offsetof(PROCLOCK, lockLink));
  }

                                   
  PROCLOCK_PRINT("LockCheckConflicts: conflicting (group)", proclock);
  return STATUS_FOUND;
}

   
                                                                     
                                       
   
                                                                             
                                                                             
   
                                                                            
                                                                            
                                                    
   
void
GrantLock(LOCK *lock, PROCLOCK *proclock, LOCKMODE lockmode)
{
  lock->nGranted++;
  lock->granted[lockmode]++;
  lock->grantMask |= LOCKBIT_ON(lockmode);
  if (lock->granted[lockmode] == lock->requested[lockmode])
  {
    lock->waitMask &= LOCKBIT_OFF(lockmode);
  }
  proclock->holdMask |= LOCKBIT_ON(lockmode);
  LOCK_PRINT("GrantLock", lock, lockmode);
  Assert((lock->nGranted > 0) && (lock->granted[lockmode] > 0));
  Assert(lock->nGranted <= lock->nRequested);
}

   
                                         
   
                                                                       
                                                          
   
                                                                   
                                               
   
static bool
UnGrantLock(LOCK *lock, LOCKMODE lockmode, PROCLOCK *proclock, LockMethod lockMethodTable)
{
  bool wakeupNeeded = false;

  Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));
  Assert((lock->nGranted > 0) && (lock->granted[lockmode] > 0));
  Assert(lock->nGranted <= lock->nRequested);

     
                                
     
  lock->nRequested--;
  lock->requested[lockmode]--;
  lock->nGranted--;
  lock->granted[lockmode]--;

  if (lock->granted[lockmode] == 0)
  {
                                                               
    lock->grantMask &= LOCKBIT_OFF(lockmode);
  }

  LOCK_PRINT("UnGrantLock: updated", lock, lockmode);

     
                                                                            
                                                                             
                                                                            
                                                                           
                                                                           
                                                                             
                    
     
  if (lockMethodTable->conflictTab[lockmode] & lock->waitMask)
  {
    wakeupNeeded = true;
  }

     
                                     
     
  proclock->holdMask &= LOCKBIT_OFF(lockmode);
  PROCLOCK_PRINT("UnGrantLock: updated", proclock);

  return wakeupNeeded;
}

   
                                                                           
                                                                           
                                                                        
                                                                           
                 
   
                                                                     
                 
   
static void
CleanUpLock(LOCK *lock, PROCLOCK *proclock, LockMethod lockMethodTable, uint32 hashcode, bool wakeupNeeded)
{
     
                                                                            
            
     
  if (proclock->holdMask == 0)
  {
    uint32 proclock_hashcode;

    PROCLOCK_PRINT("CleanUpLock: deleting", proclock);
    SHMQueueDelete(&proclock->lockLink);
    SHMQueueDelete(&proclock->procLink);
    proclock_hashcode = ProcLockHashCode(&proclock->tag, hashcode);
    if (!hash_search_with_hash_value(LockMethodProcLockHash, (void *)&(proclock->tag), proclock_hashcode, HASH_REMOVE, NULL))
    {
      elog(PANIC, "proclock table corrupted");
    }
  }

  if (lock->nRequested == 0)
  {
       
                                                                           
               
       
    LOCK_PRINT("CleanUpLock: deleting", lock, 0);
    Assert(SHMQueueEmpty(&(lock->procLocks)));
    if (!hash_search_with_hash_value(LockMethodLockHash, (void *)&(lock->tag), hashcode, HASH_REMOVE, NULL))
    {
      elog(PANIC, "lock table corrupted");
    }
  }
  else if (wakeupNeeded)
  {
                                                          
    ProcLockWakeup(lockMethodTable, lock);
  }
}

   
                                                                  
                                       
   
                                                                   
                        
   
static void
GrantLockLocal(LOCALLOCK *locallock, ResourceOwner owner)
{
  LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
  int i;

  Assert(locallock->numLockOwners < locallock->maxLockOwners);
                       
  locallock->nLocks++;
                                
  for (i = 0; i < locallock->numLockOwners; i++)
  {
    if (lockOwners[i].owner == owner)
    {
      lockOwners[i].nLocks++;
      return;
    }
  }
  lockOwners[i].owner = owner;
  lockOwners[i].nLocks = 1;
  locallock->numLockOwners++;
  if (owner != NULL)
  {
    ResourceOwnerRememberLock(owner, locallock);
  }
}

   
                                                                           
                                             
   
static void
BeginStrongLockAcquire(LOCALLOCK *locallock, uint32 fasthashcode)
{
  Assert(StrongLockInProgress == NULL);
  Assert(locallock->holdsStrongLockCount == false);

     
                                                                         
                                                                           
                    
     
                                                                      
                                                                 
     

  SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
  FastPathStrongRelationLocks->count[fasthashcode]++;
  locallock->holdsStrongLockCount = true;
  StrongLockInProgress = locallock;
  SpinLockRelease(&FastPathStrongRelationLocks->mutex);
}

   
                                                                      
                                          
   
static void
FinishStrongLockAcquire(void)
{
  StrongLockInProgress = NULL;
}

   
                                                                        
                           
   
void
AbortStrongLockAcquire(void)
{
  uint32 fasthashcode;
  LOCALLOCK *locallock = StrongLockInProgress;

  if (locallock == NULL)
  {
    return;
  }

  fasthashcode = FastPathStrongLockHashPartition(locallock->hashcode);
  Assert(locallock->holdsStrongLockCount == true);
  SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
  Assert(FastPathStrongRelationLocks->count[fasthashcode] > 0);
  FastPathStrongRelationLocks->count[fasthashcode]--;
  locallock->holdsStrongLockCount = false;
  StrongLockInProgress = NULL;
  SpinLockRelease(&FastPathStrongRelationLocks->mutex);
}

   
                                                                     
                   
   
                                                                      
                                                                  
   
                                                                         
                                                    
   
void
GrantAwaitedLock(void)
{
  GrantLockLocal(awaitedLock, awaitedOwner);
}

   
                                                     
   
                                                                           
                                                                              
                                                                    
   
void
MarkLockClear(LOCALLOCK *locallock)
{
  Assert(locallock->nLocks > 0);
  locallock->lockCleared = true;
}

   
                                        
   
                                                                        
                                           
   
                                                         
   
static void
WaitOnLock(LOCALLOCK *locallock, ResourceOwner owner)
{
  LOCKMETHODID lockmethodid = LOCALLOCK_LOCKMETHOD(*locallock);
  LockMethod lockMethodTable = LockMethods[lockmethodid];
  char *volatile new_status = NULL;

  LOCK_PRINT("WaitOnLock: sleeping on lock", locallock->lock, locallock->tag.mode);

                                       
  if (update_process_title)
  {
    const char *old_status;
    int len;

    old_status = get_ps_display(&len);
    new_status = (char *)palloc(len + 8 + 1);
    memcpy(new_status, old_status, len);
    strcpy(new_status + len, " waiting");
    set_ps_display(new_status, false);
    new_status[len] = '\0';                              
  }

  awaitedLock = locallock;
  awaitedOwner = owner;

     
                                                                       
                                                                           
                                                                         
                                                                         
                                                                             
                                                                             
                                                                           
                                            
     
                                                                            
                                                                            
                                                                            
                                                                             
                                                                             
                                          
     
  PG_TRY();
  {
    if (ProcSleep(locallock, lockMethodTable) != STATUS_OK)
    {
         
                                                                        
              
         
      awaitedLock = NULL;
      LOCK_PRINT("WaitOnLock: aborting on lock", locallock->lock, locallock->tag.mode);
      LWLockRelease(LockHashPartitionLock(locallock->hashcode));

         
                                                                       
                                                                     
         
      DeadLockReport();
                       
    }
  }
  PG_CATCH();
  {
                                                                      

                                             
    if (update_process_title)
    {
      set_ps_display(new_status, false);
      pfree(new_status);
    }

                                 
    PG_RE_THROW();
  }
  PG_END_TRY();

  awaitedLock = NULL;

                                           
  if (update_process_title)
  {
    set_ps_display(new_status, false);
    pfree(new_status);
  }

  LOCK_PRINT("WaitOnLock: wakeup on lock", locallock->lock, locallock->tag.mode);
}

   
                                                                               
                                                                             
                               
   
                                                                       
                                                 
   
                                                                                
   
void
RemoveFromWaitQueue(PGPROC *proc, uint32 hashcode)
{
  LOCK *waitLock = proc->waitLock;
  PROCLOCK *proclock = proc->waitProcLock;
  LOCKMODE lockmode = proc->waitLockMode;
  LOCKMETHODID lockmethodid = LOCK_LOCKMETHOD(*waitLock);

                                 
  Assert(proc->waitStatus == STATUS_WAITING);
  Assert(proc->links.next != NULL);
  Assert(waitLock);
  Assert(waitLock->waitProcs.size > 0);
  Assert(0 < lockmethodid && lockmethodid < lengthof(LockMethods));

                                          
  SHMQueueDelete(&(proc->links));
  waitLock->waitProcs.size--;

                                                            
  Assert(waitLock->nRequested > 0);
  Assert(waitLock->nRequested > proc->waitLock->nGranted);
  waitLock->nRequested--;
  Assert(waitLock->requested[lockmode] > 0);
  waitLock->requested[lockmode]--;
                                                         
  if (waitLock->granted[lockmode] == waitLock->requested[lockmode])
  {
    waitLock->waitMask &= LOCKBIT_OFF(lockmode);
  }

                                                                     
  proc->waitLock = NULL;
  proc->waitProcLock = NULL;
  proc->waitStatus = STATUS_ERROR;

     
                                                                             
                                                                       
                                                                   
                                                                          
                                                         
     
  CleanUpLock(waitLock, proclock, LockMethods[lockmethodid], hashcode, true);
}

   
                                                                           
                                                                    
                              
   
                                                                  
                                                      
                                                            
                                              
                                      
   
bool
LockRelease(const LOCKTAG *locktag, LOCKMODE lockmode, bool sessionLock)
{
  LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
  LockMethod lockMethodTable;
  LOCALLOCKTAG localtag;
  LOCALLOCK *locallock;
  LOCK *lock;
  PROCLOCK *proclock;
  LWLock *partitionLock;
  bool wakeupNeeded;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }
  lockMethodTable = LockMethods[lockmethodid];
  if (lockmode <= 0 || lockmode > lockMethodTable->numLockModes)
  {
    elog(ERROR, "unrecognized lock mode: %d", lockmode);
  }

#ifdef LOCK_DEBUG
  if (LOCK_DEBUG_ENABLED(locktag))
  {
    elog(LOG, "LockRelease: lock [%u,%u] %s", locktag->locktag_field1, locktag->locktag_field2, lockMethodTable->lockModeNames[lockmode]);
  }
#endif

     
                                                         
     
  MemSet(&localtag, 0, sizeof(localtag));                         
  localtag.lock = *locktag;
  localtag.mode = lockmode;

  locallock = (LOCALLOCK *)hash_search(LockMethodLocalHash, (void *)&localtag, HASH_FIND, NULL);

     
                                                                             
     
  if (!locallock || locallock->nLocks <= 0)
  {
    elog(WARNING, "you don't own a lock of type %s", lockMethodTable->lockModeNames[lockmode]);
    return false;
  }

     
                                                
     
  {
    LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
    ResourceOwner owner;
    int i;

                                 
    if (sessionLock)
    {
      owner = NULL;
    }
    else
    {
      owner = CurrentResourceOwner;
    }

    for (i = locallock->numLockOwners - 1; i >= 0; i--)
    {
      if (lockOwners[i].owner == owner)
      {
        Assert(lockOwners[i].nLocks > 0);
        if (--lockOwners[i].nLocks == 0)
        {
          if (owner != NULL)
          {
            ResourceOwnerForgetLock(owner, locallock);
          }
                                       
          locallock->numLockOwners--;
          if (i < locallock->numLockOwners)
          {
            lockOwners[i] = lockOwners[locallock->numLockOwners];
          }
        }
        break;
      }
    }
    if (i < 0)
    {
                                                           
      elog(WARNING, "you don't own a lock of type %s", lockMethodTable->lockModeNames[lockmode]);
      return false;
    }
  }

     
                                                                             
           
     
  locallock->nLocks--;

  if (locallock->nLocks > 0)
  {
    return true;
  }

     
                                                                         
                                                                         
                                                                            
                                                                         
                                                         
     
  locallock->lockCleared = false;

                                                                    
  if (EligibleForRelationFastPath(locktag, lockmode) && FastPathLocalUseCount > 0)
  {
    bool released;

       
                                                                         
                                                                   
       
    LWLockAcquire(&MyProc->backendLock, LW_EXCLUSIVE);
    released = FastPathUnGrantRelationLock(locktag->locktag_field2, lockmode);
    LWLockRelease(&MyProc->backendLock);
    if (released)
    {
      RemoveLocalLock(locallock);
      return true;
    }
  }

     
                                                             
     
  partitionLock = LockHashPartitionLock(locallock->hashcode);

  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

     
                                                                            
                                                                         
                                                                           
                                                                            
                                                                         
                                                            
     
  lock = locallock->lock;
  if (!lock)
  {
    PROCLOCKTAG proclocktag;

    Assert(EligibleForRelationFastPath(locktag, lockmode));
    lock = (LOCK *)hash_search_with_hash_value(LockMethodLockHash, (const void *)locktag, locallock->hashcode, HASH_FIND, NULL);
    if (!lock)
    {
      elog(ERROR, "failed to re-find shared lock object");
    }
    locallock->lock = lock;

    proclocktag.myLock = lock;
    proclocktag.myProc = MyProc;
    locallock->proclock = (PROCLOCK *)hash_search(LockMethodProcLockHash, (void *)&proclocktag, HASH_FIND, NULL);
    if (!locallock->proclock)
    {
      elog(ERROR, "failed to re-find shared proclock object");
    }
  }
  LOCK_PRINT("LockRelease: found", lock, lockmode);
  proclock = locallock->proclock;
  PROCLOCK_PRINT("LockRelease: found", proclock);

     
                                                                             
              
     
  if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
  {
    PROCLOCK_PRINT("LockRelease: WRONGTYPE", proclock);
    LWLockRelease(partitionLock);
    elog(WARNING, "you don't own a lock of type %s", lockMethodTable->lockModeNames[lockmode]);
    RemoveLocalLock(locallock);
    return false;
  }

     
                                                                        
     
  wakeupNeeded = UnGrantLock(lock, lockmode, proclock, lockMethodTable);

  CleanUpLock(lock, proclock, lockMethodTable, locallock->hashcode, wakeupNeeded);

  LWLockRelease(partitionLock);

  RemoveLocalLock(locallock);
  return true;
}

   
                                                                         
                                     
   
                                                                    
                                                                 
                                                      
   
void
LockReleaseAll(LOCKMETHODID lockmethodid, bool allLocks)
{
  HASH_SEQ_STATUS status;
  LockMethod lockMethodTable;
  int i, numLockModes;
  LOCALLOCK *locallock;
  LOCK *lock;
  PROCLOCK *proclock;
  int partition;
  bool have_fast_path_lwlock = false;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }
  lockMethodTable = LockMethods[lockmethodid];

#ifdef LOCK_DEBUG
  if (*(lockMethodTable->trace_flag))
  {
    elog(LOG, "LockReleaseAll: lockmethod=%d", lockmethodid);
  }
#endif

     
                                                                            
                                                                     
                                                                          
           
     
  if (lockmethodid == DEFAULT_LOCKMETHOD)
  {
    VirtualXactLockTableCleanup();
  }

  numLockModes = lockMethodTable->numLockModes;

     
                                                                      
                                                                            
                                                                       
                                                                            
                                                                          
                   
     
  hash_seq_init(&status, LockMethodLocalHash);

  while ((locallock = (LOCALLOCK *)hash_seq_search(&status)) != NULL)
  {
       
                                                                      
                                                                       
              
       
    if (locallock->nLocks == 0)
    {
      RemoveLocalLock(locallock);
      continue;
    }

                                                                   
    if (LOCALLOCK_LOCKMETHOD(*locallock) != lockmethodid)
    {
      continue;
    }

       
                                                                        
                                                                         
                                                                
       
    if (!allLocks)
    {
      LOCALLOCKOWNER *lockOwners = locallock->lockOwners;

                                                                        
      for (i = 0; i < locallock->numLockOwners; i++)
      {
        if (lockOwners[i].owner == NULL)
        {
          lockOwners[0] = lockOwners[i];
        }
        else
        {
          ResourceOwnerForgetLock(lockOwners[i].owner, locallock);
        }
      }

      if (locallock->numLockOwners > 0 && lockOwners[0].owner == NULL && lockOwners[0].nLocks > 0)
      {
                                                              
        locallock->nLocks = lockOwners[0].nLocks;
        locallock->numLockOwners = 1;
                                                        
        continue;
      }
      else
      {
        locallock->numLockOwners = 0;
      }
    }

       
                                                                          
                                                                           
       
    if (locallock->proclock == NULL || locallock->lock == NULL)
    {
      LOCKMODE lockmode = locallock->tag.mode;
      Oid relid;

                                                           
      if (!EligibleForRelationFastPath(&locallock->tag.lock, lockmode))
      {
        elog(PANIC, "locallock table corrupted");
      }

         
                                                                 
                                                                         
                                                                     
                                                                        
                                                                   
                                                                        
         
      if (!have_fast_path_lwlock)
      {
        LWLockAcquire(&MyProc->backendLock, LW_EXCLUSIVE);
        have_fast_path_lwlock = true;
      }

                                      
      relid = locallock->tag.lock.locktag_field2;
      if (FastPathUnGrantRelationLock(relid, lockmode))
      {
        RemoveLocalLock(locallock);
        continue;
      }

         
                                                                
                                                                      
                                                                         
         
      LWLockRelease(&MyProc->backendLock);
      have_fast_path_lwlock = false;

         
                                                                     
                                                                
                                                                      
                                                                   
                                                                 
         
      LockRefindAndRelease(lockMethodTable, MyProc, &locallock->tag.lock, lockmode, false);
      RemoveLocalLock(locallock);
      continue;
    }

                                                                    
    if (locallock->nLocks > 0)
    {
      locallock->proclock->releaseMask |= LOCKBIT_ON(locallock->tag.mode);
    }

                                                  
    RemoveLocalLock(locallock);
  }

                                               
  if (have_fast_path_lwlock)
  {
    LWLockRelease(&MyProc->backendLock);
  }

     
                                               
     
  for (partition = 0; partition < NUM_LOCK_PARTITIONS; partition++)
  {
    LWLock *partitionLock;
    SHM_QUEUE *procLocks = &(MyProc->myProcLocks[partition]);
    PROCLOCK *nextplock;

    partitionLock = LockHashPartitionLockByIndex(partition);

       
                                                                     
                                                                         
                                                                       
                                                                  
                                                                    
                                                                       
                                                                      
                                                                   
                                                                       
                                                                        
                                                  
       
                                                                    
                                                                   
                                                                        
                                                                        
                          
       
    if (SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, procLink)) == NULL)
    {
      continue;                                     
    }

    LWLockAcquire(partitionLock, LW_EXCLUSIVE);

    for (proclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, procLink)); proclock; proclock = nextplock)
    {
      bool wakeupNeeded = false;

                                                                    
      nextplock = (PROCLOCK *)SHMQueueNext(procLocks, &proclock->procLink, offsetof(PROCLOCK, procLink));

      Assert(proclock->tag.myProc == MyProc);

      lock = proclock->tag.myLock;

                                                                     
      if (LOCK_LOCKMETHOD(*lock) != lockmethodid)
      {
        continue;
      }

         
                                                                        
                            
         
      if (allLocks)
      {
        proclock->releaseMask = proclock->holdMask;
      }
      else
      {
        Assert((proclock->releaseMask & ~proclock->holdMask) == 0);
      }

         
                                                                         
                                                    
         
      if (proclock->releaseMask == 0 && proclock->holdMask != 0)
      {
        continue;
      }

      PROCLOCK_PRINT("LockReleaseAll", proclock);
      LOCK_PRINT("LockReleaseAll", lock, 0);
      Assert(lock->nRequested >= 0);
      Assert(lock->nGranted >= 0);
      Assert(lock->nGranted <= lock->nRequested);
      Assert((proclock->holdMask & ~lock->grantMask) == 0);

         
                                                  
         
      for (i = 1; i <= numLockModes; i++)
      {
        if (proclock->releaseMask & LOCKBIT_ON(i))
        {
          wakeupNeeded |= UnGrantLock(lock, i, proclock, lockMethodTable);
        }
      }
      Assert((lock->nRequested >= 0) && (lock->nGranted >= 0));
      Assert(lock->nGranted <= lock->nRequested);
      LOCK_PRINT("LockReleaseAll: updated", lock, 0);

      proclock->releaseMask = 0;

                                                       
      CleanUpLock(lock, proclock, lockMethodTable, LockTagHashCode(&lock->tag), wakeupNeeded);
    }                                                

    LWLockRelease(partitionLock);
  }                           

#ifdef LOCK_DEBUG
  if (*(lockMethodTable->trace_flag))
  {
    elog(LOG, "LockReleaseAll done");
  }
#endif
}

   
                                                                                
                                          
   
void
LockReleaseSession(LOCKMETHODID lockmethodid)
{
  HASH_SEQ_STATUS status;
  LOCALLOCK *locallock;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }

  hash_seq_init(&status, LockMethodLocalHash);

  while ((locallock = (LOCALLOCK *)hash_seq_search(&status)) != NULL)
  {
                                                                
    if (LOCALLOCK_LOCKMETHOD(*locallock) != lockmethodid)
    {
      continue;
    }

    ReleaseLockIfHeld(locallock, true);
  }
}

   
                           
                                                        
   
                                                                           
                                                                        
                                                                            
                       
   
void
LockReleaseCurrentOwner(LOCALLOCK **locallocks, int nlocks)
{
  if (locallocks == NULL)
  {
    HASH_SEQ_STATUS status;
    LOCALLOCK *locallock;

    hash_seq_init(&status, LockMethodLocalHash);

    while ((locallock = (LOCALLOCK *)hash_seq_search(&status)) != NULL)
    {
      ReleaseLockIfHeld(locallock, false);
    }
  }
  else
  {
    int i;

    for (i = nlocks - 1; i >= 0; i--)
    {
      ReleaseLockIfHeld(locallocks[i], false);
    }
  }
}

   
                     
                                                                           
                                                                   
   
                                                                            
                                                                             
                                                                       
                                                                             
                                                                         
                                                                     
                
   
static void
ReleaseLockIfHeld(LOCALLOCK *locallock, bool sessionLock)
{
  ResourceOwner owner;
  LOCALLOCKOWNER *lockOwners;
  int i;

                                                         
  if (sessionLock)
  {
    owner = NULL;
  }
  else
  {
    owner = CurrentResourceOwner;
  }

                                                                        
  lockOwners = locallock->lockOwners;
  for (i = locallock->numLockOwners - 1; i >= 0; i--)
  {
    if (lockOwners[i].owner == owner)
    {
      Assert(lockOwners[i].nLocks > 0);
      if (lockOwners[i].nLocks < locallock->nLocks)
      {
           
                                                              
                          
           
        locallock->nLocks -= lockOwners[i].nLocks;
                                     
        locallock->numLockOwners--;
        if (owner != NULL)
        {
          ResourceOwnerForgetLock(owner, locallock);
        }
        if (i < locallock->numLockOwners)
        {
          lockOwners[i] = lockOwners[locallock->numLockOwners];
        }
      }
      else
      {
        Assert(lockOwners[i].nLocks == locallock->nLocks);
                                                   
        lockOwners[i].nLocks = 1;
        locallock->nLocks = 1;
        if (!LockRelease(&locallock->tag.lock, locallock->tag.mode, sessionLock))
        {
          elog(WARNING, "ReleaseLockIfHeld: failed??");
        }
      }
      break;
    }
  }
}

   
                            
                                                                   
                                  
   
                                                                           
                                                                       
                                                                            
                                                           
   
void
LockReassignCurrentOwner(LOCALLOCK **locallocks, int nlocks)
{
  ResourceOwner parent = ResourceOwnerGetParent(CurrentResourceOwner);

  Assert(parent != NULL);

  if (locallocks == NULL)
  {
    HASH_SEQ_STATUS status;
    LOCALLOCK *locallock;

    hash_seq_init(&status, LockMethodLocalHash);

    while ((locallock = (LOCALLOCK *)hash_seq_search(&status)) != NULL)
    {
      LockReassignOwner(locallock, parent);
    }
  }
  else
  {
    int i;

    for (i = nlocks - 1; i >= 0; i--)
    {
      LockReassignOwner(locallocks[i], parent);
    }
  }
}

   
                                                                               
                                       
   
static void
LockReassignOwner(LOCALLOCK *locallock, ResourceOwner parent)
{
  LOCALLOCKOWNER *lockOwners;
  int i;
  int ic = -1;
  int ip = -1;

     
                                                                          
            
     
  lockOwners = locallock->lockOwners;
  for (i = locallock->numLockOwners - 1; i >= 0; i--)
  {
    if (lockOwners[i].owner == CurrentResourceOwner)
    {
      ic = i;
    }
    else if (lockOwners[i].owner == parent)
    {
      ip = i;
    }
  }

  if (ic < 0)
  {
    return;                       
  }

  if (ip < 0)
  {
                                                              
    lockOwners[ic].owner = parent;
    ResourceOwnerRememberLock(parent, locallock);
  }
  else
  {
                                           
    lockOwners[ip].nLocks += lockOwners[ic].nLocks;
                                 
    locallock->numLockOwners--;
    if (ic < locallock->numLockOwners)
    {
      lockOwners[ic] = lockOwners[locallock->numLockOwners];
    }
  }
  ResourceOwnerForgetLock(CurrentResourceOwner, locallock);
}

   
                             
                                                                     
   
static bool
FastPathGrantRelationLock(Oid relid, LOCKMODE lockmode)
{
  uint32 f;
  uint32 unused_slot = FP_LOCK_SLOTS_PER_BACKEND;

                                                                       
  for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
  {
    if (FAST_PATH_GET_BITS(MyProc, f) == 0)
    {
      unused_slot = f;
    }
    else if (MyProc->fpRelId[f] == relid)
    {
      Assert(!FAST_PATH_CHECK_LOCKMODE(MyProc, f, lockmode));
      FAST_PATH_SET_LOCKMODE(MyProc, f, lockmode);
      return true;
    }
  }

                                                 
  if (unused_slot < FP_LOCK_SLOTS_PER_BACKEND)
  {
    MyProc->fpRelId[unused_slot] = relid;
    FAST_PATH_SET_LOCKMODE(MyProc, unused_slot, lockmode);
    ++FastPathLocalUseCount;
    return true;
  }

                                             
  return false;
}

   
                               
                                                                      
                                  
   
static bool
FastPathUnGrantRelationLock(Oid relid, LOCKMODE lockmode)
{
  uint32 f;
  bool result = false;

  FastPathLocalUseCount = 0;
  for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
  {
    if (MyProc->fpRelId[f] == relid && FAST_PATH_CHECK_LOCKMODE(MyProc, f, lockmode))
    {
      Assert(!result);
      FAST_PATH_CLEAR_LOCKMODE(MyProc, f, lockmode);
      result = true;
                                                                       
    }
    if (FAST_PATH_GET_BITS(MyProc, f) != 0)
    {
      ++FastPathLocalUseCount;
    }
  }
  return result;
}

   
                                 
                                                                          
                                     
   
                                                                  
   
static bool
FastPathTransferRelationLocks(LockMethod lockMethodTable, const LOCKTAG *locktag, uint32 hashcode)
{
  LWLock *partitionLock = LockHashPartitionLock(hashcode);
  Oid relid = locktag->locktag_field2;
  uint32 i;

     
                                                                           
                                                                   
                                                                   
                                         
     
  for (i = 0; i < ProcGlobal->allProcCount; i++)
  {
    PGPROC *proc = &ProcGlobal->allProcs[i];
    uint32 f;

    LWLockAcquire(&proc->backendLock, LW_EXCLUSIVE);

       
                                                                        
                                                                         
                                     
       
                                                                         
                                                                   
                                                                         
                                                                          
                                                                         
                                                                          
                                                                         
                                                                           
                                                                       
       
    if (proc->databaseId != locktag->locktag_field1)
    {
      LWLockRelease(&proc->backendLock);
      continue;
    }

    for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
    {
      uint32 lockmode;

                                                                
      if (relid != proc->fpRelId[f] || FAST_PATH_GET_BITS(proc, f) == 0)
      {
        continue;
      }

                                       
      LWLockAcquire(partitionLock, LW_EXCLUSIVE);
      for (lockmode = FAST_PATH_LOCKNUMBER_OFFSET; lockmode < FAST_PATH_LOCKNUMBER_OFFSET + FAST_PATH_BITS_PER_SLOT; ++lockmode)
      {
        PROCLOCK *proclock;

        if (!FAST_PATH_CHECK_LOCKMODE(proc, f, lockmode))
        {
          continue;
        }
        proclock = SetupLockInTable(lockMethodTable, proc, locktag, hashcode, lockmode);
        if (!proclock)
        {
          LWLockRelease(partitionLock);
          LWLockRelease(&proc->backendLock);
          return false;
        }
        GrantLock(proclock->tag.myLock, proclock, lockmode);
        FAST_PATH_CLEAR_LOCKMODE(proc, f, lockmode);
      }
      LWLockRelease(partitionLock);

                                               
      break;
    }
    LWLockRelease(&proc->backendLock);
  }
  return true;
}

   
                        
                                                                       
                                                            
   
                                                             
   
static PROCLOCK *
FastPathGetRelationLockEntry(LOCALLOCK *locallock)
{
  LockMethod lockMethodTable = LockMethods[DEFAULT_LOCKMETHOD];
  LOCKTAG *locktag = &locallock->tag.lock;
  PROCLOCK *proclock = NULL;
  LWLock *partitionLock = LockHashPartitionLock(locallock->hashcode);
  Oid relid = locktag->locktag_field2;
  uint32 f;

  LWLockAcquire(&MyProc->backendLock, LW_EXCLUSIVE);

  for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
  {
    uint32 lockmode;

                                                              
    if (relid != MyProc->fpRelId[f] || FAST_PATH_GET_BITS(MyProc, f) == 0)
    {
      continue;
    }

                                                               
    lockmode = locallock->tag.mode;
    if (!FAST_PATH_CHECK_LOCKMODE(MyProc, f, lockmode))
    {
      break;
    }

                                     
    LWLockAcquire(partitionLock, LW_EXCLUSIVE);

    proclock = SetupLockInTable(lockMethodTable, MyProc, locktag, locallock->hashcode, lockmode);
    if (!proclock)
    {
      LWLockRelease(partitionLock);
      LWLockRelease(&MyProc->backendLock);
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_locks_per_transaction.")));
    }
    GrantLock(proclock->tag.myLock, proclock, lockmode);
    FAST_PATH_CLEAR_LOCKMODE(MyProc, f, lockmode);

    LWLockRelease(partitionLock);

                                             
    break;
  }

  LWLockRelease(&MyProc->backendLock);

                                                                     
  if (proclock == NULL)
  {
    LOCK *lock;
    PROCLOCKTAG proclocktag;
    uint32 proclock_hashcode;

    LWLockAcquire(partitionLock, LW_SHARED);

    lock = (LOCK *)hash_search_with_hash_value(LockMethodLockHash, (void *)locktag, locallock->hashcode, HASH_FIND, NULL);
    if (!lock)
    {
      elog(ERROR, "failed to re-find shared lock object");
    }

    proclocktag.myLock = lock;
    proclocktag.myProc = MyProc;

    proclock_hashcode = ProcLockHashCode(&proclocktag, locallock->hashcode);
    proclock = (PROCLOCK *)hash_search_with_hash_value(LockMethodProcLockHash, (void *)&proclocktag, proclock_hashcode, HASH_FIND, NULL);
    if (!proclock)
    {
      elog(ERROR, "failed to re-find shared proclock object");
    }
    LWLockRelease(partitionLock);
  }

  return proclock;
}

   
                    
                                                                           
                                                          
                                                        
   
                                                                        
                                                                
   
                                                                            
                                                                         
                                                                              
                                                                         
                                                                               
                                               
   
                                                                      
                                      
   
VirtualTransactionId *
GetLockConflicts(const LOCKTAG *locktag, LOCKMODE lockmode, int *countp)
{
  static VirtualTransactionId *vxids;
  LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
  LockMethod lockMethodTable;
  LOCK *lock;
  LOCKMASK conflictMask;
  SHM_QUEUE *procLocks;
  PROCLOCK *proclock;
  uint32 hashcode;
  LWLock *partitionLock;
  int count = 0;
  int fast_count = 0;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }
  lockMethodTable = LockMethods[lockmethodid];
  if (lockmode <= 0 || lockmode > lockMethodTable->numLockModes)
  {
    elog(ERROR, "unrecognized lock mode: %d", lockmode);
  }

     
                                                                           
                                                                            
                                                     
     
  if (InHotStandby)
  {
    if (vxids == NULL)
    {
      vxids = (VirtualTransactionId *)MemoryContextAlloc(TopMemoryContext, sizeof(VirtualTransactionId) * (MaxBackends + max_prepared_xacts + 1));
    }
  }
  else
  {
    vxids = (VirtualTransactionId *)palloc0(sizeof(VirtualTransactionId) * (MaxBackends + max_prepared_xacts + 1));
  }

                                                                            
  hashcode = LockTagHashCode(locktag);
  partitionLock = LockHashPartitionLock(hashcode);
  conflictMask = lockMethodTable->conflictTab[lockmode];

     
                                                                            
                                                                             
                                                           
     
  if (ConflictsWithRelationFastPath(locktag, lockmode))
  {
    int i;
    Oid relid = locktag->locktag_field2;
    VirtualTransactionId vxid;

       
                                                                   
                                                                         
                                                                           
                                                                 
                                                                           
                                                                       
                                                                       
                
       
    for (i = 0; i < ProcGlobal->allProcCount; i++)
    {
      PGPROC *proc = &ProcGlobal->allProcs[i];
      uint32 f;

                                         
      if (proc == MyProc)
      {
        continue;
      }

      LWLockAcquire(&proc->backendLock, LW_SHARED);

         
                                                                      
                                                                       
                                               
         
                                                                      
                                        
         
      if (proc->databaseId != locktag->locktag_field1)
      {
        LWLockRelease(&proc->backendLock);
        continue;
      }

      for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; f++)
      {
        uint32 lockmask;

                                                                  
        if (relid != proc->fpRelId[f])
        {
          continue;
        }
        lockmask = FAST_PATH_GET_BITS(proc, f);
        if (!lockmask)
        {
          continue;
        }
        lockmask <<= FAST_PATH_LOCKNUMBER_OFFSET;

           
                                                                       
                                                                       
           
        if ((lockmask & conflictMask) == 0)
        {
          break;
        }

                       
        GET_VXID_FROM_PGPROC(vxid, *proc);

        if (VirtualTransactionIdIsValid(vxid))
        {
          vxids[count++] = vxid;
        }
                                                     

                                                 
        break;
      }

      LWLockRelease(&proc->backendLock);
    }
  }

                                                       
  fast_count = count;

     
                                               
     
  LWLockAcquire(partitionLock, LW_SHARED);

  lock = (LOCK *)hash_search_with_hash_value(LockMethodLockHash, (const void *)locktag, hashcode, HASH_FIND, NULL);
  if (!lock)
  {
       
                                                                         
                                
       
    LWLockRelease(partitionLock);
    vxids[count].backendId = InvalidBackendId;
    vxids[count].localTransactionId = InvalidLocalTransactionId;
    if (countp)
    {
      *countp = count;
    }
    return vxids;
  }

     
                                                            
     

  procLocks = &(lock->procLocks);

  proclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, lockLink));

  while (proclock)
  {
    if (conflictMask & proclock->holdMask)
    {
      PGPROC *proc = proclock->tag.myProc;

                                         
      if (proc != MyProc)
      {
        VirtualTransactionId vxid;

        GET_VXID_FROM_PGPROC(vxid, *proc);

        if (VirtualTransactionIdIsValid(vxid))
        {
          int i;

                                        
          for (i = 0; i < fast_count; ++i)
          {
            if (VirtualTransactionIdEquals(vxids[i], vxid))
            {
              break;
            }
          }
          if (i >= fast_count)
          {
            vxids[count++] = vxid;
          }
        }
                                                     
      }
    }

    proclock = (PROCLOCK *)SHMQueueNext(procLocks, &proclock->lockLink, offsetof(PROCLOCK, lockLink));
  }

  LWLockRelease(partitionLock);

  if (count > MaxBackends + max_prepared_xacts)                          
  {
    elog(PANIC, "too many conflicting locks found");
  }

  vxids[count].backendId = InvalidBackendId;
  vxids[count].localTransactionId = InvalidLocalTransactionId;
  if (countp)
  {
    *countp = count;
  }
  return vxids;
}

   
                                                                            
                                                                               
                                                                           
                                
   
                                                                            
                                                                               
                                                                          
                                                  
   
static void
LockRefindAndRelease(LockMethod lockMethodTable, PGPROC *proc, LOCKTAG *locktag, LOCKMODE lockmode, bool decrement_strong_lock_count)
{
  LOCK *lock;
  PROCLOCK *proclock;
  PROCLOCKTAG proclocktag;
  uint32 hashcode;
  uint32 proclock_hashcode;
  LWLock *partitionLock;
  bool wakeupNeeded;

  hashcode = LockTagHashCode(locktag);
  partitionLock = LockHashPartitionLock(hashcode);

  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

     
                                                       
     
  lock = (LOCK *)hash_search_with_hash_value(LockMethodLockHash, (void *)locktag, hashcode, HASH_FIND, NULL);
  if (!lock)
  {
    elog(PANIC, "failed to re-find shared lock object");
  }

     
                                          
     
  proclocktag.myLock = lock;
  proclocktag.myProc = proc;

  proclock_hashcode = ProcLockHashCode(&proclocktag, hashcode);

  proclock = (PROCLOCK *)hash_search_with_hash_value(LockMethodProcLockHash, (void *)&proclocktag, proclock_hashcode, HASH_FIND, NULL);
  if (!proclock)
  {
    elog(PANIC, "failed to re-find shared proclock object");
  }

     
                                                                             
              
     
  if (!(proclock->holdMask & LOCKBIT_ON(lockmode)))
  {
    PROCLOCK_PRINT("lock_twophase_postcommit: WRONGTYPE", proclock);
    LWLockRelease(partitionLock);
    elog(WARNING, "you don't own a lock of type %s", lockMethodTable->lockModeNames[lockmode]);
    return;
  }

     
                                                                        
     
  wakeupNeeded = UnGrantLock(lock, lockmode, proclock, lockMethodTable);

  CleanUpLock(lock, proclock, lockMethodTable, hashcode, wakeupNeeded);

  LWLockRelease(partitionLock);

     
                                                                      
     
  if (decrement_strong_lock_count && ConflictsWithRelationFastPath(locktag, lockmode))
  {
    uint32 fasthashcode = FastPathStrongLockHashPartition(hashcode);

    SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
    Assert(FastPathStrongRelationLocks->count[fasthashcode] > 0);
    FastPathStrongRelationLocks->count[fasthashcode]--;
    SpinLockRelease(&FastPathStrongRelationLocks->mutex);
  }
}

   
                               
                                                                        
                                                     
   
                                                                            
                                                                         
                                                                               
                                                                        
   
                                                                        
                                                                             
                                                                         
                                                                      
                                                             
   
                                                                        
                                                                              
                                                                        
   
static void
CheckForSessionAndXactLocks(void)
{
  typedef struct
  {
    LOCKTAG lock;                                      
    bool sessLock;                                             
    bool xactLock;                                          
  } PerLockTagEntry;

  HASHCTL hash_ctl;
  HTAB *lockhtab;
  HASH_SEQ_STATUS status;
  LOCALLOCK *locallock;

                                                       
  hash_ctl.keysize = sizeof(LOCKTAG);
  hash_ctl.entrysize = sizeof(PerLockTagEntry);
  hash_ctl.hcxt = CurrentMemoryContext;

  lockhtab = hash_create("CheckForSessionAndXactLocks table", 256,                             
      &hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

                                                              
  hash_seq_init(&status, LockMethodLocalHash);

  while ((locallock = (LOCALLOCK *)hash_seq_search(&status)) != NULL)
  {
    LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
    PerLockTagEntry *hentry;
    bool found;
    int i;

       
                                                                      
                                                                   
       
    if (locallock->tag.lock.locktag_type == LOCKTAG_VIRTUALTRANSACTION)
    {
      continue;
    }

                                                      
    if (locallock->nLocks <= 0)
    {
      continue;
    }

                                                      
    hentry = (PerLockTagEntry *)hash_search(lockhtab, (void *)&locallock->tag.lock, HASH_ENTER, &found);
    if (!found)                                   
    {
      hentry->sessLock = hentry->xactLock = false;
    }

                                                                      
    for (i = locallock->numLockOwners - 1; i >= 0; i--)
    {
      if (lockOwners[i].owner == NULL)
      {
        hentry->sessLock = true;
      }
      else
      {
        hentry->xactLock = true;
      }
    }

       
                                                                          
                                                                
       
    if (hentry->sessLock && hentry->xactLock)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE while holding both session-level and transaction-level locks on the same object")));
    }
  }

                            
  hash_destroy(lockhtab);
}

   
                   
                                                                       
                                  
   
                                                       
   
                                                                        
                                                                  
                                                                        
                                                                  
   
void
AtPrepare_Locks(void)
{
  HASH_SEQ_STATUS status;
  LOCALLOCK *locallock;

                                                                       
  CheckForSessionAndXactLocks();

                                             
  hash_seq_init(&status, LockMethodLocalHash);

  while ((locallock = (LOCALLOCK *)hash_seq_search(&status)) != NULL)
  {
    TwoPhaseLockRecord record;
    LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
    bool haveSessionLock;
    bool haveXactLock;
    int i;

       
                                                                      
                                                                   
       
    if (locallock->tag.lock.locktag_type == LOCKTAG_VIRTUALTRANSACTION)
    {
      continue;
    }

                                                      
    if (locallock->nLocks <= 0)
    {
      continue;
    }

                                                                        
    haveSessionLock = haveXactLock = false;
    for (i = locallock->numLockOwners - 1; i >= 0; i--)
    {
      if (lockOwners[i].owner == NULL)
      {
        haveSessionLock = true;
      }
      else
      {
        haveXactLock = true;
      }
    }

                                                
    if (!haveXactLock)
    {
      continue;
    }

                                                          
    if (haveSessionLock)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE while holding both session-level and transaction-level locks on the same object")));
    }

       
                                                                         
                                                                        
                                                               
                    
       
    if (locallock->proclock == NULL)
    {
      locallock->proclock = FastPathGetRelationLockEntry(locallock);
      locallock->lock = locallock->proclock->tag.myLock;
    }

       
                                                                      
                                                                          
                                 
       
    locallock->holdsStrongLockCount = false;

       
                            
       
    memcpy(&(record.locktag), &(locallock->tag.lock), sizeof(LOCKTAG));
    record.lockmode = locallock->tag.mode;

    RegisterTwoPhaseRecord(TWOPHASE_RM_LOCK_ID, 0, &record, sizeof(TwoPhaseLockRecord));
  }
}

   
                     
                                      
   
                                                                      
                                                                       
                                                               
   
                                                                    
                                                                    
                                                                          
                                                                        
                                                                              
                                        
   
void
PostPrepare_Locks(TransactionId xid)
{
  PGPROC *newproc = TwoPhaseGetDummyProc(xid, false);
  HASH_SEQ_STATUS status;
  LOCALLOCK *locallock;
  LOCK *lock;
  PROCLOCK *proclock;
  PROCLOCKTAG proclocktag;
  int partition;

                                            
  Assert(MyProc->lockGroupLeader == NULL || MyProc->lockGroupLeader == MyProc);

                                                               
  START_CRIT_SECTION();

     
                                                                      
                                                                            
                  
     
                                                                          
                                                                            
               
     
  hash_seq_init(&status, LockMethodLocalHash);

  while ((locallock = (LOCALLOCK *)hash_seq_search(&status)) != NULL)
  {
    LOCALLOCKOWNER *lockOwners = locallock->lockOwners;
    bool haveSessionLock;
    bool haveXactLock;
    int i;

    if (locallock->proclock == NULL || locallock->lock == NULL)
    {
         
                                                                         
                                             
         
      Assert(locallock->nLocks == 0);
      RemoveLocalLock(locallock);
      continue;
    }

                           
    if (locallock->tag.lock.locktag_type == LOCKTAG_VIRTUALTRANSACTION)
    {
      continue;
    }

                                                                        
    haveSessionLock = haveXactLock = false;
    for (i = locallock->numLockOwners - 1; i >= 0; i--)
    {
      if (lockOwners[i].owner == NULL)
      {
        haveSessionLock = true;
      }
      else
      {
        haveXactLock = true;
      }
    }

                                                
    if (!haveXactLock)
    {
      continue;
    }

                                                          
    if (haveSessionLock)
    {
      ereport(PANIC, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE while holding both session-level and transaction-level locks on the same object")));
    }

                                                                    
    if (locallock->nLocks > 0)
    {
      locallock->proclock->releaseMask |= LOCKBIT_ON(locallock->tag.mode);
    }

                                                  
    RemoveLocalLock(locallock);
  }

     
                                               
     
  for (partition = 0; partition < NUM_LOCK_PARTITIONS; partition++)
  {
    LWLock *partitionLock;
    SHM_QUEUE *procLocks = &(MyProc->myProcLocks[partition]);
    PROCLOCK *nextplock;

    partitionLock = LockHashPartitionLockByIndex(partition);

       
                                                                     
                                                                          
                                                                        
                                                                       
                                                                          
                                                               
       
    if (SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, procLink)) == NULL)
    {
      continue;                                     
    }

    LWLockAcquire(partitionLock, LW_EXCLUSIVE);

    for (proclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, procLink)); proclock; proclock = nextplock)
    {
                                                                    
      nextplock = (PROCLOCK *)SHMQueueNext(procLocks, &proclock->procLink, offsetof(PROCLOCK, procLink));

      Assert(proclock->tag.myProc == MyProc);

      lock = proclock->tag.myLock;

                             
      if (lock->tag.locktag_type == LOCKTAG_VIRTUALTRANSACTION)
      {
        continue;
      }

      PROCLOCK_PRINT("PostPrepare_Locks", proclock);
      LOCK_PRINT("PostPrepare_Locks", lock, 0);
      Assert(lock->nRequested >= 0);
      Assert(lock->nGranted >= 0);
      Assert(lock->nGranted <= lock->nRequested);
      Assert((proclock->holdMask & ~lock->grantMask) == 0);

                                                                    
      if (proclock->releaseMask == 0)
      {
        continue;
      }

                                                 
      if (proclock->releaseMask != proclock->holdMask)
      {
        elog(PANIC, "we seem to have dropped a bit somewhere");
      }

         
                                                                  
                                                                        
                                                                      
                                                                         
                                                                     
                                                                      
                                                                       
         
                                                                      
                                                                         
                                                 
         
      SHMQueueDelete(&proclock->procLink);

         
                                                   
         
      proclocktag.myLock = lock;
      proclocktag.myProc = newproc;

         
                                                                     
                                                                
         
      Assert(proclock->groupLeader == proclock->tag.myProc);
      proclock->groupLeader = newproc;

         
                                                                         
                                                                      
                                      
         
      if (!hash_update_hash_key(LockMethodProcLockHash, (void *)proclock, (void *)&proclocktag))
      {
        elog(PANIC, "duplicate entry found while reassigning a prepared transaction's locks");
      }

                                                     
      SHMQueueInsertBefore(&(newproc->myProcLocks[partition]), &proclock->procLink);

      PROCLOCK_PRINT("PostPrepare_Locks: updated", proclock);
    }                                                

    LWLockRelease(partitionLock);
  }                           

  END_CRIT_SECTION();
}

   
                                                     
   
Size
LockShmemSize(void)
{
  Size size = 0;
  long max_table_size;

                       
  max_table_size = NLOCKENTS();
  size = add_size(size, hash_estimate_size(max_table_size, sizeof(LOCK)));

                           
  max_table_size *= 2;
  size = add_size(size, hash_estimate_size(max_table_size, sizeof(PROCLOCK)));

     
                                                                 
     
  size = add_size(size, size / 10);

  return size;
}

   
                                                                       
                                                       
   
                                                                     
                                                                           
                                                                       
                                                                        
                                                                
   
                                                                           
                                                                              
                                                                            
                       
   
LockData *
GetLockStatusData(void)
{
  LockData *data;
  PROCLOCK *proclock;
  HASH_SEQ_STATUS seqstat;
  int els;
  int el;
  int i;

  data = (LockData *)palloc(sizeof(LockData));

                                        
  els = MaxBackends;
  el = 0;
  data->locks = (LockInstanceData *)palloc(sizeof(LockInstanceData) * els);

     
                                                                         
                                                                            
                                                                          
                                                                        
                                                                          
                                                                             
                                                                         
                                                                       
                                                                         
                           
     
  for (i = 0; i < ProcGlobal->allProcCount; ++i)
  {
    PGPROC *proc = &ProcGlobal->allProcs[i];
    uint32 f;

    LWLockAcquire(&proc->backendLock, LW_SHARED);

    for (f = 0; f < FP_LOCK_SLOTS_PER_BACKEND; ++f)
    {
      LockInstanceData *instance;
      uint32 lockbits = FAST_PATH_GET_BITS(proc, f);

                                   
      if (!lockbits)
      {
        continue;
      }

      if (el >= els)
      {
        els += MaxBackends;
        data->locks = (LockInstanceData *)repalloc(data->locks, sizeof(LockInstanceData) * els);
      }

      instance = &data->locks[el];
      SET_LOCKTAG_RELATION(instance->locktag, proc->databaseId, proc->fpRelId[f]);
      instance->holdMask = lockbits << FAST_PATH_LOCKNUMBER_OFFSET;
      instance->waitLockMode = NoLock;
      instance->backend = proc->backendId;
      instance->lxid = proc->lxid;
      instance->pid = proc->pid;
      instance->leaderPid = proc->pid;
      instance->fastpath = true;

      el++;
    }

    if (proc->fpVXIDLock)
    {
      VirtualTransactionId vxid;
      LockInstanceData *instance;

      if (el >= els)
      {
        els += MaxBackends;
        data->locks = (LockInstanceData *)repalloc(data->locks, sizeof(LockInstanceData) * els);
      }

      vxid.backendId = proc->backendId;
      vxid.localTransactionId = proc->fpLocalTransactionId;

      instance = &data->locks[el];
      SET_LOCKTAG_VIRTUALTRANSACTION(instance->locktag, vxid);
      instance->holdMask = LOCKBIT_ON(ExclusiveLock);
      instance->waitLockMode = NoLock;
      instance->backend = proc->backendId;
      instance->lxid = proc->lxid;
      instance->pid = proc->pid;
      instance->leaderPid = proc->pid;
      instance->fastpath = true;

      el++;
    }

    LWLockRelease(&proc->backendLock);
  }

     
                                                                         
                                                                           
                              
     
                                                                    
                                                                            
                                                                       
                                                                
                                    
     
                                                                           
     
  for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
  {
    LWLockAcquire(LockHashPartitionLockByIndex(i), LW_SHARED);
  }

                                                       
  data->nelements = el + hash_get_num_entries(LockMethodProcLockHash);
  if (data->nelements > els)
  {
    els = data->nelements;
    data->locks = (LockInstanceData *)repalloc(data->locks, sizeof(LockInstanceData) * els);
  }

                                            
  hash_seq_init(&seqstat, LockMethodProcLockHash);

  while ((proclock = (PROCLOCK *)hash_seq_search(&seqstat)))
  {
    PGPROC *proc = proclock->tag.myProc;
    LOCK *lock = proclock->tag.myLock;
    LockInstanceData *instance = &data->locks[el];

    memcpy(&instance->locktag, &lock->tag, sizeof(LOCKTAG));
    instance->holdMask = proclock->holdMask;
    if (proc->waitLock == proclock->tag.myLock)
    {
      instance->waitLockMode = proc->waitLockMode;
    }
    else
    {
      instance->waitLockMode = NoLock;
    }
    instance->backend = proc->backendId;
    instance->lxid = proc->lxid;
    instance->pid = proc->pid;
    instance->leaderPid = proclock->groupLeader->pid;
    instance->fastpath = false;

    el++;
  }

     
                                                                          
                                                                             
                                                                          
                                                                     
                                    
     
  for (i = NUM_LOCK_PARTITIONS; --i >= 0;)
  {
    LWLockRelease(LockHashPartitionLockByIndex(i));
  }

  Assert(el == data->nelements);

  return data;
}

   
                                                                       
                                                                         
                                                                     
   
                                                                              
                                                                            
                                                                                
                                                                              
                                                                           
                                                                                
                                                                             
                                                                             
                                                                
   
                                                                            
                                                         
   
                                                                           
                                                                              
                                                                            
                       
   
BlockedProcsData *
GetBlockerStatusData(int blocked_pid)
{
  BlockedProcsData *data;
  PGPROC *proc;
  int i;

  data = (BlockedProcsData *)palloc(sizeof(BlockedProcsData));

     
                                                                         
                                                                            
                                                                          
                                                                           
     
  data->nprocs = data->nlocks = data->npids = 0;
  data->maxprocs = data->maxlocks = data->maxpids = MaxBackends;
  data->procs = (BlockedProcData *)palloc(sizeof(BlockedProcData) * data->maxprocs);
  data->locks = (LockInstanceData *)palloc(sizeof(LockInstanceData) * data->maxlocks);
  data->waiter_pids = (int *)palloc(sizeof(int) * data->maxpids);

     
                                                                           
                                                                             
                                                                            
                                                                             
                                                                           
                                                                  
                                                                         
                                                               
                                          
     
  LWLockAcquire(ProcArrayLock, LW_SHARED);

  proc = BackendPidGetProcWithLock(blocked_pid);

                                  
  if (proc != NULL)
  {
       
                                                                         
                               
       
    for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
    {
      LWLockAcquire(LockHashPartitionLockByIndex(i), LW_SHARED);
    }

    if (proc->lockGroupLeader == NULL)
    {
                                                      
      GetSingleProcBlockerStatusData(proc, data);
    }
    else
    {
                                                  
      dlist_iter iter;

      dlist_foreach(iter, &proc->lockGroupLeader->lockGroupMembers)
      {
        PGPROC *memberProc;

        memberProc = dlist_container(PGPROC, lockGroupLink, iter.cur);
        GetSingleProcBlockerStatusData(memberProc, data);
      }
    }

       
                                                             
       
    for (i = NUM_LOCK_PARTITIONS; --i >= 0;)
    {
      LWLockRelease(LockHashPartitionLockByIndex(i));
    }

    Assert(data->nprocs <= data->maxprocs);
  }

  LWLockRelease(ProcArrayLock);

  return data;
}

                                                                              
static void
GetSingleProcBlockerStatusData(PGPROC *blocked_proc, BlockedProcsData *data)
{
  LOCK *theLock = blocked_proc->waitLock;
  BlockedProcData *bproc;
  SHM_QUEUE *procLocks;
  PROCLOCK *proclock;
  PROC_QUEUE *waitQueue;
  PGPROC *proc;
  int queue_size;
  int i;

                                                 
  if (theLock == NULL)
  {
    return;
  }

                                
  bproc = &data->procs[data->nprocs++];
  bproc->pid = blocked_proc->pid;
  bproc->first_lock = data->nlocks;
  bproc->first_waiter = data->npids;

     
                                                                             
                                     
     

                                                     
  procLocks = &(theLock->procLocks);
  proclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, lockLink));
  while (proclock)
  {
    PGPROC *proc = proclock->tag.myProc;
    LOCK *lock = proclock->tag.myLock;
    LockInstanceData *instance;

    if (data->nlocks >= data->maxlocks)
    {
      data->maxlocks += MaxBackends;
      data->locks = (LockInstanceData *)repalloc(data->locks, sizeof(LockInstanceData) * data->maxlocks);
    }

    instance = &data->locks[data->nlocks];
    memcpy(&instance->locktag, &lock->tag, sizeof(LOCKTAG));
    instance->holdMask = proclock->holdMask;
    if (proc->waitLock == lock)
    {
      instance->waitLockMode = proc->waitLockMode;
    }
    else
    {
      instance->waitLockMode = NoLock;
    }
    instance->backend = proc->backendId;
    instance->lxid = proc->lxid;
    instance->pid = proc->pid;
    instance->leaderPid = proclock->groupLeader->pid;
    instance->fastpath = false;
    data->nlocks++;

    proclock = (PROCLOCK *)SHMQueueNext(procLocks, &proclock->lockLink, offsetof(PROCLOCK, lockLink));
  }

                                                                           
  waitQueue = &(theLock->waitProcs);
  queue_size = waitQueue->size;

  if (queue_size > data->maxpids - data->npids)
  {
    data->maxpids = Max(data->maxpids + MaxBackends, data->npids + queue_size);
    data->waiter_pids = (int *)repalloc(data->waiter_pids, sizeof(int) * data->maxpids);
  }

                                                                         
  proc = (PGPROC *)waitQueue->links.next;
  for (i = 0; i < queue_size; i++)
  {
    if (proc == blocked_proc)
    {
      break;
    }
    data->waiter_pids[data->npids++] = proc->pid;
    proc = (PGPROC *)proc->links.next;
  }

  bproc->num_locks = data->nlocks - bproc->first_lock;
  bproc->num_waiters = data->npids - bproc->first_waiter;
}

   
                                                                     
                                                          
                                                      
   
                                                                        
                                                                            
                                                                          
                                                                          
                                                                      
                             
   
xl_standby_lock *
GetRunningTransactionLocks(int *nlocks)
{
  xl_standby_lock *accessExclusiveLocks;
  PROCLOCK *proclock;
  HASH_SEQ_STATUS seqstat;
  int i;
  int index;
  int els;

     
                                                            
     
                                                                           
     
  for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
  {
    LWLockAcquire(LockHashPartitionLockByIndex(i), LW_SHARED);
  }

                                                       
  els = hash_get_num_entries(LockMethodProcLockHash);

     
                                                                          
                                                                           
     
  accessExclusiveLocks = palloc(els * sizeof(xl_standby_lock));

                                            
  hash_seq_init(&seqstat, LockMethodProcLockHash);

     
                                                                          
                                                                         
                                                                            
                                                                     
                               
     
  index = 0;
  while ((proclock = (PROCLOCK *)hash_seq_search(&seqstat)))
  {
                                                                       
    if ((proclock->holdMask & LOCKBIT_ON(AccessExclusiveLock)) && proclock->tag.myLock->tag.locktag_type == LOCKTAG_RELATION)
    {
      PGPROC *proc = proclock->tag.myProc;
      PGXACT *pgxact = &ProcGlobal->allPgXact[proc->pgprocno];
      LOCK *lock = proclock->tag.myLock;
      TransactionId xid = pgxact->xid;

         
                                                                  
                                                                         
                                                                      
                                                                       
         
      if (!TransactionIdIsValid(xid))
      {
        continue;
      }

      accessExclusiveLocks[index].xid = xid;
      accessExclusiveLocks[index].dbOid = lock->tag.locktag_field1;
      accessExclusiveLocks[index].relOid = lock->tag.locktag_field2;

      index++;
    }
  }

  Assert(index <= els);

     
                                                                          
                                                                             
                                                                          
                                                                     
                                    
     
  for (i = NUM_LOCK_PARTITIONS; --i >= 0;)
  {
    LWLockRelease(LockHashPartitionLockByIndex(i));
  }

  *nlocks = index;
  return accessExclusiveLocks;
}

                                               
const char *
GetLockmodeName(LOCKMETHODID lockmethodid, LOCKMODE mode)
{
  Assert(lockmethodid > 0 && lockmethodid < lengthof(LockMethods));
  Assert(mode > 0 && mode <= LockMethods[lockmethodid]->numLockModes);
  return LockMethods[lockmethodid]->lockModeNames[mode];
}

#ifdef LOCK_DEBUG
   
                                                         
   
                                                                  
   
void
DumpLocks(PGPROC *proc)
{
  SHM_QUEUE *procLocks;
  PROCLOCK *proclock;
  LOCK *lock;
  int i;

  if (proc == NULL)
  {
    return;
  }

  if (proc->waitLock)
  {
    LOCK_PRINT("DumpLocks: waiting on", proc->waitLock, 0);
  }

  for (i = 0; i < NUM_LOCK_PARTITIONS; i++)
  {
    procLocks = &(proc->myProcLocks[i]);

    proclock = (PROCLOCK *)SHMQueueNext(procLocks, procLocks, offsetof(PROCLOCK, procLink));

    while (proclock)
    {
      Assert(proclock->tag.myProc == proc);

      lock = proclock->tag.myLock;

      PROCLOCK_PRINT("DumpLocks", proclock);
      LOCK_PRINT("DumpLocks", lock, 0);

      proclock = (PROCLOCK *)SHMQueueNext(procLocks, &proclock->procLink, offsetof(PROCLOCK, procLink));
    }
  }
}

   
                        
   
                                                                  
   
void
DumpAllLocks(void)
{
  PGPROC *proc;
  PROCLOCK *proclock;
  LOCK *lock;
  HASH_SEQ_STATUS status;

  proc = MyProc;

  if (proc && proc->waitLock)
  {
    LOCK_PRINT("DumpAllLocks: waiting on", proc->waitLock, 0);
  }

  hash_seq_init(&status, LockMethodProcLockHash);

  while ((proclock = (PROCLOCK *)hash_seq_search(&status)) != NULL)
  {
    PROCLOCK_PRINT("DumpAllLocks", proclock);

    lock = proclock->tag.myLock;
    if (lock)
    {
      LOCK_PRINT("DumpAllLocks", lock, 0);
    }
    else
    {
      elog(LOG, "DumpAllLocks: proclock->tag.myLock = NULL");
    }
  }
}
#endif                 

   
                                        
   

   
                                                                   
   
                                                                             
                                                                        
                                                                            
   
                                                                            
                                                                              
                                                                                
                                                                             
                                                                         
                                                                           
                                                                            
                                                                               
   
                                                                             
                                                                             
                                                                           
                                                                 
                                                                          
                                                                           
                                                                             
                     
   
void
lock_twophase_recover(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  TwoPhaseLockRecord *rec = (TwoPhaseLockRecord *)recdata;
  PGPROC *proc = TwoPhaseGetDummyProc(xid, false);
  LOCKTAG *locktag;
  LOCKMODE lockmode;
  LOCKMETHODID lockmethodid;
  LOCK *lock;
  PROCLOCK *proclock;
  PROCLOCKTAG proclocktag;
  bool found;
  uint32 hashcode;
  uint32 proclock_hashcode;
  int partition;
  LWLock *partitionLock;
  LockMethod lockMethodTable;

  Assert(len == sizeof(TwoPhaseLockRecord));
  locktag = &rec->locktag;
  lockmode = rec->lockmode;
  lockmethodid = locktag->locktag_lockmethodid;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }
  lockMethodTable = LockMethods[lockmethodid];

  hashcode = LockTagHashCode(locktag);
  partition = LockHashPartition(hashcode);
  partitionLock = LockHashPartitionLock(hashcode);

  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

     
                                          
     
  lock = (LOCK *)hash_search_with_hash_value(LockMethodLockHash, (void *)locktag, hashcode, HASH_ENTER_NULL, &found);
  if (!lock)
  {
    LWLockRelease(partitionLock);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_locks_per_transaction.")));
  }

     
                                              
     
  if (!found)
  {
    lock->grantMask = 0;
    lock->waitMask = 0;
    SHMQueueInit(&(lock->procLocks));
    ProcQueueInit(&(lock->waitProcs));
    lock->nRequested = 0;
    lock->nGranted = 0;
    MemSet(lock->requested, 0, sizeof(int) * MAX_LOCKMODES);
    MemSet(lock->granted, 0, sizeof(int) * MAX_LOCKMODES);
    LOCK_PRINT("lock_twophase_recover: new", lock, lockmode);
  }
  else
  {
    LOCK_PRINT("lock_twophase_recover: found", lock, lockmode);
    Assert((lock->nRequested >= 0) && (lock->requested[lockmode] >= 0));
    Assert((lock->nGranted >= 0) && (lock->granted[lockmode] >= 0));
    Assert(lock->nGranted <= lock->nRequested);
  }

     
                                                 
     
  proclocktag.myLock = lock;
  proclocktag.myProc = proc;

  proclock_hashcode = ProcLockHashCode(&proclocktag, hashcode);

     
                                                   
     
  proclock = (PROCLOCK *)hash_search_with_hash_value(LockMethodProcLockHash, (void *)&proclocktag, proclock_hashcode, HASH_ENTER_NULL, &found);
  if (!proclock)
  {
                                                 
    if (lock->nRequested == 0)
    {
         
                                                                        
                                                                       
                                                                    
                                                  
         
      Assert(SHMQueueEmpty(&(lock->procLocks)));
      if (!hash_search_with_hash_value(LockMethodLockHash, (void *)&(lock->tag), hashcode, HASH_REMOVE, NULL))
      {
        elog(PANIC, "lock table corrupted");
      }
    }
    LWLockRelease(partitionLock);
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_locks_per_transaction.")));
  }

     
                                      
     
  if (!found)
  {
    Assert(proc->lockGroupLeader == NULL);
    proclock->groupLeader = proc;
    proclock->holdMask = 0;
    proclock->releaseMask = 0;
                                           
    SHMQueueInsertBefore(&lock->procLocks, &proclock->lockLink);
    SHMQueueInsertBefore(&(proc->myProcLocks[partition]), &proclock->procLink);
    PROCLOCK_PRINT("lock_twophase_recover: new", proclock);
  }
  else
  {
    PROCLOCK_PRINT("lock_twophase_recover: found", proclock);
    Assert((proclock->holdMask & ~lock->grantMask) == 0);
  }

     
                                                                      
                                                                           
     
  lock->nRequested++;
  lock->requested[lockmode]++;
  Assert((lock->nRequested > 0) && (lock->requested[lockmode] > 0));

     
                                                 
     
  if (proclock->holdMask & LOCKBIT_ON(lockmode))
  {
    elog(ERROR, "lock %s on object %u/%u/%u is already held", lockMethodTable->lockModeNames[lockmode], lock->tag.locktag_field1, lock->tag.locktag_field2, lock->tag.locktag_field3);
  }

     
                                                                             
                                                                    
                                                                  
     
  GrantLock(lock, proclock, lockmode);

     
                                                                            
                                                           
     
  if (ConflictsWithRelationFastPath(&lock->tag, lockmode))
  {
    uint32 fasthashcode = FastPathStrongLockHashPartition(hashcode);

    SpinLockAcquire(&FastPathStrongRelationLocks->mutex);
    FastPathStrongRelationLocks->count[fasthashcode]++;
    SpinLockRelease(&FastPathStrongRelationLocks->mutex);
  }

  LWLockRelease(partitionLock);
}

   
                                                                        
                                      
   
void
lock_twophase_standby_recover(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  TwoPhaseLockRecord *rec = (TwoPhaseLockRecord *)recdata;
  LOCKTAG *locktag;
  LOCKMODE lockmode;
  LOCKMETHODID lockmethodid;

  Assert(len == sizeof(TwoPhaseLockRecord));
  locktag = &rec->locktag;
  lockmode = rec->lockmode;
  lockmethodid = locktag->locktag_lockmethodid;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }

  if (lockmode == AccessExclusiveLock && locktag->locktag_type == LOCKTAG_RELATION)
  {
    StandbyAcquireAccessExclusiveLock(xid, locktag->locktag_field1            , locktag->locktag_field2 /* reloid */);
  }
}

   
                                                    
   
                                                          
   
void
lock_twophase_postcommit(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  TwoPhaseLockRecord *rec = (TwoPhaseLockRecord *)recdata;
  PGPROC *proc = TwoPhaseGetDummyProc(xid, true);
  LOCKTAG *locktag;
  LOCKMETHODID lockmethodid;
  LockMethod lockMethodTable;

  Assert(len == sizeof(TwoPhaseLockRecord));
  locktag = &rec->locktag;
  lockmethodid = locktag->locktag_lockmethodid;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }
  lockMethodTable = LockMethods[lockmethodid];

  LockRefindAndRelease(lockMethodTable, proc, locktag, rec->lockmode, true);
}

   
                                                      
   
                                                      
   
void
lock_twophase_postabort(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  lock_twophase_postcommit(xid, info, recdata, len);
}

   
                               
   
                                                                       
                                                                       
   
                                                                           
                                                                   
                                                                     
                                                                        
                                                                          
                                                                           
   
                                                                            
                                                              
                                                          
   
void
VirtualXactLockTableInsert(VirtualTransactionId vxid)
{
  Assert(VirtualTransactionIdIsValid(vxid));

  LWLockAcquire(&MyProc->backendLock, LW_EXCLUSIVE);

  Assert(MyProc->backendId == vxid.backendId);
  Assert(MyProc->fpLocalTransactionId == InvalidLocalTransactionId);
  Assert(MyProc->fpVXIDLock == false);

  MyProc->fpVXIDLock = true;
  MyProc->fpLocalTransactionId = vxid.localTransactionId;

  LWLockRelease(&MyProc->backendLock);
}

   
                                
   
                                                                        
                        
   
void
VirtualXactLockTableCleanup(void)
{
  bool fastpath;
  LocalTransactionId lxid;

  Assert(MyProc->backendId != InvalidBackendId);

     
                                   
     
  LWLockAcquire(&MyProc->backendLock, LW_EXCLUSIVE);

  fastpath = MyProc->fpVXIDLock;
  lxid = MyProc->fpLocalTransactionId;
  MyProc->fpVXIDLock = false;
  MyProc->fpLocalTransactionId = InvalidLocalTransactionId;

  LWLockRelease(&MyProc->backendLock);

     
                                                                           
                                                                     
     
  if (!fastpath && LocalTransactionIdIsValid(lxid))
  {
    VirtualTransactionId vxid;
    LOCKTAG locktag;

    vxid.backendId = MyBackendId;
    vxid.localTransactionId = lxid;
    SET_LOCKTAG_VIRTUALTRANSACTION(locktag, vxid);

    LockRefindAndRelease(LockMethods[DEFAULT_LOCKMETHOD], MyProc, &locktag, ExclusiveLock, false);
  }
}

   
                           
   
                                                                            
                                                                              
                                                                           
                                    
   
                                                                            
                                                   
   
static bool
XactLockForVirtualXact(VirtualTransactionId vxid, TransactionId xid, bool wait)
{
  bool more = false;

                                                               
  if (max_prepared_xacts == 0)
  {
    return true;
  }

  do
  {
    LockAcquireResult lar;
    LOCKTAG tag;

                                               
    if (more)
    {
      xid = InvalidTransactionId;
      more = false;
    }

                                             
    if (!TransactionIdIsValid(xid))
    {
      xid = TwoPhaseGetXidByVirtualXID(vxid, &more);
    }
    if (!TransactionIdIsValid(xid))
    {
      Assert(!more);
      return true;
    }

                                           
    SET_LOCKTAG_TRANSACTION(tag, xid);
    lar = LockAcquire(&tag, ShareLock, false, !wait);
    if (lar == LOCKACQUIRE_NOT_AVAIL)
    {
      return false;
    }
    LockRelease(&tag, ShareLock, false);
  } while (more);

  return true;
}

   
                    
   
                                                                             
                                                          
   
                                                                               
                                      
   
bool
VirtualXactLock(VirtualTransactionId vxid, bool wait)
{
  LOCKTAG tag;
  PGPROC *proc;
  TransactionId xid = InvalidTransactionId;

  Assert(VirtualTransactionIdIsValid(vxid));

  if (VirtualTransactionIdIsRecoveredPreparedXact(vxid))
  {
                                                                  
    return XactLockForVirtualXact(vxid, vxid.localTransactionId, wait);
  }

  SET_LOCKTAG_VIRTUALTRANSACTION(tag, vxid);

     
                                                                            
                                                                         
                                                                            
                                                                      
                                                                          
                                      
     
  proc = BackendIdGetProc(vxid.backendId);
  if (proc == NULL)
  {
    return XactLockForVirtualXact(vxid, InvalidTransactionId, wait);
  }

     
                                                                      
                                                                           
                                            
     
  LWLockAcquire(&proc->backendLock, LW_EXCLUSIVE);

  if (proc->backendId != vxid.backendId || proc->fpLocalTransactionId != vxid.localTransactionId)
  {
                    
    LWLockRelease(&proc->backendLock);
    return XactLockForVirtualXact(vxid, InvalidTransactionId, wait);
  }

     
                                                                        
                                                                         
     
  if (!wait)
  {
    LWLockRelease(&proc->backendLock);
    return false;
  }

     
                                                                           
                                                                        
                                                    
     
  if (proc->fpVXIDLock)
  {
    PROCLOCK *proclock;
    uint32 hashcode;
    LWLock *partitionLock;

    hashcode = LockTagHashCode(&tag);

    partitionLock = LockHashPartitionLock(hashcode);
    LWLockAcquire(partitionLock, LW_EXCLUSIVE);

    proclock = SetupLockInTable(LockMethods[DEFAULT_LOCKMETHOD], proc, &tag, hashcode, ExclusiveLock);
    if (!proclock)
    {
      LWLockRelease(partitionLock);
      LWLockRelease(&proc->backendLock);
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_locks_per_transaction.")));
    }
    GrantLock(proclock->tag.myLock, proclock, ExclusiveLock);

    LWLockRelease(partitionLock);

    proc->fpVXIDLock = false;
  }

     
                                                                            
                                                                           
                                                                          
                                                                              
                                                                             
                                                                           
     
  xid = ProcGlobal->allPgXact[proc->pgprocno].xid;

                                  
  LWLockRelease(&proc->backendLock);

                     
  (void)LockAcquire(&tag, ShareLock, false, false);

  LockRelease(&tag, ShareLock, false);
  return XactLockForVirtualXact(vxid, xid, wait);
}

   
                   
   
                                                     
   
int
LockWaiterCount(const LOCKTAG *locktag)
{
  LOCKMETHODID lockmethodid = locktag->locktag_lockmethodid;
  LOCK *lock;
  bool found;
  uint32 hashcode;
  LWLock *partitionLock;
  int waiters = 0;

  if (lockmethodid <= 0 || lockmethodid >= lengthof(LockMethods))
  {
    elog(ERROR, "unrecognized lock method: %d", lockmethodid);
  }

  hashcode = LockTagHashCode(locktag);
  partitionLock = LockHashPartitionLock(hashcode);
  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

  lock = (LOCK *)hash_search_with_hash_value(LockMethodLockHash, (const void *)locktag, hashcode, HASH_FIND, &found);
  if (found)
  {
    Assert(lock != NULL);
    waiters = lock->nRequested;
  }
  LWLockRelease(partitionLock);

  return waiters;
}
