                                                                            
   
            
                              
   
                                                                           
                                                                        
                                                                        
                                                                            
                                                                          
                                        
   
                                                                               
                                                                           
                                                                            
                                                                           
                                                                            
                                                                          
                                                                              
                                                                            
                                                                       
   
                                                                         
                                                                        
   
                  
                                       
   
          
   
                                                                
                                                                  
                                                                            
                                                                    
                                                                             
                                                                 
   
                                                                             
                                                         
   
                                                                             
                                                                         
                                                                      
                                                                               
              
   
                                                                               
                                                            
                                                                        
   
                                                                              
                                                                              
            
   
                                                                         
                                                                        
                                                    
   
   
                                                                              
                                                                          
                                                                           
                                                                         
                                                                           
 
                                                                   
                                                          
                                                        
                                                                              
                 
                                              
   
                                                                             
                                                                           
                                                                             
   
#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"
#include "pg_trace.h"
#include "postmaster/postmaster.h"
#include "replication/slot.h"
#include "storage/ipc.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/proclist.h"
#include "storage/spin.h"
#include "utils/memutils.h"

#ifdef LWLOCK_STATS
#include "utils/hsearch.h"
#endif

                                                            
extern slock_t *ShmemLock;

#define LW_FLAG_HAS_WAITERS ((uint32)1 << 30)
#define LW_FLAG_RELEASE_OK ((uint32)1 << 29)
#define LW_FLAG_LOCKED ((uint32)1 << 28)

#define LW_VAL_EXCLUSIVE ((uint32)1 << 24)
#define LW_VAL_SHARED 1

#define LW_LOCK_MASK ((uint32)((1 << 25) - 1))
                                                                         
#define LW_SHARED_MASK ((uint32)((1 << 24) - 1))

   
                                                                            
                           
   
static const char **LWLockTrancheArray = NULL;
static int LWLockTranchesAllocated = 0;

#define T_NAME(lock) (LWLockTrancheArray[(lock)->tranche])

   
                                                                                
                                                                             
                                                    
   
LWLockPadded *MainLWLockArray = NULL;

   
                                                                     
                                                                          
                                                                               
                                                         
   
#define MAX_SIMUL_LWLOCKS 200

                                                   
typedef struct LWLockHandle
{
  LWLock *lock;
  LWLockMode mode;
} LWLockHandle;

static int num_held_lwlocks = 0;
static LWLockHandle held_lwlocks[MAX_SIMUL_LWLOCKS];

                                                                      
typedef struct NamedLWLockTrancheRequest
{
  char tranche_name[NAMEDATALEN];
  int num_lwlocks;
} NamedLWLockTrancheRequest;

NamedLWLockTrancheRequest *NamedLWLockTrancheRequestArray = NULL;
static int NamedLWLockTrancheRequestsAllocated = 0;
int NamedLWLockTrancheRequests = 0;

NamedLWLockTranche *NamedLWLockTrancheArray = NULL;

static bool lock_named_request_allowed = true;

static void
InitializeLWLocks(void);
static void
RegisterLWLockTranches(void);

static inline void
LWLockReportWaitStart(LWLock *lock);
static inline void
LWLockReportWaitEnd(void);

#ifdef LWLOCK_STATS
typedef struct lwlock_stats_key
{
  int tranche;
  void *instance;
} lwlock_stats_key;

typedef struct lwlock_stats
{
  lwlock_stats_key key;
  int sh_acquire_count;
  int ex_acquire_count;
  int block_count;
  int dequeue_self_count;
  int spin_delay_count;
} lwlock_stats;

static HTAB *lwlock_stats_htab;
static lwlock_stats lwlock_stats_dummy;
#endif

#ifdef LOCK_DEBUG
bool Trace_lwlocks = false;

inline static void
PRINT_LWDEBUG(const char *where, LWLock *lock, LWLockMode mode)
{
                                                                            
  if (Trace_lwlocks)
  {
    uint32 state = pg_atomic_read_u32(&lock->state);

    ereport(LOG, (errhidestmt(true), errhidecontext(true), errmsg_internal("%d: %s(%s %p): excl %u shared %u haswaiters %u waiters %u rOK %d", MyProcPid, where, T_NAME(lock), lock, (state & LW_VAL_EXCLUSIVE) != 0, state & LW_SHARED_MASK, (state & LW_FLAG_HAS_WAITERS) != 0, pg_atomic_read_u32(&lock->nwaiters), (state & LW_FLAG_RELEASE_OK) != 0)));
  }
}

inline static void
LOG_LWDEBUG(const char *where, LWLock *lock, const char *msg)
{
                                                                            
  if (Trace_lwlocks)
  {
    ereport(LOG, (errhidestmt(true), errhidecontext(true), errmsg_internal("%s(%s %p): %s", where, T_NAME(lock), lock, msg)));
  }
}

#else                     
#define PRINT_LWDEBUG(a, b, c) ((void)0)
#define LOG_LWDEBUG(a, b, c) ((void)0)
#endif                 

#ifdef LWLOCK_STATS

static void
init_lwlock_stats(void);
static void
print_lwlock_stats(int code, Datum arg);
static lwlock_stats *
get_lwlock_stats_entry(LWLock *lockid);

static void
init_lwlock_stats(void)
{
  HASHCTL ctl;
  static MemoryContext lwlock_stats_cxt = NULL;
  static bool exit_registered = false;

  if (lwlock_stats_cxt != NULL)
  {
    MemoryContextDelete(lwlock_stats_cxt);
  }

     
                                                                       
                                                                         
                                                                          
                                                                             
                                                                             
                                                                             
     
  lwlock_stats_cxt = AllocSetContextCreate(TopMemoryContext, "LWLock stats", ALLOCSET_DEFAULT_SIZES);
  MemoryContextAllowInCriticalSection(lwlock_stats_cxt, true);

  MemSet(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(lwlock_stats_key);
  ctl.entrysize = sizeof(lwlock_stats);
  ctl.hcxt = lwlock_stats_cxt;
  lwlock_stats_htab = hash_create("lwlock stats", 16384, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
  if (!exit_registered)
  {
    on_shmem_exit(print_lwlock_stats, 0);
    exit_registered = true;
  }
}

static void
print_lwlock_stats(int code, Datum arg)
{
  HASH_SEQ_STATUS scan;
  lwlock_stats *lwstats;

  hash_seq_init(&scan, lwlock_stats_htab);

                                                                     
  LWLockAcquire(&MainLWLockArray[0].lock, LW_EXCLUSIVE);

  while ((lwstats = (lwlock_stats *)hash_seq_search(&scan)) != NULL)
  {
    fprintf(stderr, "PID %d lwlock %s %p: shacq %u exacq %u blk %u spindelay %u dequeue self %u\n", MyProcPid, LWLockTrancheArray[lwstats->key.tranche], lwstats->key.instance, lwstats->sh_acquire_count, lwstats->ex_acquire_count, lwstats->block_count, lwstats->spin_delay_count, lwstats->dequeue_self_count);
  }

  LWLockRelease(&MainLWLockArray[0].lock);
}

static lwlock_stats *
get_lwlock_stats_entry(LWLock *lock)
{
  lwlock_stats_key key;
  lwlock_stats *lwstats;
  bool found;

     
                                                                            
                                                                             
                                           
     
  if (lwlock_stats_htab == NULL)
  {
    return &lwlock_stats_dummy;
  }

                                  
  MemSet(&key, 0, sizeof(key));
  key.tranche = lock->tranche;
  key.instance = lock;
  lwstats = hash_search(lwlock_stats_htab, &key, HASH_ENTER, &found);
  if (!found)
  {
    lwstats->sh_acquire_count = 0;
    lwstats->ex_acquire_count = 0;
    lwstats->block_count = 0;
    lwstats->dequeue_self_count = 0;
    lwstats->spin_delay_count = 0;
  }
  return lwstats;
}
#endif                   

   
                                                                        
                                
   
static int
NumLWLocksByNamedTranches(void)
{
  int numLocks = 0;
  int i;

  for (i = 0; i < NamedLWLockTrancheRequests; i++)
  {
    numLocks += NamedLWLockTrancheRequestArray[i].num_lwlocks;
  }

  return numLocks;
}

   
                                                              
   
Size
LWLockShmemSize(void)
{
  Size size;
  int i;
  int numLocks = NUM_FIXED_LWLOCKS;

  numLocks += NumLWLocksByNamedTranches();

                                   
  size = mul_size(numLocks, sizeof(LWLockPadded));

                                                                      
  size = add_size(size, sizeof(int) + LWLOCK_PADDED_SIZE);

                                 
  size = add_size(size, mul_size(NamedLWLockTrancheRequests, sizeof(NamedLWLockTranche)));

                                       
  for (i = 0; i < NamedLWLockTrancheRequests; i++)
  {
    size = add_size(size, strlen(NamedLWLockTrancheRequestArray[i].tranche_name) + 1);
  }

                                                      
  lock_named_request_allowed = false;

  return size;
}

   
                                                                       
                                                                  
   
void
CreateLWLocks(void)
{
  StaticAssertStmt(LW_VAL_EXCLUSIVE > (uint32)MAX_BACKENDS, "MAX_BACKENDS too big for lwlock.c");

  StaticAssertStmt(sizeof(LWLock) <= LWLOCK_MINIMAL_SIZE && sizeof(LWLock) <= LWLOCK_PADDED_SIZE, "Miscalculated LWLock padding");

  if (!IsUnderPostmaster)
  {
    Size spaceLocks = LWLockShmemSize();
    int *LWLockCounter;
    char *ptr;

                        
    ptr = (char *)ShmemAlloc(spaceLocks);

                                                       
    ptr += sizeof(int);

                                                  
    ptr += LWLOCK_PADDED_SIZE - ((uintptr_t)ptr) % LWLOCK_PADDED_SIZE;

    MainLWLockArray = (LWLockPadded *)ptr;

       
                                                                        
                                            
       
    LWLockCounter = (int *)((char *)MainLWLockArray - sizeof(int));
    *LWLockCounter = LWTRANCHE_FIRST_USER_DEFINED;

                                
    InitializeLWLocks();
  }

                                    
  RegisterLWLockTranches();
}

   
                                                                            
   
static void
InitializeLWLocks(void)
{
  int numNamedLocks = NumLWLocksByNamedTranches();
  int id;
  int i;
  int j;
  LWLockPadded *lock;

                                                       
  for (id = 0, lock = MainLWLockArray; id < NUM_INDIVIDUAL_LWLOCKS; id++, lock++)
  {
    LWLockInitialize(&lock->lock, id);
  }

                                                       
  lock = MainLWLockArray + NUM_INDIVIDUAL_LWLOCKS;
  for (id = 0; id < NUM_BUFFER_PARTITIONS; id++, lock++)
  {
    LWLockInitialize(&lock->lock, LWTRANCHE_BUFFER_MAPPING);
  }

                                               
  lock = MainLWLockArray + NUM_INDIVIDUAL_LWLOCKS + NUM_BUFFER_PARTITIONS;
  for (id = 0; id < NUM_LOCK_PARTITIONS; id++, lock++)
  {
    LWLockInitialize(&lock->lock, LWTRANCHE_LOCK_MANAGER);
  }

                                                         
  lock = MainLWLockArray + NUM_INDIVIDUAL_LWLOCKS + NUM_BUFFER_PARTITIONS + NUM_LOCK_PARTITIONS;
  for (id = 0; id < NUM_PREDICATELOCK_PARTITIONS; id++, lock++)
  {
    LWLockInitialize(&lock->lock, LWTRANCHE_PREDICATE_LOCK_MANAGER);
  }

                                  
  if (NamedLWLockTrancheRequests > 0)
  {
    char *trancheNames;

    NamedLWLockTrancheArray = (NamedLWLockTranche *)&MainLWLockArray[NUM_FIXED_LWLOCKS + numNamedLocks];

    trancheNames = (char *)NamedLWLockTrancheArray + (NamedLWLockTrancheRequests * sizeof(NamedLWLockTranche));
    lock = &MainLWLockArray[NUM_FIXED_LWLOCKS];

    for (i = 0; i < NamedLWLockTrancheRequests; i++)
    {
      NamedLWLockTrancheRequest *request;
      NamedLWLockTranche *tranche;
      char *name;

      request = &NamedLWLockTrancheRequestArray[i];
      tranche = &NamedLWLockTrancheArray[i];

      name = trancheNames;
      trancheNames += strlen(request->tranche_name) + 1;
      strcpy(name, request->tranche_name);
      tranche->trancheId = LWLockNewTrancheId();
      tranche->trancheName = name;

      for (j = 0; j < request->num_lwlocks; j++, lock++)
      {
        LWLockInitialize(&lock->lock, tranche->trancheId);
      }
    }
  }
}

   
                                                           
   
static void
RegisterLWLockTranches(void)
{
  int i;

  if (LWLockTrancheArray == NULL)
  {
    LWLockTranchesAllocated = 128;
    LWLockTrancheArray = (const char **)MemoryContextAllocZero(TopMemoryContext, LWLockTranchesAllocated * sizeof(char *));
    Assert(LWLockTranchesAllocated >= LWTRANCHE_FIRST_USER_DEFINED);
  }

  for (i = 0; i < NUM_INDIVIDUAL_LWLOCKS; ++i)
  {
    LWLockRegisterTranche(i, MainLWLockNames[i]);
  }

  LWLockRegisterTranche(LWTRANCHE_BUFFER_MAPPING, "buffer_mapping");
  LWLockRegisterTranche(LWTRANCHE_LOCK_MANAGER, "lock_manager");
  LWLockRegisterTranche(LWTRANCHE_PREDICATE_LOCK_MANAGER, "predicate_lock_manager");
  LWLockRegisterTranche(LWTRANCHE_PARALLEL_QUERY_DSA, "parallel_query_dsa");
  LWLockRegisterTranche(LWTRANCHE_SESSION_DSA, "session_dsa");
  LWLockRegisterTranche(LWTRANCHE_SESSION_RECORD_TABLE, "session_record_table");
  LWLockRegisterTranche(LWTRANCHE_SESSION_TYPMOD_TABLE, "session_typmod_table");
  LWLockRegisterTranche(LWTRANCHE_SHARED_TUPLESTORE, "shared_tuplestore");
  LWLockRegisterTranche(LWTRANCHE_TBM, "tbm");
  LWLockRegisterTranche(LWTRANCHE_PARALLEL_APPEND, "parallel_append");
  LWLockRegisterTranche(LWTRANCHE_PARALLEL_HASH_JOIN, "parallel_hash_join");
  LWLockRegisterTranche(LWTRANCHE_SXACT, "serializable_xact");

                                
  for (i = 0; i < NamedLWLockTrancheRequests; i++)
  {
    LWLockRegisterTranche(NamedLWLockTrancheArray[i].trancheId, NamedLWLockTrancheArray[i].trancheName);
  }
}

   
                                                                            
   
void
InitLWLockAccess(void)
{
#ifdef LWLOCK_STATS
  init_lwlock_stats();
#endif
}

   
                                                                       
                       
   
                                                                          
                                                                     
                                                                         
   
LWLockPadded *
GetNamedLWLockTranche(const char *tranche_name)
{
  int lock_pos;
  int i;

     
                                                                          
                                                                             
                                           
     
  lock_pos = NUM_FIXED_LWLOCKS;
  for (i = 0; i < NamedLWLockTrancheRequests; i++)
  {
    if (strcmp(NamedLWLockTrancheRequestArray[i].tranche_name, tranche_name) == 0)
    {
      return &MainLWLockArray[lock_pos];
    }

    lock_pos += NamedLWLockTrancheRequestArray[i].num_lwlocks;
  }

  if (i >= NamedLWLockTrancheRequests)
  {
    elog(ERROR, "requested tranche is not registered");
  }

                                   
  return NULL;
}

   
                              
   
int
LWLockNewTrancheId(void)
{
  int result;
  int *LWLockCounter;

  LWLockCounter = (int *)((char *)MainLWLockArray - sizeof(int));
  SpinLockAcquire(ShmemLock);
  result = (*LWLockCounter)++;
  SpinLockRelease(ShmemLock);

  return result;
}

   
                                                                            
                                                                          
                                                                 
                                                    
   
void
LWLockRegisterTranche(int tranche_id, const char *tranche_name)
{
  Assert(LWLockTrancheArray != NULL);

  if (tranche_id >= LWLockTranchesAllocated)
  {
    int i = LWLockTranchesAllocated;
    int j = LWLockTranchesAllocated;

    while (i <= tranche_id)
    {
      i *= 2;
    }

    LWLockTrancheArray = (const char **)repalloc(LWLockTrancheArray, i * sizeof(char *));
    LWLockTranchesAllocated = i;
    while (j < LWLockTranchesAllocated)
    {
      LWLockTrancheArray[j++] = NULL;
    }
  }

  LWLockTrancheArray[tranche_id] = tranche_name;
}

   
                             
                                                              
             
   
                                                                       
                                                       
                                                                           
                                                                           
                                                                          
            
   
void
RequestNamedLWLockTranche(const char *tranche_name, int num_lwlocks)
{
  NamedLWLockTrancheRequest *request;

  if (IsUnderPostmaster || !lock_named_request_allowed)
  {
    return;               
  }

  if (NamedLWLockTrancheRequestArray == NULL)
  {
    NamedLWLockTrancheRequestsAllocated = 16;
    NamedLWLockTrancheRequestArray = (NamedLWLockTrancheRequest *)MemoryContextAlloc(TopMemoryContext, NamedLWLockTrancheRequestsAllocated * sizeof(NamedLWLockTrancheRequest));
  }

  if (NamedLWLockTrancheRequests >= NamedLWLockTrancheRequestsAllocated)
  {
    int i = NamedLWLockTrancheRequestsAllocated;

    while (i <= NamedLWLockTrancheRequests)
    {
      i *= 2;
    }

    NamedLWLockTrancheRequestArray = (NamedLWLockTrancheRequest *)repalloc(NamedLWLockTrancheRequestArray, i * sizeof(NamedLWLockTrancheRequest));
    NamedLWLockTrancheRequestsAllocated = i;
  }

  request = &NamedLWLockTrancheRequestArray[NamedLWLockTrancheRequests];
  Assert(strlen(tranche_name) + 1 < NAMEDATALEN);
  StrNCpy(request->tranche_name, tranche_name, NAMEDATALEN);
  request->num_lwlocks = num_lwlocks;
  NamedLWLockTrancheRequests++;
}

   
                                                                       
   
void
LWLockInitialize(LWLock *lock, int tranche_id)
{
  pg_atomic_init_u32(&lock->state, LW_FLAG_RELEASE_OK);
#ifdef LOCK_DEBUG
  pg_atomic_init_u32(&lock->nwaiters, 0);
#endif
  lock->tranche = tranche_id;
  proclist_init(&lock->waiters);
}

   
                                                      
   
                                                                       
                                                                        
                                       
   
static inline void
LWLockReportWaitStart(LWLock *lock)
{
  pgstat_report_wait_start(PG_WAIT_LWLOCK | lock->tranche);
}

   
                                                    
   
static inline void
LWLockReportWaitEnd(void)
{
  pgstat_report_wait_end();
}

   
                                                                         
   
const char *
GetLWLockIdentifier(uint32 classId, uint16 eventId)
{
  Assert(classId == PG_WAIT_LWLOCK);

     
                                                                         
                                                                            
                                                                     
     
  if (eventId >= LWLockTranchesAllocated || LWLockTrancheArray[eventId] == NULL)
  {
    return "extension";
  }

  return LWLockTrancheArray[eventId];
}

   
                                                                               
            
   
                                                                               
                
   
                                                            
   
static bool
LWLockAttemptLock(LWLock *lock, LWLockMode mode)
{
  uint32 old_state;

  AssertArg(mode == LW_EXCLUSIVE || mode == LW_SHARED);

     
                                                                           
                             
     
  old_state = pg_atomic_read_u32(&lock->state);

                                                                            
  while (true)
  {
    uint32 desired_state;
    bool lock_free;

    desired_state = old_state;

    if (mode == LW_EXCLUSIVE)
    {
      lock_free = (old_state & LW_LOCK_MASK) == 0;
      if (lock_free)
      {
        desired_state += LW_VAL_EXCLUSIVE;
      }
    }
    else
    {
      lock_free = (old_state & LW_VAL_EXCLUSIVE) == 0;
      if (lock_free)
      {
        desired_state += LW_VAL_SHARED;
      }
    }

       
                                                                       
                                                                         
                                                                         
                                                                           
                                                                         
                                                            
       
                                                              
       
    if (pg_atomic_compare_exchange_u32(&lock->state, &old_state, desired_state))
    {
      if (lock_free)
      {
                                  
#ifdef LOCK_DEBUG
        if (mode == LW_EXCLUSIVE)
        {
          lock->owner = MyProc;
        }
#endif
        return false;
      }
      else
      {
        return true;                                 
      }
    }
  }
  pg_unreachable();
}

   
                                                            
   
                                                                            
                                  
   
                                             
   
static void
LWLockWaitListLock(LWLock *lock)
{
  uint32 old_state;
#ifdef LWLOCK_STATS
  lwlock_stats *lwstats;
  uint32 delays = 0;

  lwstats = get_lwlock_stats_entry(lock);
#endif

  while (true)
  {
                                                  
    old_state = pg_atomic_fetch_or_u32(&lock->state, LW_FLAG_LOCKED);
    if (!(old_state & LW_FLAG_LOCKED))
    {
      break;               
    }

                                                                        
    {
      SpinDelayStatus delayStatus;

      init_local_spin_delay(&delayStatus);

      while (old_state & LW_FLAG_LOCKED)
      {
        perform_spin_delay(&delayStatus);
        old_state = pg_atomic_read_u32(&lock->state);
      }
#ifdef LWLOCK_STATS
      delays += delayStatus.delays;
#endif
      finish_spin_delay(&delayStatus);
    }

       
                                                                          
                                         
       
  }

#ifdef LWLOCK_STATS
  lwstats->spin_delay_count += delays;
#endif
}

   
                                  
   
                                                                          
                                       
   
static void
LWLockWaitListUnlock(LWLock *lock)
{
  uint32 old_state PG_USED_FOR_ASSERTS_ONLY;

  old_state = pg_atomic_fetch_and_u32(&lock->state, ~LW_FLAG_LOCKED);

  Assert(old_state & LW_FLAG_LOCKED);
}

   
                                                                            
   
static void
LWLockWakeup(LWLock *lock)
{
  bool new_release_ok;
  bool wokeup_somebody = false;
  proclist_head wakeup;
  proclist_mutable_iter iter;

  proclist_init(&wakeup);

  new_release_ok = true;

                                                           
  LWLockWaitListLock(lock);

  proclist_foreach_modify(iter, &lock->waiters, lwWaitLink)
  {
    PGPROC *waiter = GetPGProcByNumber(iter.cur);

    if (wokeup_somebody && waiter->lwWaitMode == LW_EXCLUSIVE)
    {
      continue;
    }

    proclist_delete(&lock->waiters, iter.cur, lwWaitLink);
    proclist_push_tail(&wakeup, iter.cur, lwWaitLink);

    if (waiter->lwWaitMode != LW_WAIT_UNTIL_FREE)
    {
         
                                                                        
                                                                       
                        
         
      new_release_ok = false;

         
                                                 
         
      wokeup_somebody = true;
    }

       
                                                                         
                        
       
    if (waiter->lwWaitMode == LW_EXCLUSIVE)
    {
      break;
    }
  }

  Assert(proclist_is_empty(&wakeup) || pg_atomic_read_u32(&lock->state) & LW_FLAG_HAS_WAITERS);

                                                                 
  {
    uint32 old_state;
    uint32 desired_state;

    old_state = pg_atomic_read_u32(&lock->state);
    while (true)
    {
      desired_state = old_state;

                                 

      if (new_release_ok)
      {
        desired_state |= LW_FLAG_RELEASE_OK;
      }
      else
      {
        desired_state &= ~LW_FLAG_RELEASE_OK;
      }

      if (proclist_is_empty(&wakeup))
      {
        desired_state &= ~LW_FLAG_HAS_WAITERS;
      }

      desired_state &= ~LW_FLAG_LOCKED;                   

      if (pg_atomic_compare_exchange_u32(&lock->state, &old_state, desired_state))
      {
        break;
      }
    }
  }

                                                    
  proclist_foreach_modify(iter, &wakeup, lwWaitLink)
  {
    PGPROC *waiter = GetPGProcByNumber(iter.cur);

    LOG_LWDEBUG("LWLockRelease", lock, "release waiter");
    proclist_delete(&wakeup, iter.cur, lwWaitLink);

       
                                                                          
                                                                        
                                                                          
                                                                          
                        
       
                                                                          
                     
       
    pg_write_barrier();
    waiter->lwWaiting = false;
    PGSemaphoreUnlock(waiter->sem);
  }
}

   
                                          
   
                                            
   
static void
LWLockQueueSelf(LWLock *lock, LWLockMode mode)
{
     
                                                                       
                                                                        
                            
     
  if (MyProc == NULL)
  {
    elog(PANIC, "cannot wait without a PGPROC structure");
  }

  if (MyProc->lwWaiting)
  {
    elog(PANIC, "queueing for lock while waiting on another one");
  }

  LWLockWaitListLock(lock);

                                                     
  pg_atomic_fetch_or_u32(&lock->state, LW_FLAG_HAS_WAITERS);

  MyProc->lwWaiting = true;
  MyProc->lwWaitMode = mode;

                                                                       
  if (mode == LW_WAIT_UNTIL_FREE)
  {
    proclist_push_head(&lock->waiters, MyProc->pgprocno, lwWaitLink);
  }
  else
  {
    proclist_push_tail(&lock->waiters, MyProc->pgprocno, lwWaitLink);
  }

                                 
  LWLockWaitListUnlock(lock);

#ifdef LOCK_DEBUG
  pg_atomic_fetch_add_u32(&lock->nwaiters, 1);
#endif
}

   
                                       
   
                                                                             
                                                                             
          
   
static void
LWLockDequeueSelf(LWLock *lock)
{
  bool found = false;
  proclist_mutable_iter iter;

#ifdef LWLOCK_STATS
  lwlock_stats *lwstats;

  lwstats = get_lwlock_stats_entry(lock);

  lwstats->dequeue_self_count++;
#endif

  LWLockWaitListLock(lock);

     
                                                                            
                                                          
     
  proclist_foreach_modify(iter, &lock->waiters, lwWaitLink)
  {
    if (iter.cur == MyProc->pgprocno)
    {
      found = true;
      proclist_delete(&lock->waiters, iter.cur, lwWaitLink);
      break;
    }
  }

  if (proclist_is_empty(&lock->waiters) && (pg_atomic_read_u32(&lock->state) & LW_FLAG_HAS_WAITERS) != 0)
  {
    pg_atomic_fetch_and_u32(&lock->state, ~LW_FLAG_HAS_WAITERS);
  }

                                          
  LWLockWaitListUnlock(lock);

                                                     
  if (found)
  {
    MyProc->lwWaiting = false;
  }
  else
  {
    int extraWaits = 0;

       
                                                                           
                                           
       

       
                                                                         
                                     
       
    pg_atomic_fetch_or_u32(&lock->state, LW_FLAG_RELEASE_OK);

       
                                                                          
                                                                         
                                
       
    for (;;)
    {
      PGSemaphoreLock(MyProc->sem);
      if (!MyProc->lwWaiting)
      {
        break;
      }
      extraWaits++;
    }

       
                                                                        
       
    while (extraWaits-- > 0)
    {
      PGSemaphoreUnlock(MyProc->sem);
    }
  }

#ifdef LOCK_DEBUG
  {
                             
    uint32 nwaiters PG_USED_FOR_ASSERTS_ONLY = pg_atomic_fetch_sub_u32(&lock->nwaiters, 1);

    Assert(nwaiters < MAX_BACKENDS);
  }
#endif
}

   
                                                                    
   
                                                                              
                                                        
   
                                                                       
   
bool
LWLockAcquire(LWLock *lock, LWLockMode mode)
{
  PGPROC *proc = MyProc;
  bool result = true;
  int extraWaits = 0;
#ifdef LWLOCK_STATS
  lwlock_stats *lwstats;

  lwstats = get_lwlock_stats_entry(lock);
#endif

  AssertArg(mode == LW_SHARED || mode == LW_EXCLUSIVE);

  PRINT_LWDEBUG("LWLockAcquire", lock, mode);

#ifdef LWLOCK_STATS
                                       
  if (mode == LW_EXCLUSIVE)
  {
    lwstats->ex_acquire_count++;
  }
  else
  {
    lwstats->sh_acquire_count++;
  }
#endif                   

     
                                                                       
                                                                           
                                       
     
  Assert(!(proc == NULL && IsUnderPostmaster));

                                                     
  if (num_held_lwlocks >= MAX_SIMUL_LWLOCKS)
  {
    elog(ERROR, "too many LWLocks taken");
  }

     
                                                                             
                                                                          
                                                        
     
  HOLD_INTERRUPTS();

     
                                                                         
                    
     
                                                                            
                                                                             
                                                                           
                                                                             
                                                                     
                                                                        
                                                                           
                                                                             
                                                                        
                                                                            
                                                        
     
  for (;;)
  {
    bool mustwait;

       
                                                                       
                    
       
    mustwait = LWLockAttemptLock(lock, mode);

    if (!mustwait)
    {
      LOG_LWDEBUG("LWLockAcquire", lock, "immediately acquired lock");
      break;                   
    }

       
                                                                        
                                                                           
                                                                       
                                                                         
                                                                        
                                                                        
                                                                         
                                               
       

                          
    LWLockQueueSelf(lock, mode);

                                                          
    mustwait = LWLockAttemptLock(lock, mode);

                                                                           
    if (!mustwait)
    {
      LOG_LWDEBUG("LWLockAcquire", lock, "acquired, undoing queue");

      LWLockDequeueSelf(lock);
      break;
    }

       
                            
       
                                                                         
                                                                          
                                                                       
                                    
       
    LOG_LWDEBUG("LWLockAcquire", lock, "waiting");

#ifdef LWLOCK_STATS
    lwstats->block_count++;
#endif

    LWLockReportWaitStart(lock);
    TRACE_POSTGRESQL_LWLOCK_WAIT_START(T_NAME(lock), mode);

    for (;;)
    {
      PGSemaphoreLock(proc->sem);
      if (!proc->lwWaiting)
      {
        break;
      }
      extraWaits++;
    }

                                                                 
    pg_atomic_fetch_or_u32(&lock->state, LW_FLAG_RELEASE_OK);

#ifdef LOCK_DEBUG
    {
                               
      uint32 nwaiters PG_USED_FOR_ASSERTS_ONLY = pg_atomic_fetch_sub_u32(&lock->nwaiters, 1);

      Assert(nwaiters < MAX_BACKENDS);
    }
#endif

    TRACE_POSTGRESQL_LWLOCK_WAIT_DONE(T_NAME(lock), mode);
    LWLockReportWaitEnd();

    LOG_LWDEBUG("LWLockAcquire", lock, "awakened");

                                                      
    result = false;
  }

  TRACE_POSTGRESQL_LWLOCK_ACQUIRE(T_NAME(lock), mode);

                                                      
  held_lwlocks[num_held_lwlocks].lock = lock;
  held_lwlocks[num_held_lwlocks++].mode = mode;

     
                                                                      
     
  while (extraWaits-- > 0)
  {
    PGSemaphoreUnlock(proc->sem);
  }

  return result;
}

   
                                                                               
   
                                                                    
   
                                                                         
   
bool
LWLockConditionalAcquire(LWLock *lock, LWLockMode mode)
{
  bool mustwait;

  AssertArg(mode == LW_SHARED || mode == LW_EXCLUSIVE);

  PRINT_LWDEBUG("LWLockConditionalAcquire", lock, mode);

                                                     
  if (num_held_lwlocks >= MAX_SIMUL_LWLOCKS)
  {
    elog(ERROR, "too many LWLocks taken");
  }

     
                                                                             
                                                                          
                                                        
     
  HOLD_INTERRUPTS();

                          
  mustwait = LWLockAttemptLock(lock, mode);

  if (mustwait)
  {
                                                          
    RESUME_INTERRUPTS();

    LOG_LWDEBUG("LWLockConditionalAcquire", lock, "failed");
    TRACE_POSTGRESQL_LWLOCK_CONDACQUIRE_FAIL(T_NAME(lock), mode);
  }
  else
  {
                                                        
    held_lwlocks[num_held_lwlocks].lock = lock;
    held_lwlocks[num_held_lwlocks++].mode = mode;
    TRACE_POSTGRESQL_LWLOCK_CONDACQUIRE(T_NAME(lock), mode);
  }
  return !mustwait;
}

   
                                                               
   
                                                                             
                                                                              
                                                                            
                                                     
   
                                                                            
                                                                       
                                                                           
                                                                              
                                                                              
   
bool
LWLockAcquireOrWait(LWLock *lock, LWLockMode mode)
{
  PGPROC *proc = MyProc;
  bool mustwait;
  int extraWaits = 0;
#ifdef LWLOCK_STATS
  lwlock_stats *lwstats;

  lwstats = get_lwlock_stats_entry(lock);
#endif

  Assert(mode == LW_SHARED || mode == LW_EXCLUSIVE);

  PRINT_LWDEBUG("LWLockAcquireOrWait", lock, mode);

                                                     
  if (num_held_lwlocks >= MAX_SIMUL_LWLOCKS)
  {
    elog(ERROR, "too many LWLocks taken");
  }

     
                                                                             
                                                                          
                                                        
     
  HOLD_INTERRUPTS();

     
                                                                     
                                                                  
     
  mustwait = LWLockAttemptLock(lock, mode);

  if (mustwait)
  {
    LWLockQueueSelf(lock, LW_WAIT_UNTIL_FREE);

    mustwait = LWLockAttemptLock(lock, mode);

    if (mustwait)
    {
         
                                                                      
                        
         
      LOG_LWDEBUG("LWLockAcquireOrWait", lock, "waiting");

#ifdef LWLOCK_STATS
      lwstats->block_count++;
#endif

      LWLockReportWaitStart(lock);
      TRACE_POSTGRESQL_LWLOCK_WAIT_START(T_NAME(lock), mode);

      for (;;)
      {
        PGSemaphoreLock(proc->sem);
        if (!proc->lwWaiting)
        {
          break;
        }
        extraWaits++;
      }

#ifdef LOCK_DEBUG
      {
                                 
        uint32 nwaiters PG_USED_FOR_ASSERTS_ONLY = pg_atomic_fetch_sub_u32(&lock->nwaiters, 1);

        Assert(nwaiters < MAX_BACKENDS);
      }
#endif
      TRACE_POSTGRESQL_LWLOCK_WAIT_DONE(T_NAME(lock), mode);
      LWLockReportWaitEnd();

      LOG_LWDEBUG("LWLockAcquireOrWait", lock, "awakened");
    }
    else
    {
      LOG_LWDEBUG("LWLockAcquireOrWait", lock, "acquired, undoing queue");

         
                                                                         
                                                                       
                                                                       
                   
         
      LWLockDequeueSelf(lock);
    }
  }

     
                                                                      
     
  while (extraWaits-- > 0)
  {
    PGSemaphoreUnlock(proc->sem);
  }

  if (mustwait)
  {
                                                          
    RESUME_INTERRUPTS();
    LOG_LWDEBUG("LWLockAcquireOrWait", lock, "failed");
    TRACE_POSTGRESQL_LWLOCK_ACQUIRE_OR_WAIT_FAIL(T_NAME(lock), mode);
  }
  else
  {
    LOG_LWDEBUG("LWLockAcquireOrWait", lock, "succeeded");
                                                        
    held_lwlocks[num_held_lwlocks].lock = lock;
    held_lwlocks[num_held_lwlocks++].mode = mode;
    TRACE_POSTGRESQL_LWLOCK_ACQUIRE_OR_WAIT(T_NAME(lock), mode);
  }

  return !mustwait;
}

   
                                                                               
           
   
                                                                            
                                               
   
                                                                     
   
static bool
LWLockConflictsWithVar(LWLock *lock, uint64 *valptr, uint64 oldval, uint64 *newval, bool *result)
{
  bool mustwait;
  uint64 value;

     
                                                         
     
                                                                            
                                                                            
                             
     
  mustwait = (pg_atomic_read_u32(&lock->state) & LW_VAL_EXCLUSIVE) != 0;

  if (!mustwait)
  {
    *result = true;
    return false;
  }

  *result = false;

     
                                                                         
                                                                           
                                                                          
     
  LWLockWaitListLock(lock);
  value = *valptr;
  LWLockWaitListUnlock(lock);

  if (value != oldval)
  {
    mustwait = false;
    *newval = value;
  }
  else
  {
    mustwait = true;
  }

  return mustwait;
}

   
                                                                         
   
                                                                          
                                                               
                                                                       
                                                                             
                                                                          
            
   
                                                                        
                                   
   
bool
LWLockWaitForVar(LWLock *lock, uint64 *valptr, uint64 oldval, uint64 *newval)
{
  PGPROC *proc = MyProc;
  int extraWaits = 0;
  bool result = false;
#ifdef LWLOCK_STATS
  lwlock_stats *lwstats;

  lwstats = get_lwlock_stats_entry(lock);
#endif

  PRINT_LWDEBUG("LWLockWaitForVar", lock, LW_WAIT_UNTIL_FREE);

     
                                                                             
                                                                  
                  
     
  HOLD_INTERRUPTS();

     
                                                                           
     
  for (;;)
  {
    bool mustwait;

    mustwait = LWLockConflictsWithVar(lock, valptr, oldval, newval, &result);

    if (!mustwait)
    {
      break;                                              
    }

       
                                                                       
                                                                          
                                                            
                                                                 
                                                                           
                                       
       
    LWLockQueueSelf(lock, LW_WAIT_UNTIL_FREE);

       
                                                                        
                         
       
    pg_atomic_fetch_or_u32(&lock->state, LW_FLAG_RELEASE_OK);

       
                                                                          
                            
       
    mustwait = LWLockConflictsWithVar(lock, valptr, oldval, newval, &result);

                                                                   
    if (!mustwait)
    {
      LOG_LWDEBUG("LWLockWaitForVar", lock, "free, undoing queue");

      LWLockDequeueSelf(lock);
      break;
    }

       
                            
       
                                                                         
                                                                          
                                                                       
                                    
       
    LOG_LWDEBUG("LWLockWaitForVar", lock, "waiting");

#ifdef LWLOCK_STATS
    lwstats->block_count++;
#endif

    LWLockReportWaitStart(lock);
    TRACE_POSTGRESQL_LWLOCK_WAIT_START(T_NAME(lock), LW_EXCLUSIVE);

    for (;;)
    {
      PGSemaphoreLock(proc->sem);
      if (!proc->lwWaiting)
      {
        break;
      }
      extraWaits++;
    }

#ifdef LOCK_DEBUG
    {
                               
      uint32 nwaiters PG_USED_FOR_ASSERTS_ONLY = pg_atomic_fetch_sub_u32(&lock->nwaiters, 1);

      Assert(nwaiters < MAX_BACKENDS);
    }
#endif

    TRACE_POSTGRESQL_LWLOCK_WAIT_DONE(T_NAME(lock), LW_EXCLUSIVE);
    LWLockReportWaitEnd();

    LOG_LWDEBUG("LWLockWaitForVar", lock, "awakened");

                                                               
  }

  TRACE_POSTGRESQL_LWLOCK_ACQUIRE(T_NAME(lock), LW_EXCLUSIVE);

     
                                                                      
     
  while (extraWaits-- > 0)
  {
    PGSemaphoreUnlock(proc->sem);
  }

     
                                              
     
  RESUME_INTERRUPTS();

  return result;
}

   
                                                                      
   
                                                                         
                                                                             
                                                                              
                                                            
   
                                                          
   
void
LWLockUpdateVar(LWLock *lock, uint64 *valptr, uint64 val)
{
  proclist_head wakeup;
  proclist_mutable_iter iter;

  PRINT_LWDEBUG("LWLockUpdateVar", lock, LW_EXCLUSIVE);

  proclist_init(&wakeup);

  LWLockWaitListLock(lock);

  Assert(pg_atomic_read_u32(&lock->state) & LW_VAL_EXCLUSIVE);

                               
  *valptr = val;

     
                                                                           
                                                    
     
  proclist_foreach_modify(iter, &lock->waiters, lwWaitLink)
  {
    PGPROC *waiter = GetPGProcByNumber(iter.cur);

    if (waiter->lwWaitMode != LW_WAIT_UNTIL_FREE)
    {
      break;
    }

    proclist_delete(&lock->waiters, iter.cur, lwWaitLink);
    proclist_push_tail(&wakeup, iter.cur, lwWaitLink);
  }

                                                             
  LWLockWaitListUnlock(lock);

     
                                                  
     
  proclist_foreach_modify(iter, &wakeup, lwWaitLink)
  {
    PGPROC *waiter = GetPGProcByNumber(iter.cur);

    proclist_delete(&wakeup, iter.cur, lwWaitLink);
                                                            
    pg_write_barrier();
    waiter->lwWaiting = false;
    PGSemaphoreUnlock(waiter->sem);
  }
}

   
                                                      
   
void
LWLockRelease(LWLock *lock)
{
  LWLockMode mode;
  uint32 oldstate;
  bool check_waiters;
  int i;

     
                                                                            
                                                             
     
  for (i = num_held_lwlocks; --i >= 0;)
  {
    if (lock == held_lwlocks[i].lock)
    {
      break;
    }
  }

  if (i < 0)
  {
    elog(ERROR, "lock %s is not held", T_NAME(lock));
  }

  mode = held_lwlocks[i].mode;

  num_held_lwlocks--;
  for (; i < num_held_lwlocks; i++)
  {
    held_lwlocks[i] = held_lwlocks[i + 1];
  }

  PRINT_LWDEBUG("LWLockRelease", lock, mode);

     
                                                                           
                                                            
     
  if (mode == LW_EXCLUSIVE)
  {
    oldstate = pg_atomic_sub_fetch_u32(&lock->state, LW_VAL_EXCLUSIVE);
  }
  else
  {
    oldstate = pg_atomic_sub_fetch_u32(&lock->state, LW_VAL_SHARED);
  }

                                              
  Assert(!(oldstate & LW_VAL_EXCLUSIVE));

     
                                                                           
            
     
  if ((oldstate & (LW_FLAG_HAS_WAITERS | LW_FLAG_RELEASE_OK)) == (LW_FLAG_HAS_WAITERS | LW_FLAG_RELEASE_OK) && (oldstate & LW_LOCK_MASK) == 0)
  {
    check_waiters = true;
  }
  else
  {
    check_waiters = false;
  }

     
                                                                           
                   
     
  if (check_waiters)
  {
                                    
    LOG_LWDEBUG("LWLockRelease", lock, "releasing waiters");
    LWLockWakeup(lock);
  }

  TRACE_POSTGRESQL_LWLOCK_RELEASE(T_NAME(lock));

     
                                              
     
  RESUME_INTERRUPTS();
}

   
                                                                              
   
void
LWLockReleaseClearVar(LWLock *lock, uint64 *valptr, uint64 val)
{
  LWLockWaitListLock(lock);

     
                                                                            
                                                                             
                              
     
  *valptr = val;
  LWLockWaitListUnlock(lock);

  LWLockRelease(lock);
}

   
                                                       
   
                                                                               
                                                                            
                                                                               
                                                                            
                                                                          
   
void
LWLockReleaseAll(void)
{
  while (num_held_lwlocks > 0)
  {
    HOLD_INTERRUPTS();                                           

    LWLockRelease(held_lwlocks[num_held_lwlocks - 1].lock);
  }
}

   
                                                                     
   
                                        
   
bool
LWLockHeldByMe(LWLock *l)
{
  int i;

  for (i = 0; i < num_held_lwlocks; i++)
  {
    if (held_lwlocks[i].lock == l)
    {
      return true;
    }
  }
  return false;
}

   
                                                                           
   
                                        
   
bool
LWLockAnyHeldByMe(LWLock *l, int nlocks, size_t stride)
{
  char *held_lock_addr;
  char *begin;
  char *end;
  int i;

  begin = (char *)l;
  end = begin + nlocks * stride;
  for (i = 0; i < num_held_lwlocks; i++)
  {
    held_lock_addr = (char *)held_lwlocks[i].lock;
    if (held_lock_addr >= begin && held_lock_addr < end && (held_lock_addr - begin) % stride == 0)
    {
      return true;
    }
  }
  return false;
}

   
                                                                             
   
                                        
   
bool
LWLockHeldByMeInMode(LWLock *l, LWLockMode mode)
{
  int i;

  for (i = 0; i < num_held_lwlocks; i++)
  {
    if (held_lwlocks[i].lock == l && held_lwlocks[i].mode == mode)
    {
      return true;
    }
  }
  return false;
}
