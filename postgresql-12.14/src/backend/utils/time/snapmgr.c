                                                                            
   
             
                                
   
                                                                             
                                                                             
                                                                          
                                                                              
   
                                                                               
                                                                            
                                                                               
                                                                             
                                                                               
                                                                             
                                                       
   
                                                                         
                                                                          
                                  
   
                                                                          
                                                       
   
                                                                         
                                                                             
                 
   
                                                                              
                                                                           
                                                                          
                                                                               
                                                                         
                   
   
   
                                                                         
                                                                        
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "lib/pairingheap.h"
#include "miscadmin.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinval.h"
#include "storage/sinvaladt.h"
#include "storage/spin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/resowner_private.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

   
                  
   
int old_snapshot_threshold;                                     

   
                                                                     
   
typedef struct OldSnapshotControlData
{
     
                                                                            
                                   
     
  slock_t mutex_current;                                          
  TimestampTz current_timestamp;                                  
  slock_t mutex_latest_xmin;                                                    
  TransactionId latest_xmin;                                 
  TimestampTz next_map_update;                                      
  slock_t mutex_threshold;                                       
  TimestampTz threshold_timestamp;                              
  TransactionId threshold_xid;                                  

     
                                                              
     
                                                                            
                                                                           
                                                                      
                                                                             
                                                                    
                                                                    
                                                                       
                                                                           
                                                                           
                        
     
                                                                    
                                                                     
                                                                         
                                                                        
                                                                             
                                                                             
                                 
     
                                                                     
                                                                             
                        
     
                                
     
  int head_offset;                                                  
  TimestampTz head_timestamp;                                     
  int count_used;                                            
  TransactionId xid_by_minute[FLEXIBLE_ARRAY_MEMBER];
} OldSnapshotControlData;

static volatile OldSnapshotControlData *oldSnapshotControl;

   
                                                                             
                                                                      
                                                                              
                                                                           
                                                                          
                                                                              
                                            
   
                                                                       
                                                                    
   
static SnapshotData CurrentSnapshotData = {SNAPSHOT_MVCC};
static SnapshotData SecondarySnapshotData = {SNAPSHOT_MVCC};
SnapshotData CatalogSnapshotData = {SNAPSHOT_MVCC};
SnapshotData SnapshotSelfData = {SNAPSHOT_SELF};
SnapshotData SnapshotAnyData = {SNAPSHOT_ANY};

                                 
static Snapshot CurrentSnapshot = NULL;
static Snapshot SecondarySnapshot = NULL;
static Snapshot CatalogSnapshot = NULL;
static Snapshot HistoricSnapshot = NULL;

   
                                                                      
                                                                       
                                                                             
   
                                                                
                                                                    
                                                                       
                    
   
TransactionId TransactionXmin = FirstNormalTransactionId;
TransactionId RecentXmin = FirstNormalTransactionId;
TransactionId RecentGlobalXmin = InvalidTransactionId;
TransactionId RecentGlobalDataXmin = InvalidTransactionId;

                                                             
static HTAB *tuplecid_data = NULL;

   
                                          
   
                                                                            
   
                                                                         
                                                              
   
typedef struct ActiveSnapshotElt
{
  Snapshot as_snap;
  int as_level;
  struct ActiveSnapshotElt *as_next;
} ActiveSnapshotElt;

                                          
static ActiveSnapshotElt *ActiveSnapshot = NULL;

                                             
static ActiveSnapshotElt *OldestActiveSnapshot = NULL;

   
                                                                              
                                                                         
   
static int
xmin_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg);

static pairingheap RegisteredSnapshots = {&xmin_cmp, NULL, NULL};

                                                         
bool FirstSnapshotSet = false;

   
                                                                            
                                                                             
                                                                                
   
static Snapshot FirstXactSnapshot = NULL;

                                                
#define SNAPSHOT_EXPORT_DIR "pg_snapshots"

                                                     
typedef struct ExportedSnapshot
{
  char *snapfile;
  Snapshot snapshot;
} ExportedSnapshot;

                                                                            
static List *exportedSnapshots = NIL;

                                    
static TimestampTz
AlignTimestampToMinuteBoundary(TimestampTz ts);
static Snapshot
CopySnapshot(Snapshot snapshot);
static void
FreeSnapshot(Snapshot snapshot);
static void
SnapshotResetXmin(void);

   
                                     
   
                                                                     
                                                                      
   
typedef struct SerializedSnapshotData
{
  TransactionId xmin;
  TransactionId xmax;
  uint32 xcnt;
  int32 subxcnt;
  bool suboverflowed;
  bool takenDuringRecovery;
  CommandId curcid;
  TimestampTz whenTaken;
  XLogRecPtr lsn;
} SerializedSnapshotData;

Size
SnapMgrShmemSize(void)
{
  Size size;

  size = offsetof(OldSnapshotControlData, xid_by_minute);
  if (old_snapshot_threshold > 0)
  {
    size = add_size(size, mul_size(sizeof(TransactionId), OLD_SNAPSHOT_TIME_MAP_ENTRIES));
  }

  return size;
}

   
                                                   
   
void
SnapMgrInit(void)
{
  bool found;

     
                                                               
     
  oldSnapshotControl = (volatile OldSnapshotControlData *)ShmemInitStruct("OldSnapshotControlData", SnapMgrShmemSize(), &found);

  if (!found)
  {
    SpinLockInit(&oldSnapshotControl->mutex_current);
    oldSnapshotControl->current_timestamp = 0;
    SpinLockInit(&oldSnapshotControl->mutex_latest_xmin);
    oldSnapshotControl->latest_xmin = InvalidTransactionId;
    oldSnapshotControl->next_map_update = 0;
    SpinLockInit(&oldSnapshotControl->mutex_threshold);
    oldSnapshotControl->threshold_timestamp = 0;
    oldSnapshotControl->threshold_xid = InvalidTransactionId;
    oldSnapshotControl->head_offset = 0;
    oldSnapshotControl->head_timestamp = 0;
    oldSnapshotControl->count_used = 0;
  }
}

   
                          
                                                                   
   
                                                                                
                                                                          
                                                                              
                   
   
Snapshot
GetTransactionSnapshot(void)
{
     
                                                                            
                                                                             
                                                            
                               
     
  if (HistoricSnapshotActive())
  {
    Assert(!FirstSnapshotSet);
    return HistoricSnapshot;
  }

                                  
  if (!FirstSnapshotSet)
  {
       
                                                                          
                                                                
       
    InvalidateCatalogSnapshot();

    Assert(pairingheap_is_empty(&RegisteredSnapshots));
    Assert(FirstXactSnapshot == NULL);

    if (IsInParallelMode())
    {
      elog(ERROR, "cannot take query snapshot during a parallel operation");
    }

       
                                                                        
                                                                          
                                                                   
                                                                      
                                                                           
       
    if (IsolationUsesXactSnapshot())
    {
                                                             
      if (IsolationIsSerializable())
      {
        CurrentSnapshot = GetSerializableTransactionSnapshot(&CurrentSnapshotData);
      }
      else
      {
        CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);
      }
                             
      CurrentSnapshot = CopySnapshot(CurrentSnapshot);
      FirstXactSnapshot = CurrentSnapshot;
                                                        
      FirstXactSnapshot->regd_count++;
      pairingheap_add(&RegisteredSnapshots, &FirstXactSnapshot->ph_node);
    }
    else
    {
      CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);
    }

    FirstSnapshotSet = true;
    return CurrentSnapshot;
  }

  if (IsolationUsesXactSnapshot())
  {
    return CurrentSnapshot;
  }

                                                                    
  InvalidateCatalogSnapshot();

  CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);

  return CurrentSnapshot;
}

   
                     
                                                                 
                                                           
   
Snapshot
GetLatestSnapshot(void)
{
     
                                                                           
               
     
  if (IsInParallelMode())
  {
    elog(ERROR, "cannot update SecondarySnapshot during a parallel operation");
  }

     
                                                                         
                                                                          
     
  Assert(!HistoricSnapshotActive());

                                                                        
  if (!FirstSnapshotSet)
  {
    return GetTransactionSnapshot();
  }

  SecondarySnapshot = GetSnapshotData(&SecondarySnapshotData);

  return SecondarySnapshot;
}

   
                     
   
                                                                       
                                                                     
   
Snapshot
GetOldestSnapshot(void)
{
  Snapshot OldestRegisteredSnapshot = NULL;
  XLogRecPtr RegisteredLSN = InvalidXLogRecPtr;

  if (!pairingheap_is_empty(&RegisteredSnapshots))
  {
    OldestRegisteredSnapshot = pairingheap_container(SnapshotData, ph_node, pairingheap_first(&RegisteredSnapshots));
    RegisteredLSN = OldestRegisteredSnapshot->lsn;
  }

  if (OldestActiveSnapshot != NULL)
  {
    XLogRecPtr ActiveLSN = OldestActiveSnapshot->as_snap->lsn;

    if (XLogRecPtrIsInvalid(RegisteredLSN) || RegisteredLSN > ActiveLSN)
    {
      return OldestActiveSnapshot->as_snap;
    }
  }

  return OldestRegisteredSnapshot;
}

   
                      
                                                                   
                                           
   
Snapshot
GetCatalogSnapshot(Oid relid)
{
     
                                                                            
                                               
     
                                                                             
                         
     
  if (HistoricSnapshotActive())
  {
    return HistoricSnapshot;
  }

  return GetNonHistoricCatalogSnapshot(relid);
}

   
                                 
                                                                          
                                                                          
        
   
Snapshot
GetNonHistoricCatalogSnapshot(Oid relid)
{
     
                                                                         
                                                                            
                                                                             
                                                                           
                                                        
     
  if (CatalogSnapshot && !RelationInvalidatesSnapshotsOnly(relid) && !RelationHasSysCache(relid))
  {
    InvalidateCatalogSnapshot();
  }

  if (CatalogSnapshot == NULL)
  {
                           
    CatalogSnapshot = GetSnapshotData(&CatalogSnapshotData);

       
                                                                         
                                                                           
                                                                           
                                                                          
                                                                           
                                                                           
                                                                   
       
                                                                          
                                                 
       
    pairingheap_add(&RegisteredSnapshots, &CatalogSnapshot->ph_node);
  }

  return CatalogSnapshot;
}

   
                             
                                                          
   
                                                                             
                                                                            
                                                                            
                                                                             
               
   
void
InvalidateCatalogSnapshot(void)
{
  if (CatalogSnapshot)
  {
    pairingheap_remove(&RegisteredSnapshots, &CatalogSnapshot->ph_node);
    CatalogSnapshot = NULL;
    SnapshotResetXmin();
  }
}

   
                                          
                                                       
   
                                                                          
                                                                           
                                                                             
                                                                           
                                            
   
void
InvalidateCatalogSnapshotConditionally(void)
{
  if (CatalogSnapshot && ActiveSnapshot == NULL && pairingheap_is_singular(&RegisteredSnapshots))
  {
    InvalidateCatalogSnapshot();
  }
}

   
                        
                                                                        
   
void
SnapshotSetCommandId(CommandId curcid)
{
  if (!FirstSnapshotSet)
  {
    return;
  }

  if (CurrentSnapshot)
  {
    CurrentSnapshot->curcid = curcid;
  }
  if (SecondarySnapshot)
  {
    SecondarySnapshot->curcid = curcid;
  }
                                                   
}

   
                          
                                                                   
   
                                                                        
                                                                            
                              
   
static void
SetTransactionSnapshot(Snapshot sourcesnap, VirtualTransactionId *sourcevxid, int sourcepid, PGPROC *sourceproc)
{
                                               
  Assert(!FirstSnapshotSet);

                                                           
  InvalidateCatalogSnapshot();

  Assert(pairingheap_is_empty(&RegisteredSnapshots));
  Assert(FirstXactSnapshot == NULL);
  Assert(!HistoricSnapshotActive());

     
                                                                           
                                                                
                                                                             
                                                                             
                                                                           
                                                                        
     
  CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);

     
                                                           
     
  CurrentSnapshot->xmin = sourcesnap->xmin;
  CurrentSnapshot->xmax = sourcesnap->xmax;
  CurrentSnapshot->xcnt = sourcesnap->xcnt;
  Assert(sourcesnap->xcnt <= GetMaxSnapshotXidCount());
  if (sourcesnap->xcnt > 0)
  {
    memcpy(CurrentSnapshot->xip, sourcesnap->xip, sourcesnap->xcnt * sizeof(TransactionId));
  }
  CurrentSnapshot->subxcnt = sourcesnap->subxcnt;
  Assert(sourcesnap->subxcnt <= GetMaxSnapshotSubxidCount());
  if (sourcesnap->subxcnt > 0)
  {
    memcpy(CurrentSnapshot->subxip, sourcesnap->subxip, sourcesnap->subxcnt * sizeof(TransactionId));
  }
  CurrentSnapshot->suboverflowed = sourcesnap->suboverflowed;
  CurrentSnapshot->takenDuringRecovery = sourcesnap->takenDuringRecovery;
                                                            

     
                                                                         
                                                                          
                                                                       
                                                                  
                                           
     
                                                                            
                                                                      
                                                                      
     
  if (sourceproc != NULL)
  {
    if (!ProcArrayInstallRestoredXmin(CurrentSnapshot->xmin, sourceproc))
    {
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not import the requested snapshot"), errdetail("The source transaction is not running anymore.")));
    }
  }
  else if (!ProcArrayInstallImportedXmin(CurrentSnapshot->xmin, sourcevxid))
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not import the requested snapshot"), errdetail("The source process with PID %d is not running anymore.", sourcepid)));
  }

     
                                                                             
                                                                           
                                                                    
     
  if (IsolationUsesXactSnapshot())
  {
    if (IsolationIsSerializable())
    {
      SetSerializableTransactionSnapshot(CurrentSnapshot, sourcevxid, sourcepid);
    }
                           
    CurrentSnapshot = CopySnapshot(CurrentSnapshot);
    FirstXactSnapshot = CurrentSnapshot;
                                                      
    FirstXactSnapshot->regd_count++;
    pairingheap_add(&RegisteredSnapshots, &FirstXactSnapshot->ph_node);
  }

  FirstSnapshotSet = true;
}

   
                
                             
   
                                                                               
                                                         
   
static Snapshot
CopySnapshot(Snapshot snapshot)
{
  Snapshot newsnap;
  Size subxipoff;
  Size size;

  Assert(snapshot != InvalidSnapshot);

                                                                   
  size = subxipoff = sizeof(SnapshotData) + snapshot->xcnt * sizeof(TransactionId);
  if (snapshot->subxcnt > 0)
  {
    size += snapshot->subxcnt * sizeof(TransactionId);
  }

  newsnap = (Snapshot)MemoryContextAlloc(TopTransactionContext, size);
  memcpy(newsnap, snapshot, sizeof(SnapshotData));

  newsnap->regd_count = 0;
  newsnap->active_count = 0;
  newsnap->copied = true;

                       
  if (snapshot->xcnt > 0)
  {
    newsnap->xip = (TransactionId *)(newsnap + 1);
    memcpy(newsnap->xip, snapshot->xip, snapshot->xcnt * sizeof(TransactionId));
  }
  else
  {
    newsnap->xip = NULL;
  }

     
                                                                       
                                                                           
                                                                             
                                                 
     
  if (snapshot->subxcnt > 0 && (!snapshot->suboverflowed || snapshot->takenDuringRecovery))
  {
    newsnap->subxip = (TransactionId *)((char *)newsnap + subxipoff);
    memcpy(newsnap->subxip, snapshot->subxip, snapshot->subxcnt * sizeof(TransactionId));
  }
  else
  {
    newsnap->subxip = NULL;
  }

  return newsnap;
}

   
                
                                                
   
static void
FreeSnapshot(Snapshot snapshot)
{
  Assert(snapshot->regd_count == 0);
  Assert(snapshot->active_count == 0);
  Assert(snapshot->copied);

  pfree(snapshot);
}

   
                      
                                                          
   
                                                                           
                                                                            
                                                                    
   
void
PushActiveSnapshot(Snapshot snap)
{
  PushActiveSnapshotWithLevel(snap, GetCurrentTransactionNestLevel());
}

   
                               
                                                          
   
                                                                 
                                                                   
                                                                  
   
void
PushActiveSnapshotWithLevel(Snapshot snap, int snap_level)
{
  ActiveSnapshotElt *newactive;

  Assert(snap != InvalidSnapshot);
  Assert(ActiveSnapshot == NULL || snap_level >= ActiveSnapshot->as_level);

  newactive = MemoryContextAlloc(TopTransactionContext, sizeof(ActiveSnapshotElt));

     
                                                                       
                        
     
  if (snap == CurrentSnapshot || snap == SecondarySnapshot || !snap->copied)
  {
    newactive->as_snap = CopySnapshot(snap);
  }
  else
  {
    newactive->as_snap = snap;
  }

  newactive->as_next = ActiveSnapshot;
  newactive->as_level = snap_level;

  newactive->as_snap->active_count++;

  ActiveSnapshot = newactive;
  if (OldestActiveSnapshot == NULL)
  {
    OldestActiveSnapshot = ActiveSnapshot;
  }
}

   
                      
                                                           
   
                                                                         
                                                                        
                                                                 
   
void
PushCopiedSnapshot(Snapshot snapshot)
{
  PushActiveSnapshot(CopySnapshot(snapshot));
}

   
                                 
   
                                                                            
                                                   
   
void
UpdateActiveSnapshotCommandId(void)
{
  CommandId save_curcid, curcid;

  Assert(ActiveSnapshot != NULL);
  Assert(ActiveSnapshot->as_snap->active_count == 1);
  Assert(ActiveSnapshot->as_snap->regd_count == 0);

     
                                                                     
                                                                           
                                                                      
                                                      
                                                                        
                                                   
     
  save_curcid = ActiveSnapshot->as_snap->curcid;
  curcid = GetCurrentCommandId(false);
  if (IsInParallelMode() && save_curcid != curcid)
  {
    elog(ERROR, "cannot modify commandid in active snapshot during a parallel operation");
  }
  ActiveSnapshot->as_snap->curcid = curcid;
}

   
                     
   
                                                                                
                                                                
   
void
PopActiveSnapshot(void)
{
  ActiveSnapshotElt *newstack;

  newstack = ActiveSnapshot->as_next;

  Assert(ActiveSnapshot->as_snap->active_count > 0);

  ActiveSnapshot->as_snap->active_count--;

  if (ActiveSnapshot->as_snap->active_count == 0 && ActiveSnapshot->as_snap->regd_count == 0)
  {
    FreeSnapshot(ActiveSnapshot->as_snap);
  }

  pfree(ActiveSnapshot);
  ActiveSnapshot = newstack;
  if (ActiveSnapshot == NULL)
  {
    OldestActiveSnapshot = NULL;
  }

  SnapshotResetXmin();
}

   
                     
                                                     
   
Snapshot
GetActiveSnapshot(void)
{
  Assert(ActiveSnapshot != NULL);

  return ActiveSnapshot->as_snap;
}

   
                     
                                                                      
   
bool
ActiveSnapshotSet(void)
{
  return ActiveSnapshot != NULL;
}

   
                    
                                                                      
   
                                                       
   
Snapshot
RegisterSnapshot(Snapshot snapshot)
{
  if (snapshot == InvalidSnapshot)
  {
    return InvalidSnapshot;
  }

  return RegisterSnapshotOnOwner(snapshot, CurrentResourceOwner);
}

   
                           
                                                   
   
Snapshot
RegisterSnapshotOnOwner(Snapshot snapshot, ResourceOwner owner)
{
  Snapshot snap;

  if (snapshot == InvalidSnapshot)
  {
    return InvalidSnapshot;
  }

                                                  
  snap = snapshot->copied ? snapshot : CopySnapshot(snapshot);

                                    
  ResourceOwnerEnlargeSnapshots(owner);
  snap->regd_count++;
  ResourceOwnerRememberSnapshot(owner, snap);

  if (snap->regd_count == 1)
  {
    pairingheap_add(&RegisteredSnapshots, &snap->ph_node);
  }

  return snap;
}

   
                      
   
                                                                         
                                                                         
                      
   
void
UnregisterSnapshot(Snapshot snapshot)
{
  if (snapshot == NULL)
  {
    return;
  }

  UnregisterSnapshotFromOwner(snapshot, CurrentResourceOwner);
}

   
                               
                                                   
   
void
UnregisterSnapshotFromOwner(Snapshot snapshot, ResourceOwner owner)
{
  if (snapshot == NULL)
  {
    return;
  }

  Assert(snapshot->regd_count > 0);
  Assert(!pairingheap_is_empty(&RegisteredSnapshots));

  ResourceOwnerForgetSnapshot(owner, snapshot);

  snapshot->regd_count--;
  if (snapshot->regd_count == 0)
  {
    pairingheap_remove(&RegisteredSnapshots, &snapshot->ph_node);
  }

  if (snapshot->regd_count == 0 && snapshot->active_count == 0)
  {
    FreeSnapshot(snapshot);
    SnapshotResetXmin();
  }
}

   
                                                                            
                                                                   
   
static int
xmin_cmp(const pairingheap_node *a, const pairingheap_node *b, void *arg)
{
  const SnapshotData *asnap = pairingheap_const_container(SnapshotData, ph_node, a);
  const SnapshotData *bsnap = pairingheap_const_container(SnapshotData, ph_node, b);

  if (TransactionIdPrecedes(asnap->xmin, bsnap->xmin))
  {
    return 1;
  }
  else if (TransactionIdFollows(asnap->xmin, bsnap->xmin))
  {
    return -1;
  }
  else
  {
    return 0;
  }
}

   
                                                               
   
FullTransactionId
GetFullRecentGlobalXmin(void)
{
  FullTransactionId nextxid_full;
  uint32 nextxid_epoch;
  TransactionId nextxid_xid;
  uint32 epoch;

  Assert(TransactionIdIsNormal(RecentGlobalXmin));

     
                                                                          
                                                                             
               
     
  nextxid_full = ReadNextFullTransactionId();
  nextxid_epoch = EpochFromFullTransactionId(nextxid_full);
  nextxid_xid = XidFromFullTransactionId(nextxid_full);

  if (RecentGlobalXmin > nextxid_xid)
  {
    epoch = nextxid_epoch - 1;
  }
  else
  {
    epoch = nextxid_epoch;
  }

  return FullTransactionIdFromEpochAndXid(epoch, RecentGlobalXmin);
}

   
                     
   
                                                                                
                                                                             
              
   
                                                                             
                                                                         
                                                                            
                                                                           
                                    
   
                                                                              
                                                                           
                                                                            
                                                                              
                                                                              
                                              
   
static void
SnapshotResetXmin(void)
{
  Snapshot minSnapshot;

  if (ActiveSnapshot != NULL)
  {
    return;
  }

  if (pairingheap_is_empty(&RegisteredSnapshots))
  {
    MyPgXact->xmin = InvalidTransactionId;
    return;
  }

  minSnapshot = pairingheap_container(SnapshotData, ph_node, pairingheap_first(&RegisteredSnapshots));

  if (TransactionIdPrecedes(MyPgXact->xmin, minSnapshot->xmin))
  {
    MyPgXact->xmin = minSnapshot->xmin;
  }
}

   
                        
   
void
AtSubCommit_Snapshot(int level)
{
  ActiveSnapshotElt *active;

     
                                                                            
                                      
     
  for (active = ActiveSnapshot; active != NULL; active = active->as_next)
  {
    if (active->as_level < level)
    {
      break;
    }
    active->as_level = level - 1;
  }
}

   
                       
                                                    
   
void
AtSubAbort_Snapshot(int level)
{
                                                              
  while (ActiveSnapshot && ActiveSnapshot->as_level >= level)
  {
    ActiveSnapshotElt *next;

    next = ActiveSnapshot->as_next;

       
                                                                           
                                                                          
       
    Assert(ActiveSnapshot->as_snap->active_count >= 1);
    ActiveSnapshot->as_snap->active_count -= 1;

    if (ActiveSnapshot->as_snap->active_count == 0 && ActiveSnapshot->as_snap->regd_count == 0)
    {
      FreeSnapshot(ActiveSnapshot->as_snap);
    }

                                    
    pfree(ActiveSnapshot);

    ActiveSnapshot = next;
    if (ActiveSnapshot == NULL)
    {
      OldestActiveSnapshot = NULL;
    }
  }

  SnapshotResetXmin();
}

   
                     
                                                               
   
void
AtEOXact_Snapshot(bool isCommit, bool resetXmin)
{
     
                                                                        
                                                                    
                                                                             
                                                                       
                                                                        
                                                                             
                       
     
  if (FirstXactSnapshot != NULL)
  {
    Assert(FirstXactSnapshot->regd_count > 0);
    Assert(!pairingheap_is_empty(&RegisteredSnapshots));
    pairingheap_remove(&RegisteredSnapshots, &FirstXactSnapshot->ph_node);
  }
  FirstXactSnapshot = NULL;

     
                                                  
     
  if (exportedSnapshots != NIL)
  {
    ListCell *lc;

       
                                                                           
                                                                        
                                                       
       
                                                                        
                                
       
                                                                         
                                                                      
       
    foreach (lc, exportedSnapshots)
    {
      ExportedSnapshot *esnap = (ExportedSnapshot *)lfirst(lc);

      if (unlink(esnap->snapfile))
      {
        elog(WARNING, "could not unlink file \"%s\": %m", esnap->snapfile);
      }

      pairingheap_remove(&RegisteredSnapshots, &esnap->snapshot->ph_node);
    }

    exportedSnapshots = NIL;
  }

                                    
  InvalidateCatalogSnapshot();

                                                    
  if (isCommit)
  {
    ActiveSnapshotElt *active;

    if (!pairingheap_is_empty(&RegisteredSnapshots))
    {
      elog(WARNING, "registered snapshots seem to remain after cleanup");
    }

                                                  
    for (active = ActiveSnapshot; active != NULL; active = active->as_next)
    {
      elog(WARNING, "snapshot %p still active", active);
    }
  }

     
                                                                          
                                               
     
  ActiveSnapshot = NULL;
  OldestActiveSnapshot = NULL;
  pairingheap_reset(&RegisteredSnapshots);

  CurrentSnapshot = NULL;
  SecondarySnapshot = NULL;

  FirstSnapshotSet = false;

     
                                                                           
                                                                    
                                                                 
     
  if (resetXmin)
  {
    SnapshotResetXmin();
  }

  Assert(resetXmin || MyPgXact->xmin == 0);
}

   
                  
                                                                        
                                                                      
              
   
char *
ExportSnapshot(Snapshot snapshot)
{
  TransactionId topXid;
  TransactionId *children;
  ExportedSnapshot *esnap;
  int nchildren;
  int addTopXid;
  StringInfoData buf;
  FILE *f;
  int i;
  MemoryContext oldcxt;
  char path[MAXPGPATH];
  char pathtmp[MAXPGPATH];

     
                                                                             
                                                                             
                                                                           
                                                                           
                                                                       
                                                                             
            
     
                                                                       
                                                                          
                   
     

     
                                                                         
     
  topXid = GetTopTransactionIdIfAny();

     
                                                                          
                                                                            
              
     
  if (IsSubTransaction())
  {
    ereport(ERROR, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION), errmsg("cannot export a snapshot from a subtransaction")));
  }

     
                                                                      
                                                                            
                                       
     
  nchildren = xactGetCommittedChildren(&children);

     
                                                                           
                                    
     
  snprintf(path, sizeof(path), SNAPSHOT_EXPORT_DIR "/%08X-%08X-%d", MyProc->backendId, MyProc->lxid, list_length(exportedSnapshots) + 1);

     
                                                                 
                                                                           
                                                                    
                  
     
  snapshot = CopySnapshot(snapshot);

  oldcxt = MemoryContextSwitchTo(TopTransactionContext);
  esnap = (ExportedSnapshot *)palloc(sizeof(ExportedSnapshot));
  esnap->snapfile = pstrdup(path);
  esnap->snapshot = snapshot;
  exportedSnapshots = lappend(exportedSnapshots, esnap);
  MemoryContextSwitchTo(oldcxt);

  snapshot->regd_count++;
  pairingheap_add(&RegisteredSnapshots, &snapshot->ph_node);

     
                                                                             
                                                                            
                                                      
     
  initStringInfo(&buf);

  appendStringInfo(&buf, "vxid:%d/%u\n", MyProc->backendId, MyProc->lxid);
  appendStringInfo(&buf, "pid:%d\n", MyProcPid);
  appendStringInfo(&buf, "dbid:%u\n", MyDatabaseId);
  appendStringInfo(&buf, "iso:%d\n", XactIsoLevel);
  appendStringInfo(&buf, "ro:%d\n", XactReadOnly);

  appendStringInfo(&buf, "xmin:%u\n", snapshot->xmin);
  appendStringInfo(&buf, "xmax:%u\n", snapshot->xmax);

     
                                                                           
                                                                           
                                                                            
                                                                       
     
                                                                           
                                                                             
                                                                       
                  
     
  addTopXid = (TransactionIdIsValid(topXid) && TransactionIdPrecedes(topXid, snapshot->xmax)) ? 1 : 0;
  appendStringInfo(&buf, "xcnt:%d\n", snapshot->xcnt + addTopXid);
  for (i = 0; i < snapshot->xcnt; i++)
  {
    appendStringInfo(&buf, "xip:%u\n", snapshot->xip[i]);
  }
  if (addTopXid)
  {
    appendStringInfo(&buf, "xip:%u\n", topXid);
  }

     
                                                                             
                                             
     
  if (snapshot->suboverflowed || snapshot->subxcnt + nchildren > GetMaxSnapshotSubxidCount())
  {
    appendStringInfoString(&buf, "sof:1\n");
  }
  else
  {
    appendStringInfoString(&buf, "sof:0\n");
    appendStringInfo(&buf, "sxcnt:%d\n", snapshot->subxcnt + nchildren);
    for (i = 0; i < snapshot->subxcnt; i++)
    {
      appendStringInfo(&buf, "sxp:%u\n", snapshot->subxip[i]);
    }
    for (i = 0; i < nchildren; i++)
    {
      appendStringInfo(&buf, "sxp:%u\n", children[i]);
    }
  }
  appendStringInfo(&buf, "rec:%u\n", snapshot->takenDuringRecovery);

     
                                                                         
                                                                      
                                                               
                                                                            
     
  snprintf(pathtmp, sizeof(pathtmp), "%s.tmp", path);
  if (!(f = AllocateFile(pathtmp, PG_BINARY_W)))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not create file \"%s\": %m", pathtmp)));
  }

  if (fwrite(buf.data, buf.len, 1, f) != 1)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", pathtmp)));
  }

                                                             

  if (FreeFile(f))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", pathtmp)));
  }

     
                                                                           
                                
     
  if (rename(pathtmp, path) < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", pathtmp, path)));
  }

     
                                                                           
                                                                        
                                                                          
                                                                         
     
  return pstrdup(path + strlen(SNAPSHOT_EXPORT_DIR) + 1);
}

   
                      
                                             
   
Datum
pg_export_snapshot(PG_FUNCTION_ARGS)
{
  char *snapshotName;

  snapshotName = ExportSnapshot(GetActiveSnapshot());
  PG_RETURN_TEXT_P(cstring_to_text(snapshotName));
}

   
                                                                       
                                                                     
                                                   
   
static int
parseIntFromText(const char *prefix, char **s, const char *filename)
{
  char *ptr = *s;
  int prefixlen = strlen(prefix);
  int val;

  if (strncmp(ptr, prefix, prefixlen) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  ptr += prefixlen;
  if (sscanf(ptr, "%d", &val) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  ptr = strchr(ptr, '\n');
  if (!ptr)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  *s = ptr + 1;
  return val;
}

static TransactionId
parseXidFromText(const char *prefix, char **s, const char *filename)
{
  char *ptr = *s;
  int prefixlen = strlen(prefix);
  TransactionId val;

  if (strncmp(ptr, prefix, prefixlen) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  ptr += prefixlen;
  if (sscanf(ptr, "%u", &val) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  ptr = strchr(ptr, '\n');
  if (!ptr)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  *s = ptr + 1;
  return val;
}

static void
parseVxidFromText(const char *prefix, char **s, const char *filename, VirtualTransactionId *vxid)
{
  char *ptr = *s;
  int prefixlen = strlen(prefix);

  if (strncmp(ptr, prefix, prefixlen) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  ptr += prefixlen;
  if (sscanf(ptr, "%d/%u", &vxid->backendId, &vxid->localTransactionId) != 2)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  ptr = strchr(ptr, '\n');
  if (!ptr)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", filename)));
  }
  *s = ptr + 1;
}

   
                  
                                                                     
                                                                        
                                                        
   
void
ImportSnapshot(const char *idstr)
{
  char path[MAXPGPATH];
  FILE *f;
  struct stat stat_buf;
  char *filebuf;
  int xcnt;
  int i;
  VirtualTransactionId src_vxid;
  int src_pid;
  Oid src_dbid;
  int src_isolevel;
  bool src_readonly;
  SnapshotData snapshot;

     
                                                                           
                                                                          
                                                                            
               
     
  if (FirstSnapshotSet || GetTopTransactionIdIfAny() != InvalidTransactionId || IsSubTransaction())
  {
    ereport(ERROR, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION), errmsg("SET TRANSACTION SNAPSHOT must be called before any query")));
  }

     
                                                                             
                                                                  
     
  if (!IsolationUsesXactSnapshot())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("a snapshot-importing transaction must have isolation level SERIALIZABLE or REPEATABLE READ")));
  }

     
                                                                          
                                                     
     
  if (strspn(idstr, "0123456789ABCDEF-") != strlen(idstr))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid snapshot identifier: \"%s\"", idstr)));
  }

                         
  snprintf(path, MAXPGPATH, SNAPSHOT_EXPORT_DIR "/%s", idstr);

  f = AllocateFile(path, PG_BINARY_R);
  if (!f)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid snapshot identifier: \"%s\"", idstr)));
  }

                                                                        
  if (fstat(fileno(f), &stat_buf))
  {
    elog(ERROR, "could not stat file \"%s\": %m", path);
  }

                                                
  filebuf = (char *)palloc(stat_buf.st_size + 1);
  if (fread(filebuf, stat_buf.st_size, 1, f) != 1)
  {
    elog(ERROR, "could not read file \"%s\": %m", path);
  }

  filebuf[stat_buf.st_size] = '\0';

  FreeFile(f);

     
                                                              
     
  memset(&snapshot, 0, sizeof(snapshot));

  parseVxidFromText("vxid:", &filebuf, path, &src_vxid);
  src_pid = parseIntFromText("pid:", &filebuf, path);
                                                
  src_dbid = parseXidFromText("dbid:", &filebuf, path);
  src_isolevel = parseIntFromText("iso:", &filebuf, path);
  src_readonly = parseIntFromText("ro:", &filebuf, path);

  snapshot.snapshot_type = SNAPSHOT_MVCC;

  snapshot.xmin = parseXidFromText("xmin:", &filebuf, path);
  snapshot.xmax = parseXidFromText("xmax:", &filebuf, path);

  snapshot.xcnt = xcnt = parseIntFromText("xcnt:", &filebuf, path);

                                                
  if (xcnt < 0 || xcnt > GetMaxSnapshotXidCount())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", path)));
  }

  snapshot.xip = (TransactionId *)palloc(xcnt * sizeof(TransactionId));
  for (i = 0; i < xcnt; i++)
  {
    snapshot.xip[i] = parseXidFromText("xip:", &filebuf, path);
  }

  snapshot.suboverflowed = parseIntFromText("sof:", &filebuf, path);

  if (!snapshot.suboverflowed)
  {
    snapshot.subxcnt = xcnt = parseIntFromText("sxcnt:", &filebuf, path);

                                                  
    if (xcnt < 0 || xcnt > GetMaxSnapshotSubxidCount())
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", path)));
    }

    snapshot.subxip = (TransactionId *)palloc(xcnt * sizeof(TransactionId));
    for (i = 0; i < xcnt; i++)
    {
      snapshot.subxip[i] = parseXidFromText("sxp:", &filebuf, path);
    }
  }
  else
  {
    snapshot.subxcnt = 0;
    snapshot.subxip = NULL;
  }

  snapshot.takenDuringRecovery = parseIntFromText("rec:", &filebuf, path);

     
                                                                        
                                                                       
             
     
  if (!VirtualTransactionIdIsValid(src_vxid) || !OidIsValid(src_dbid) || !TransactionIdIsNormal(snapshot.xmin) || !TransactionIdIsNormal(snapshot.xmax))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid snapshot data in file \"%s\"", path)));
  }

     
                                                                          
                                                                             
                                                                       
                                                                     
     
  if (IsolationIsSerializable())
  {
    if (src_isolevel != XACT_SERIALIZABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("a serializable transaction cannot import a snapshot from a non-serializable transaction")));
    }
    if (src_readonly && !XactReadOnly)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("a non-read-only serializable transaction cannot import a snapshot from a read-only transaction")));
    }
  }

     
                                                                         
                                                                          
                                                                        
                                                                             
                                                                     
                                                                        
                                                                     
     
  if (src_dbid != MyDatabaseId)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot import a snapshot from a different database")));
  }

                                
  SetTransactionSnapshot(&snapshot, &src_vxid, src_pid, NULL);
}

   
                            
                                                                 
   
bool
XactHasExportedSnapshots(void)
{
  return (exportedSnapshots != NIL);
}

   
                                  
                                                                       
                                                
   
                                                                    
   
void
DeleteAllExportedSnapshotFiles(void)
{
  char buf[MAXPGPATH + sizeof(SNAPSHOT_EXPORT_DIR)];
  DIR *s_dir;
  struct dirent *s_de;

     
                                                                            
                                                                         
                                                                           
     
  s_dir = AllocateDir(SNAPSHOT_EXPORT_DIR);

  while ((s_de = ReadDirExtended(s_dir, SNAPSHOT_EXPORT_DIR, LOG)) != NULL)
  {
    if (strcmp(s_de->d_name, ".") == 0 || strcmp(s_de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(buf, sizeof(buf), SNAPSHOT_EXPORT_DIR "/%s", s_de->d_name);

    if (unlink(buf) != 0)
    {
      ereport(LOG, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", buf)));
    }
  }

  FreeDir(s_dir);
}

   
                                      
                                                                
   
                                                                               
                                                                           
                                                                 
   
bool
ThereAreNoPriorRegisteredSnapshots(void)
{
  if (pairingheap_is_empty(&RegisteredSnapshots) || pairingheap_is_singular(&RegisteredSnapshots))
  {
    return true;
  }

  return false;
}

   
                                                            
   
                                                                            
                                                      
   
static TimestampTz
AlignTimestampToMinuteBoundary(TimestampTz ts)
{
  TimestampTz retval = ts + (USECS_PER_MINUTE - 1);

  return retval - (retval % USECS_PER_MINUTE);
}

   
                                       
   
                                                                      
                                    
   
TimestampTz
GetSnapshotCurrentTimestamp(void)
{
  TimestampTz now = GetCurrentTimestamp();

     
                                                                             
     
  SpinLockAcquire(&oldSnapshotControl->mutex_current);
  if (now <= oldSnapshotControl->current_timestamp)
  {
    now = oldSnapshotControl->current_timestamp;
  }
  else
  {
    oldSnapshotControl->current_timestamp = now;
  }
  SpinLockRelease(&oldSnapshotControl->mutex_current);

  return now;
}

   
                                                                              
                                  
   
                                                                              
                                                             
   
TimestampTz
GetOldSnapshotThresholdTimestamp(void)
{
  TimestampTz threshold_timestamp;

  SpinLockAcquire(&oldSnapshotControl->mutex_threshold);
  threshold_timestamp = oldSnapshotControl->threshold_timestamp;
  SpinLockRelease(&oldSnapshotControl->mutex_threshold);

  return threshold_timestamp;
}

static void
SetOldSnapshotThresholdTimestamp(TimestampTz ts, TransactionId xlimit)
{
  SpinLockAcquire(&oldSnapshotControl->mutex_threshold);
  oldSnapshotControl->threshold_timestamp = ts;
  oldSnapshotControl->threshold_xid = xlimit;
  SpinLockRelease(&oldSnapshotControl->mutex_threshold);
}

   
                                       
   
                                                                             
                                                                            
                                                                              
                                                                             
                         
   
TransactionId
TransactionIdLimitedForOldSnapshots(TransactionId recentXmin, Relation relation)
{
  if (TransactionIdIsNormal(recentXmin) && old_snapshot_threshold >= 0 && RelationAllowsEarlyPruning(relation))
  {
    TimestampTz ts = GetSnapshotCurrentTimestamp();
    TransactionId xlimit = recentXmin;
    TransactionId latest_xmin;
    TimestampTz update_ts;
    bool same_ts_as_threshold = false;

    SpinLockAcquire(&oldSnapshotControl->mutex_latest_xmin);
    latest_xmin = oldSnapshotControl->latest_xmin;
    update_ts = oldSnapshotControl->next_map_update;
    SpinLockRelease(&oldSnapshotControl->mutex_latest_xmin);

       
                                                                          
                                                                    
                                                                        
                                                                         
                                                                       
                                                       
       
    if (old_snapshot_threshold == 0)
    {
      if (TransactionIdPrecedes(latest_xmin, MyPgXact->xmin) && TransactionIdFollows(latest_xmin, xlimit))
      {
        xlimit = latest_xmin;
      }

      ts -= 5 * USECS_PER_SEC;
      SetOldSnapshotThresholdTimestamp(ts, xlimit);

      return xlimit;
    }

    ts = AlignTimestampToMinuteBoundary(ts) - (old_snapshot_threshold * USECS_PER_MINUTE);

                                                 
    SpinLockAcquire(&oldSnapshotControl->mutex_threshold);
    if (ts == oldSnapshotControl->threshold_timestamp)
    {
      xlimit = oldSnapshotControl->threshold_xid;
      same_ts_as_threshold = true;
    }
    SpinLockRelease(&oldSnapshotControl->mutex_threshold);

    if (!same_ts_as_threshold)
    {
      if (ts == update_ts)
      {
        xlimit = latest_xmin;
        if (NormalTransactionIdFollows(xlimit, recentXmin))
        {
          SetOldSnapshotThresholdTimestamp(ts, xlimit);
        }
      }
      else
      {
        LWLockAcquire(OldSnapshotTimeMapLock, LW_SHARED);

        if (oldSnapshotControl->count_used > 0 && ts >= oldSnapshotControl->head_timestamp)
        {
          int offset;

          offset = ((ts - oldSnapshotControl->head_timestamp) / USECS_PER_MINUTE);
          if (offset > oldSnapshotControl->count_used - 1)
          {
            offset = oldSnapshotControl->count_used - 1;
          }
          offset = (oldSnapshotControl->head_offset + offset) % OLD_SNAPSHOT_TIME_MAP_ENTRIES;
          xlimit = oldSnapshotControl->xid_by_minute[offset];

          if (NormalTransactionIdFollows(xlimit, recentXmin))
          {
            SetOldSnapshotThresholdTimestamp(ts, xlimit);
          }
        }

        LWLockRelease(OldSnapshotTimeMapLock);
      }
    }

       
                                                                         
       
                                                                  
                                                                           
                                                                          
                                                                           
                                        
       
    if (TransactionIdIsNormal(latest_xmin) && TransactionIdPrecedes(latest_xmin, xlimit))
    {
      xlimit = latest_xmin;
    }

    if (NormalTransactionIdFollows(xlimit, recentXmin))
    {
      return xlimit;
    }
  }

  return recentXmin;
}

   
                                                           
   
void
MaintainOldSnapshotTimeMapping(TimestampTz whenTaken, TransactionId xmin)
{
  TimestampTz ts;
  TransactionId latest_xmin;
  TimestampTz update_ts;
  bool map_update_required = false;

                                                                        
  Assert(old_snapshot_threshold >= 0);

  ts = AlignTimestampToMinuteBoundary(whenTaken);

     
                                                                            
                                                         
     
  SpinLockAcquire(&oldSnapshotControl->mutex_latest_xmin);
  latest_xmin = oldSnapshotControl->latest_xmin;
  update_ts = oldSnapshotControl->next_map_update;
  if (ts > update_ts)
  {
    oldSnapshotControl->next_map_update = ts;
    map_update_required = true;
  }
  if (TransactionIdFollows(xmin, latest_xmin))
  {
    oldSnapshotControl->latest_xmin = xmin;
  }
  SpinLockRelease(&oldSnapshotControl->mutex_latest_xmin);

                                                            
  if (!map_update_required)
  {
    return;
  }

                                                            
  if (old_snapshot_threshold == 0)
  {
    return;
  }

     
                                                                            
                                                                    
                                                                           
                                                           
     
  if (whenTaken < 0)
  {
    elog(DEBUG1, "MaintainOldSnapshotTimeMapping called with negative whenTaken = %ld", (long)whenTaken);
    return;
  }
  if (!TransactionIdIsNormal(xmin))
  {
    elog(DEBUG1, "MaintainOldSnapshotTimeMapping called with xmin = %lu", (unsigned long)xmin);
    return;
  }

  LWLockAcquire(OldSnapshotTimeMapLock, LW_EXCLUSIVE);

  Assert(oldSnapshotControl->head_offset >= 0);
  Assert(oldSnapshotControl->head_offset < OLD_SNAPSHOT_TIME_MAP_ENTRIES);
  Assert((oldSnapshotControl->head_timestamp % USECS_PER_MINUTE) == 0);
  Assert(oldSnapshotControl->count_used >= 0);
  Assert(oldSnapshotControl->count_used <= OLD_SNAPSHOT_TIME_MAP_ENTRIES);

  if (oldSnapshotControl->count_used == 0)
  {
                                              
    oldSnapshotControl->head_offset = 0;
    oldSnapshotControl->head_timestamp = ts;
    oldSnapshotControl->count_used = 1;
    oldSnapshotControl->xid_by_minute[0] = xmin;
  }
  else if (ts < oldSnapshotControl->head_timestamp)
  {
                                 
    LWLockRelease(OldSnapshotTimeMapLock);
    elog(DEBUG1, "MaintainOldSnapshotTimeMapping called with old whenTaken = %ld", (long)whenTaken);
    return;
  }
  else if (ts <= (oldSnapshotControl->head_timestamp + ((oldSnapshotControl->count_used - 1) * USECS_PER_MINUTE)))
  {
                                                   
    int bucket = (oldSnapshotControl->head_offset + ((ts - oldSnapshotControl->head_timestamp) / USECS_PER_MINUTE)) % OLD_SNAPSHOT_TIME_MAP_ENTRIES;

    if (TransactionIdPrecedes(oldSnapshotControl->xid_by_minute[bucket], xmin))
    {
      oldSnapshotControl->xid_by_minute[bucket] = xmin;
    }
  }
  else
  {
                                                                      
    int advance = ((ts - oldSnapshotControl->head_timestamp) / USECS_PER_MINUTE);

    oldSnapshotControl->head_timestamp = ts;

    if (advance >= OLD_SNAPSHOT_TIME_MAP_ENTRIES)
    {
                                                                    
      oldSnapshotControl->head_offset = 0;
      oldSnapshotControl->count_used = 1;
      oldSnapshotControl->xid_by_minute[0] = xmin;
    }
    else
    {
                                                       
      int i;

      for (i = 0; i < advance; i++)
      {
        if (oldSnapshotControl->count_used == OLD_SNAPSHOT_TIME_MAP_ENTRIES)
        {
                                                         
          int old_head = oldSnapshotControl->head_offset;

          if (old_head == (OLD_SNAPSHOT_TIME_MAP_ENTRIES - 1))
          {
            oldSnapshotControl->head_offset = 0;
          }
          else
          {
            oldSnapshotControl->head_offset = old_head + 1;
          }
          oldSnapshotControl->xid_by_minute[old_head] = xmin;
        }
        else
        {
                                           
          int new_tail = (oldSnapshotControl->head_offset + oldSnapshotControl->count_used) % OLD_SNAPSHOT_TIME_MAP_ENTRIES;

          oldSnapshotControl->count_used++;
          oldSnapshotControl->xid_by_minute[new_tail] = xmin;
        }
      }
    }
  }

  LWLockRelease(OldSnapshotTimeMapLock);
}

   
                                                                               
                                                                     
   
                                
   
void
SetupHistoricSnapshot(Snapshot historic_snapshot, HTAB *tuplecids)
{
  Assert(historic_snapshot != NULL);

                                     
  HistoricSnapshot = historic_snapshot;

                                      
  tuplecid_data = tuplecids;
}

   
                                                 
   
void
TeardownHistoricSnapshot(bool is_error)
{
  HistoricSnapshot = NULL;
  tuplecid_data = NULL;
}

bool
HistoricSnapshotActive(void)
{
  return HistoricSnapshot != NULL;
}

HTAB *
HistoricSnapshotGetTupleCids(void)
{
  Assert(HistoricSnapshotActive());
  return tuplecid_data;
}

   
                         
                                                         
   
                                                                      
                           
   
Size
EstimateSnapshotSpace(Snapshot snap)
{
  Size size;

  Assert(snap != InvalidSnapshot);
  Assert(snap->snapshot_type == SNAPSHOT_MVCC);

                                                                   
  size = add_size(sizeof(SerializedSnapshotData), mul_size(snap->xcnt, sizeof(TransactionId)));
  if (snap->subxcnt > 0 && (!snap->suboverflowed || snap->takenDuringRecovery))
  {
    size = add_size(size, mul_size(snap->subxcnt, sizeof(TransactionId)));
  }

  return size;
}

   
                     
                                                                           
                                      
   
void
SerializeSnapshot(Snapshot snapshot, char *start_address)
{
  SerializedSnapshotData serialized_snapshot;

  Assert(snapshot->subxcnt >= 0);

                                
  serialized_snapshot.xmin = snapshot->xmin;
  serialized_snapshot.xmax = snapshot->xmax;
  serialized_snapshot.xcnt = snapshot->xcnt;
  serialized_snapshot.subxcnt = snapshot->subxcnt;
  serialized_snapshot.suboverflowed = snapshot->suboverflowed;
  serialized_snapshot.takenDuringRecovery = snapshot->takenDuringRecovery;
  serialized_snapshot.curcid = snapshot->curcid;
  serialized_snapshot.whenTaken = snapshot->whenTaken;
  serialized_snapshot.lsn = snapshot->lsn;

     
                                                                           
                                                                           
                                     
     
  if (serialized_snapshot.suboverflowed && !snapshot->takenDuringRecovery)
  {
    serialized_snapshot.subxcnt = 0;
  }

                                                
  memcpy(start_address, &serialized_snapshot, sizeof(SerializedSnapshotData));

                      
  if (snapshot->xcnt > 0)
  {
    memcpy((TransactionId *)(start_address + sizeof(SerializedSnapshotData)), snapshot->xip, snapshot->xcnt * sizeof(TransactionId));
  }

     
                                                                      
                                                                           
                                                                             
                                                 
     
  if (serialized_snapshot.subxcnt > 0)
  {
    Size subxipoff = sizeof(SerializedSnapshotData) + snapshot->xcnt * sizeof(TransactionId);

    memcpy((TransactionId *)(start_address + subxipoff), snapshot->subxip, snapshot->subxcnt * sizeof(TransactionId));
  }
}

   
                   
                                                              
   
                                                                               
                                                         
   
Snapshot
RestoreSnapshot(char *start_address)
{
  SerializedSnapshotData serialized_snapshot;
  Size size;
  Snapshot snapshot;
  TransactionId *serialized_xids;

  memcpy(&serialized_snapshot, start_address, sizeof(SerializedSnapshotData));
  serialized_xids = (TransactionId *)(start_address + sizeof(SerializedSnapshotData));

                                                                   
  size = sizeof(SnapshotData) + serialized_snapshot.xcnt * sizeof(TransactionId) + serialized_snapshot.subxcnt * sizeof(TransactionId);

                                
  snapshot = (Snapshot)MemoryContextAlloc(TopTransactionContext, size);
  snapshot->snapshot_type = SNAPSHOT_MVCC;
  snapshot->xmin = serialized_snapshot.xmin;
  snapshot->xmax = serialized_snapshot.xmax;
  snapshot->xip = NULL;
  snapshot->xcnt = serialized_snapshot.xcnt;
  snapshot->subxip = NULL;
  snapshot->subxcnt = serialized_snapshot.subxcnt;
  snapshot->suboverflowed = serialized_snapshot.suboverflowed;
  snapshot->takenDuringRecovery = serialized_snapshot.takenDuringRecovery;
  snapshot->curcid = serialized_snapshot.curcid;
  snapshot->whenTaken = serialized_snapshot.whenTaken;
  snapshot->lsn = serialized_snapshot.lsn;

                              
  if (serialized_snapshot.xcnt > 0)
  {
    snapshot->xip = (TransactionId *)(snapshot + 1);
    memcpy(snapshot->xip, serialized_xids, serialized_snapshot.xcnt * sizeof(TransactionId));
  }

                                 
  if (serialized_snapshot.subxcnt > 0)
  {
    snapshot->subxip = ((TransactionId *)(snapshot + 1)) + serialized_snapshot.xcnt;
    memcpy(snapshot->subxip, serialized_xids + serialized_snapshot.xcnt, serialized_snapshot.subxcnt * sizeof(TransactionId));
  }

                                                                            
  snapshot->regd_count = 0;
  snapshot->active_count = 0;
  snapshot->copied = true;

  return snapshot;
}

   
                                                            
   
                                                                            
                               
   
void
RestoreTransactionSnapshot(Snapshot snapshot, void *master_pgproc)
{
  SetTransactionSnapshot(snapshot, NULL, InvalidPid, master_pgproc);
}

   
                     
                                                                  
   
                                                                           
                                                                            
                                                                           
                                                                         
                                 
   
bool
XidInMVCCSnapshot(TransactionId xid, Snapshot snapshot)
{
  uint32 i;

     
                                                                            
                                                                           
                                                                             
                                                                           
                                                                     
     

                                         
  if (TransactionIdPrecedes(xid, snapshot->xmin))
  {
    return false;
  }
                                      
  if (TransactionIdFollowsOrEquals(xid, snapshot->xmax))
  {
    return true;
  }

     
                                                                            
                      
     
  if (!snapshot->takenDuringRecovery)
  {
       
                                                                      
                                                                          
                                                                        
                                                                       
                                                              
       
    if (!snapshot->suboverflowed)
    {
                                               
      int32 j;

      for (j = 0; j < snapshot->subxcnt; j++)
      {
        if (TransactionIdEquals(xid, snapshot->subxip[j]))
        {
          return true;
        }
      }

                                                   
    }
    else
    {
         
                                                                         
                                                   
         
      xid = SubTransGetTopmostTransaction(xid);

         
                                                                       
                                                                    
               
         
      if (TransactionIdPrecedes(xid, snapshot->xmin))
      {
        return false;
      }
    }

    for (i = 0; i < snapshot->xcnt; i++)
    {
      if (TransactionIdEquals(xid, snapshot->xip[i]))
      {
        return true;
      }
    }
  }
  else
  {
    int32 j;

       
                                                                           
                                                                     
                                                                 
       
                                                         
       
    if (snapshot->suboverflowed)
    {
         
                                                                         
                                                   
         
      xid = SubTransGetTopmostTransaction(xid);

         
                                                                       
                                                                    
               
         
      if (TransactionIdPrecedes(xid, snapshot->xmin))
      {
        return false;
      }
    }

       
                                                                 
                                                                          
                                                                   
       
    for (j = 0; j < snapshot->subxcnt; j++)
    {
      if (TransactionIdEquals(xid, snapshot->subxip[j]))
      {
        return true;
      }
    }
  }

  return false;
}
