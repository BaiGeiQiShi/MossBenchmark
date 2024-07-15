                                                                            
   
               
                                                                   
   
                                                                
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/predicate_internals.h"
#include "utils/array.h"
#include "utils/builtins.h"

                                       
const char *const LockTagTypeNames[] = {"relation", "extend", "page", "tuple", "transactionid", "virtualxid", "speculative token", "object", "userlock", "advisory", "frozenid"};

                                                                          
static const char *const PredicateLockTagTypeNames[] = {"relation", "page", "tuple"};

                                       
typedef struct
{
  LockData *lockData;                                        
  int currIdx;                                                 
  PredicateLockData *predLockData;                                
  int predLockIdx;                                                  
} PG_Lock_Status;

                                          
#define NUM_LOCK_STATUS_COLUMNS 15

   
                                                            
   
                                                                     
   
static Datum
VXIDGetDatum(BackendId bid, LocalTransactionId lxid)
{
     
                                                                        
                                                                      
     
  char vxidstr[32];

  snprintf(vxidstr, sizeof(vxidstr), "%d/%u", bid, lxid);

  return CStringGetTextDatum(vxidstr);
}

   
                                                                              
   
Datum
pg_lock_status(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  PG_Lock_Status *mystatus;
  LockData *lockData;
  PredicateLockData *predLockData;

  if (SRF_IS_FIRSTCALL())
  {
    TupleDesc tupdesc;
    MemoryContext oldcontext;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                        
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                         
                                                                   
    tupdesc = CreateTemplateTupleDesc(NUM_LOCK_STATUS_COLUMNS);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "locktype", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "database", OIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)3, "relation", OIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)4, "page", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)5, "tuple", INT2OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)6, "virtualxid", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)7, "transactionid", XIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)8, "classid", OIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)9, "objid", OIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)10, "objsubid", INT2OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)11, "virtualtransaction", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)12, "pid", INT4OID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)13, "mode", TEXTOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)14, "granted", BOOLOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)15, "fastpath", BOOLOID, -1, 0);

    funcctx->tuple_desc = BlessTupleDesc(tupdesc);

       
                                                                        
                            
       
    mystatus = (PG_Lock_Status *)palloc(sizeof(PG_Lock_Status));
    funcctx->user_fctx = (void *)mystatus;

    mystatus->lockData = GetLockStatusData();
    mystatus->currIdx = 0;
    mystatus->predLockData = GetPredicateLockStatusData();
    mystatus->predLockIdx = 0;

    MemoryContextSwitchTo(oldcontext);
  }

  funcctx = SRF_PERCALL_SETUP();
  mystatus = (PG_Lock_Status *)funcctx->user_fctx;
  lockData = mystatus->lockData;

  while (mystatus->currIdx < lockData->nelements)
  {
    bool granted;
    LOCKMODE mode = 0;
    const char *locktypename;
    char tnbuf[32];
    Datum values[NUM_LOCK_STATUS_COLUMNS];
    bool nulls[NUM_LOCK_STATUS_COLUMNS];
    HeapTuple tuple;
    Datum result;
    LockInstanceData *instance;

    instance = &(lockData->locks[mystatus->currIdx]);

       
                                                                         
                                                                        
              
       
    granted = false;
    if (instance->holdMask)
    {
      for (mode = 0; mode < MAX_LOCKMODES; mode++)
      {
        if (instance->holdMask & LOCKBIT_ON(mode))
        {
          granted = true;
          instance->holdMask &= LOCKBIT_OFF(mode);
          break;
        }
      }
    }

       
                                                                       
                          
       
    if (!granted)
    {
      if (instance->waitLockMode != NoLock)
      {
                                                
        mode = instance->waitLockMode;

           
                                                                     
                                                
           
        mystatus->currIdx++;
      }
      else
      {
           
                                                                    
                                              
           
        mystatus->currIdx++;
        continue;
      }
    }

       
                                         
       
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));

    if (instance->locktag.locktag_type <= LOCKTAG_LAST_TYPE)
    {
      locktypename = LockTagTypeNames[instance->locktag.locktag_type];
    }
    else
    {
      snprintf(tnbuf, sizeof(tnbuf), "unknown %d", (int)instance->locktag.locktag_type);
      locktypename = tnbuf;
    }
    values[0] = CStringGetTextDatum(locktypename);

    switch ((LockTagType)instance->locktag.locktag_type)
    {
    case LOCKTAG_RELATION:
    case LOCKTAG_RELATION_EXTEND:
      values[1] = ObjectIdGetDatum(instance->locktag.locktag_field1);
      values[2] = ObjectIdGetDatum(instance->locktag.locktag_field2);
      nulls[3] = true;
      nulls[4] = true;
      nulls[5] = true;
      nulls[6] = true;
      nulls[7] = true;
      nulls[8] = true;
      nulls[9] = true;
      break;
    case LOCKTAG_DATABASE_FROZEN_IDS:
      values[1] = ObjectIdGetDatum(instance->locktag.locktag_field1);
      nulls[2] = true;
      nulls[3] = true;
      nulls[4] = true;
      nulls[5] = true;
      nulls[6] = true;
      nulls[7] = true;
      nulls[8] = true;
      nulls[9] = true;
      break;
    case LOCKTAG_PAGE:
      values[1] = ObjectIdGetDatum(instance->locktag.locktag_field1);
      values[2] = ObjectIdGetDatum(instance->locktag.locktag_field2);
      values[3] = UInt32GetDatum(instance->locktag.locktag_field3);
      nulls[4] = true;
      nulls[5] = true;
      nulls[6] = true;
      nulls[7] = true;
      nulls[8] = true;
      nulls[9] = true;
      break;
    case LOCKTAG_TUPLE:
      values[1] = ObjectIdGetDatum(instance->locktag.locktag_field1);
      values[2] = ObjectIdGetDatum(instance->locktag.locktag_field2);
      values[3] = UInt32GetDatum(instance->locktag.locktag_field3);
      values[4] = UInt16GetDatum(instance->locktag.locktag_field4);
      nulls[5] = true;
      nulls[6] = true;
      nulls[7] = true;
      nulls[8] = true;
      nulls[9] = true;
      break;
    case LOCKTAG_TRANSACTION:
      values[6] = TransactionIdGetDatum(instance->locktag.locktag_field1);
      nulls[1] = true;
      nulls[2] = true;
      nulls[3] = true;
      nulls[4] = true;
      nulls[5] = true;
      nulls[7] = true;
      nulls[8] = true;
      nulls[9] = true;
      break;
    case LOCKTAG_VIRTUALTRANSACTION:
      values[5] = VXIDGetDatum(instance->locktag.locktag_field1, instance->locktag.locktag_field2);
      nulls[1] = true;
      nulls[2] = true;
      nulls[3] = true;
      nulls[4] = true;
      nulls[6] = true;
      nulls[7] = true;
      nulls[8] = true;
      nulls[9] = true;
      break;
    case LOCKTAG_OBJECT:
    case LOCKTAG_USERLOCK:
    case LOCKTAG_ADVISORY:
    default:                                         
      values[1] = ObjectIdGetDatum(instance->locktag.locktag_field1);
      values[7] = ObjectIdGetDatum(instance->locktag.locktag_field2);
      values[8] = ObjectIdGetDatum(instance->locktag.locktag_field3);
      values[9] = Int16GetDatum(instance->locktag.locktag_field4);
      nulls[2] = true;
      nulls[3] = true;
      nulls[4] = true;
      nulls[5] = true;
      nulls[6] = true;
      break;
    }

    values[10] = VXIDGetDatum(instance->backend, instance->lxid);
    if (instance->pid != 0)
    {
      values[11] = Int32GetDatum(instance->pid);
    }
    else
    {
      nulls[11] = true;
    }
    values[12] = CStringGetTextDatum(GetLockmodeName(instance->locktag.locktag_lockmethodid, mode));
    values[13] = BoolGetDatum(granted);
    values[14] = BoolGetDatum(instance->fastpath);

    tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    result = HeapTupleGetDatum(tuple);
    SRF_RETURN_NEXT(funcctx, result);
  }

     
                                                                        
            
     
  predLockData = mystatus->predLockData;
  if (mystatus->predLockIdx < predLockData->nelements)
  {
    PredicateLockTargetType lockType;

    PREDICATELOCKTARGETTAG *predTag = &(predLockData->locktags[mystatus->predLockIdx]);
    SERIALIZABLEXACT *xact = &(predLockData->xacts[mystatus->predLockIdx]);
    Datum values[NUM_LOCK_STATUS_COLUMNS];
    bool nulls[NUM_LOCK_STATUS_COLUMNS];
    HeapTuple tuple;
    Datum result;

    mystatus->predLockIdx++;

       
                                         
       
    MemSet(values, 0, sizeof(values));
    MemSet(nulls, false, sizeof(nulls));

                   
    lockType = GET_PREDICATELOCKTARGETTAG_TYPE(*predTag);

    values[0] = CStringGetTextDatum(PredicateLockTagTypeNames[lockType]);

                     
    values[1] = GET_PREDICATELOCKTARGETTAG_DB(*predTag);
    values[2] = GET_PREDICATELOCKTARGETTAG_RELATION(*predTag);
    if (lockType == PREDLOCKTAG_TUPLE)
    {
      values[4] = GET_PREDICATELOCKTARGETTAG_OFFSET(*predTag);
    }
    else
    {
      nulls[4] = true;
    }
    if ((lockType == PREDLOCKTAG_TUPLE) || (lockType == PREDLOCKTAG_PAGE))
    {
      values[3] = GET_PREDICATELOCKTARGETTAG_PAGE(*predTag);
    }
    else
    {
      nulls[3] = true;
    }

                                                           
    nulls[5] = true;                 
    nulls[6] = true;                    
    nulls[7] = true;              
    nulls[8] = true;            
    nulls[9] = true;               

                     
    values[10] = VXIDGetDatum(xact->vxid.backendId, xact->vxid.localTransactionId);
    if (xact->pid != 0)
    {
      values[11] = Int32GetDatum(xact->pid);
    }
    else
    {
      nulls[11] = true;
    }

       
                                                                           
                                                         
       
    values[12] = CStringGetTextDatum("SIReadLock");
    values[13] = BoolGetDatum(true);
    values[14] = BoolGetDatum(false);

    tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
    result = HeapTupleGetDatum(tuple);
    SRF_RETURN_NEXT(funcctx, result);
  }

  SRF_RETURN_DONE(funcctx);
}

   
                                                                      
   
                                                                               
                                                                             
                                                         
   
                                                                          
                                                                           
                                                                           
                                                                            
   
                                                                          
                                                                       
                                                                            
                                                 
   
                                                                                
   
Datum
pg_blocking_pids(PG_FUNCTION_ARGS)
{
  int blocked_pid = PG_GETARG_INT32(0);
  Datum *arrayelems;
  int narrayelems;
  BlockedProcsData *lockData;                           
  int i, j;

                                                
  lockData = GetBlockerStatusData(blocked_pid);

                                                                           
  arrayelems = (Datum *)palloc(lockData->nlocks * sizeof(Datum));
  narrayelems = 0;

                                                   
  for (i = 0; i < lockData->nprocs; i++)
  {
    BlockedProcData *bproc = &lockData->procs[i];
    LockInstanceData *instances = &lockData->locks[bproc->first_lock];
    int *preceding_waiters = &lockData->waiter_pids[bproc->first_waiter];
    LockInstanceData *blocked_instance;
    LockMethod lockMethodTable;
    int conflictMask;

       
                                                                          
                                                   
       
    blocked_instance = NULL;
    for (j = 0; j < bproc->num_locks; j++)
    {
      LockInstanceData *instance = &(instances[j]);

      if (instance->pid == bproc->pid)
      {
        Assert(blocked_instance == NULL);
        blocked_instance = instance;
      }
    }
    Assert(blocked_instance != NULL);

    lockMethodTable = GetLockTagsMethodTable(&(blocked_instance->locktag));
    conflictMask = lockMethodTable->conflictTab[blocked_instance->waitLockMode];

                                                          
    for (j = 0; j < bproc->num_locks; j++)
    {
      LockInstanceData *instance = &(instances[j]);

                                                            
      if (instance == blocked_instance)
      {
        continue;
      }
                                                                     
      if (instance->leaderPid == blocked_instance->leaderPid)
      {
        continue;
      }

      if (conflictMask & instance->holdMask)
      {
                                                                    
      }
      else if (instance->waitLockMode != NoLock && (conflictMask & LOCKBIT_ON(instance->waitLockMode)))
      {
                                                                      
        bool ahead = false;
        int k;

        for (k = 0; k < bproc->num_waiters; k++)
        {
          if (preceding_waiters[k] == instance->pid)
          {
                                                                 
            ahead = true;
            break;
          }
        }
        if (!ahead)
        {
          continue;                                
        }
      }
      else
      {
                                       
        continue;
      }

                                                   
      arrayelems[narrayelems++] = Int32GetDatum(instance->leaderPid);
    }
  }

                                             
  Assert(narrayelems <= lockData->nlocks);

                                                                  
  PG_RETURN_ARRAYTYPE_P(construct_array(arrayelems, narrayelems, INT4OID, sizeof(int32), true, 'i'));
}

   
                                                                          
                                          
   
                                                                        
                               
   
Datum
pg_safe_snapshot_blocking_pids(PG_FUNCTION_ARGS)
{
  int blocked_pid = PG_GETARG_INT32(0);
  int *blockers;
  int num_blockers;
  Datum *blocker_datums;

                                                                            
  blockers = (int *)palloc(MaxBackends * sizeof(int));

                                                                     
  num_blockers = GetSafeSnapshotBlockingPids(blocked_pid, blockers, MaxBackends);

                                        
  if (num_blockers > 0)
  {
    int i;

    blocker_datums = (Datum *)palloc(num_blockers * sizeof(Datum));
    for (i = 0; i < num_blockers; ++i)
    {
      blocker_datums[i] = Int32GetDatum(blockers[i]);
    }
  }
  else
  {
    blocker_datums = NULL;
  }

                                                                  
  PG_RETURN_ARRAYTYPE_P(construct_array(blocker_datums, num_blockers, INT4OID, sizeof(int32), true, 'i'));
}

   
                                                                               
   
                                                                             
                                                                       
                                                                           
                                                                    
   
                                                                              
                                                                       
   
Datum
pg_isolation_test_session_is_blocked(PG_FUNCTION_ARGS)
{
  int blocked_pid = PG_GETARG_INT32(0);
  ArrayType *interesting_pids_a = PG_GETARG_ARRAYTYPE_P(1);
  ArrayType *blocking_pids_a;
  int32 *interesting_pids;
  int32 *blocking_pids;
  int num_interesting_pids;
  int num_blocking_pids;
  int dummy;
  int i, j;

                                    
  Assert(ARR_ELEMTYPE(interesting_pids_a) == INT4OID);
  if (array_contains_nulls(interesting_pids_a))
  {
    elog(ERROR, "array must not contain nulls");
  }
  interesting_pids = (int32 *)ARR_DATA_PTR(interesting_pids_a);
  num_interesting_pids = ArrayGetNItems(ARR_NDIM(interesting_pids_a), ARR_DIMS(interesting_pids_a));

     
                                                                          
                                
     
  blocking_pids_a = DatumGetArrayTypeP(DirectFunctionCall1(pg_blocking_pids, blocked_pid));

  Assert(ARR_ELEMTYPE(blocking_pids_a) == INT4OID);
  Assert(!array_contains_nulls(blocking_pids_a));
  blocking_pids = (int32 *)ARR_DATA_PTR(blocking_pids_a);
  num_blocking_pids = ArrayGetNItems(ARR_NDIM(blocking_pids_a), ARR_DIMS(blocking_pids_a));

     
                                                                           
                                                                      
                                                                             
                                                                        
                                                                            
                                                                           
                  
     
  for (i = 0; i < num_blocking_pids; i++)
  {
    for (j = 0; j < num_interesting_pids; j++)
    {
      if (blocking_pids[i] == interesting_pids[j])
      {
        PG_RETURN_BOOL(true);
      }
    }
  }

     
                                                                       
                                                                  
                                                                            
                                                                             
                                                                         
                                                                            
                                                                           
     
  if (GetSafeSnapshotBlockingPids(blocked_pid, &dummy, 1) > 0)
  {
    PG_RETURN_BOOL(true);
  }

  PG_RETURN_BOOL(false);
}

   
                                             
   
                                                 
   
                                                                     
                                                                   
                                                                   
                                                          
   
#define SET_LOCKTAG_INT64(tag, key64) SET_LOCKTAG_ADVISORY(tag, MyDatabaseId, (uint32)((key64) >> 32), (uint32)(key64), 1)
#define SET_LOCKTAG_INT32(tag, key1, key2) SET_LOCKTAG_ADVISORY(tag, MyDatabaseId, key1, key2, 2)

static void
PreventAdvisoryLocksInParallelMode(void)
{
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot use advisory locks during a parallel operation")));
  }
}

   
                                                                  
   
Datum
pg_advisory_lock_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  (void)LockAcquire(&tag, ExclusiveLock, true, false);

  PG_RETURN_VOID();
}

   
                                                     
                                 
   
Datum
pg_advisory_xact_lock_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  (void)LockAcquire(&tag, ExclusiveLock, false, false);

  PG_RETURN_VOID();
}

   
                                                                     
   
Datum
pg_advisory_lock_shared_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  (void)LockAcquire(&tag, ShareLock, true, false);

  PG_RETURN_VOID();
}

   
                                                            
                             
   
Datum
pg_advisory_xact_lock_shared_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  (void)LockAcquire(&tag, ShareLock, false, false);

  PG_RETURN_VOID();
}

   
                                                                               
   
                                                           
   
Datum
pg_try_advisory_lock_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  res = LockAcquire(&tag, ExclusiveLock, true, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                         
                                          
   
                                                           
   
Datum
pg_try_advisory_xact_lock_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  res = LockAcquire(&tag, ExclusiveLock, false, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                                                  
   
                                                           
   
Datum
pg_try_advisory_lock_shared_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  res = LockAcquire(&tag, ShareLock, true, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                                
                                      
   
                                                           
   
Datum
pg_try_advisory_xact_lock_shared_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  res = LockAcquire(&tag, ShareLock, false, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                                    
   
                                                          
   
Datum
pg_advisory_unlock_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;
  bool res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  res = LockRelease(&tag, ExclusiveLock, true);

  PG_RETURN_BOOL(res);
}

   
                                                                       
   
                                                          
   
Datum
pg_advisory_unlock_shared_int8(PG_FUNCTION_ARGS)
{
  int64 key = PG_GETARG_INT64(0);
  LOCKTAG tag;
  bool res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT64(tag, key);

  res = LockRelease(&tag, ShareLock, true);

  PG_RETURN_BOOL(res);
}

   
                                                                        
   
Datum
pg_advisory_lock_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  (void)LockAcquire(&tag, ExclusiveLock, true, false);

  PG_RETURN_VOID();
}

   
                                                           
                                 
   
Datum
pg_advisory_xact_lock_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  (void)LockAcquire(&tag, ExclusiveLock, false, false);

  PG_RETURN_VOID();
}

   
                                                                           
   
Datum
pg_advisory_lock_shared_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  (void)LockAcquire(&tag, ShareLock, true, false);

  PG_RETURN_VOID();
}

   
                                                                  
                             
   
Datum
pg_advisory_xact_lock_shared_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  (void)LockAcquire(&tag, ShareLock, false, false);

  PG_RETURN_VOID();
}

   
                                                                                     
   
                                                           
   
Datum
pg_try_advisory_lock_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  res = LockAcquire(&tag, ExclusiveLock, true, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                               
                                          
   
                                                           
   
Datum
pg_try_advisory_xact_lock_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  res = LockAcquire(&tag, ExclusiveLock, false, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                                                        
   
                                                           
   
Datum
pg_try_advisory_lock_shared_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  res = LockAcquire(&tag, ShareLock, true, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                                      
                                      
   
                                                           
   
Datum
pg_try_advisory_xact_lock_shared_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;
  LockAcquireResult res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  res = LockAcquire(&tag, ShareLock, false, true);

  PG_RETURN_BOOL(res != LOCKACQUIRE_NOT_AVAIL);
}

   
                                                                          
   
                                                          
   
Datum
pg_advisory_unlock_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;
  bool res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  res = LockRelease(&tag, ExclusiveLock, true);

  PG_RETURN_BOOL(res);
}

   
                                                                             
   
                                                          
   
Datum
pg_advisory_unlock_shared_int4(PG_FUNCTION_ARGS)
{
  int32 key1 = PG_GETARG_INT32(0);
  int32 key2 = PG_GETARG_INT32(1);
  LOCKTAG tag;
  bool res;

  PreventAdvisoryLocksInParallelMode();
  SET_LOCKTAG_INT32(tag, key1, key2);

  res = LockRelease(&tag, ShareLock, true);

  PG_RETURN_BOOL(res);
}

   
                                                         
   
Datum
pg_advisory_unlock_all(PG_FUNCTION_ARGS)
{
  LockReleaseSession(USER_LOCKMETHOD);

  PG_RETURN_VOID();
}
