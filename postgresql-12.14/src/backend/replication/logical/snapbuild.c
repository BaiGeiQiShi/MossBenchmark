                                                                            
   
               
   
                                                                              
                                                                          
          
   
          
   
                                                                               
                                                                           
                                                                              
                                             
   
                                                                    
                                                                             
                                                                             
                                                                             
                                                                  
                                                                             
                                                                               
                                                                               
                                                                      
                                                                            
                                                                   
                                                                      
   
                                                                              
                                                                              
                                                                               
                                                                               
                                                                           
                                                                              
                                                                        
                                                                               
                                                                              
                                                         
   
                                                                     
                                                                        
                                                                               
                                                                         
                                                       
                                                                         
                                                                           
                                                                            
                                                
   
                                                                            
                                              
   
                                                                      
                                                                     
   
   
   
                                                                            
                                                                       
   
                                   
                                       
                                             
                       
                       
                                     
                       
                       
                       
                                             
                                                 
                                             
                       
                       
                                                    
                       
                       
                       
                                             
                                              
                                             
                       
                                          
                                                  
                       
                                                    
                       
                       
                                             
                                                 
                                   
   
                                                                           
                                                                          
                                                                       
                                                                         
                                                                               
                                                                             
                                                                         
                                                                        
                                                   
   
                                                                              
                                                                   
                                                                          
                                                                             
                                                                         
                                                                             
   
                                                                
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "miscadmin.h"

#include "access/heapam_xlog.h"
#include "access/transam.h"
#include "access/xact.h"

#include "pgstat.h"

#include "replication/logical.h"
#include "replication/reorderbuffer.h"
#include "replication/snapbuild.h"

#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapshot.h"
#include "utils/snapmgr.h"

#include "storage/block.h"                       
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/standby.h"

   
                                                                   
                                                                             
                                                        
   
struct SnapBuild
{
                                                             
  SnapBuildState state;

                                                                       
  MemoryContext context;

                                                           
  TransactionId xmin;

                                                     
  TransactionId xmax;

     
                                                                             
                                                                           
     
  XLogRecPtr start_decoding_at;

     
                                                                       
                                                                        
     
  TransactionId initial_xmin_horizon;

                                                                       
  bool building_full_snapshot;

     
                                                                         
     
  Snapshot snapshot;

     
                                                                             
     
  XLogRecPtr last_serialized_snapshot;

     
                                                                      
     
  ReorderBuffer *reorder;

     
                                                                            
                                                                        
              
     
  struct
  {
       
                                                                          
                                                   
                                    
       
    TransactionId was_xmin;
    TransactionId was_xmax;

    size_t was_xcnt;                                        
    size_t was_xcnt_space;                             
    TransactionId *was_xip;                                                
  } was_running;

     
                                                                           
                            
     
  struct
  {
                                          
    size_t xcnt;

                                                    
    size_t xcnt_space;

       
                                                                   
                                                                          
                                                                      
       
    bool includes_all_transactions;

       
                                                                       
       
                                                                   
                                                                      
                                
       
                                                                       
                                                                      
                                                                  
                                                                          
                                                                      
                                      
       
    TransactionId *xip;
  } committed;
};

   
                                                                               
                                                                             
   
static ResourceOwner SavedResourceOwnerDuringExport = NULL;
static bool ExportInProgress = false;

   
                                                                    
                                                                         
                                                                      
                                                                           
                                                                           
                            
   
                                                                           
                                                                            
                                                                            
                                                                           
                                                                              
                   
   
                                                                             
                                                                            
                                                                       
   
                                                                     
                                                                       
                                                                      
                                                                        
                                                                   
                                                                        
                                                                        
   
static TransactionId *InitialRunningXacts = NULL;
static int NInitialRunningXacts = 0;

                                                      
static void
SnapBuildPurgeOlderTxn(SnapBuild *builder);

                                                           
static Snapshot
SnapBuildBuildSnapshot(SnapBuild *builder);

static void
SnapBuildFreeSnapshot(Snapshot snap);

static void
SnapBuildSnapIncRefcount(Snapshot snap);

static void
SnapBuildDistributeNewCatalogSnapshot(SnapBuild *builder, XLogRecPtr lsn);

                                                              
static bool
SnapBuildFindSnapshot(SnapBuild *builder, XLogRecPtr lsn, xl_running_xacts *running);
static void
SnapBuildWaitSnapshot(xl_running_xacts *running, TransactionId cutoff);

                             
static void
SnapBuildSerialize(SnapBuild *builder, XLogRecPtr lsn);
static bool
SnapBuildRestore(SnapBuild *builder, XLogRecPtr lsn);

   
                                                                       
                         
   
static inline TransactionId
SnapBuildNextPhaseAt(SnapBuild *builder)
{
     
                                                                             
                                                        
     
  return builder->was_running.was_xmax;
}

   
                                                                             
                
   
static inline void
SnapBuildStartNextPhaseAt(SnapBuild *builder, TransactionId at)
{
     
                                                                             
                                                        
     
  builder->was_running.was_xmax = at;
}

   
                                    
   
                                                                             
                                                               
   
SnapBuild *
AllocateSnapshotBuilder(ReorderBuffer *reorder, TransactionId xmin_horizon, XLogRecPtr start_lsn, bool need_full_snapshot)
{
  MemoryContext context;
  MemoryContext oldcontext;
  SnapBuild *builder;

                                                                     
  context = AllocSetContextCreate(CurrentMemoryContext, "snapshot builder context", ALLOCSET_DEFAULT_SIZES);
  oldcontext = MemoryContextSwitchTo(context);

  builder = palloc0(sizeof(SnapBuild));

  builder->state = SNAPBUILD_START;
  builder->context = context;
  builder->reorder = reorder;
                                                                     

  builder->committed.xcnt = 0;
  builder->committed.xcnt_space = 128;                       
  builder->committed.xip = palloc0(builder->committed.xcnt_space * sizeof(TransactionId));
  builder->committed.includes_all_transactions = true;

  builder->initial_xmin_horizon = xmin_horizon;
  builder->start_decoding_at = start_lsn;
  builder->building_full_snapshot = need_full_snapshot;

  MemoryContextSwitchTo(oldcontext);

                                                             
  Assert(NInitialRunningXacts == 0 && InitialRunningXacts == NULL);

  return builder;
}

   
                            
   
void
FreeSnapshotBuilder(SnapBuild *builder)
{
  MemoryContext context = builder->context;

                                                                   
  if (builder->snapshot != NULL)
  {
    SnapBuildSnapDecRefcount(builder->snapshot);
    builder->snapshot = NULL;
  }

                                                                
  MemoryContextDelete(context);

                                                           
  NInitialRunningXacts = 0;
  InitialRunningXacts = NULL;
}

   
                                                                       
   
static void
SnapBuildFreeSnapshot(Snapshot snap)
{
                                                          
  Assert(snap->snapshot_type == SNAPSHOT_HISTORIC_MVCC);

                                              
  Assert(snap->curcid == FirstCommandId);
  Assert(!snap->suboverflowed);
  Assert(!snap->takenDuringRecovery);
  Assert(snap->regd_count == 0);

                                                                    
  if (snap->copied)
  {
    elog(ERROR, "cannot free a copied snapshot");
  }

  if (snap->active_count)
  {
    elog(ERROR, "cannot free an active snapshot");
  }

  pfree(snap);
}

   
                                               
   
SnapBuildState
SnapBuildCurrentState(SnapBuild *builder)
{
  return builder->state;
}

   
                                                                  
   
bool
SnapBuildXactNeedsSkip(SnapBuild *builder, XLogRecPtr ptr)
{
  return ptr < builder->start_decoding_at;
}

   
                                    
   
                                                                              
                                           
   
static void
SnapBuildSnapIncRefcount(Snapshot snap)
{
  snap->active_count++;
}

   
                                                                          
   
                                                                           
                                                      
   
void
SnapBuildSnapDecRefcount(Snapshot snap)
{
                                                          
  Assert(snap->snapshot_type == SNAPSHOT_HISTORIC_MVCC);

                                              
  Assert(snap->curcid == FirstCommandId);
  Assert(!snap->suboverflowed);
  Assert(!snap->takenDuringRecovery);

  Assert(snap->regd_count == 0);

  Assert(snap->active_count > 0);

                                                                   
  if (snap->copied)
  {
    elog(ERROR, "cannot free a copied snapshot");
  }

  snap->active_count--;
  if (snap->active_count == 0)
  {
    SnapBuildFreeSnapshot(snap);
  }
}

   
                                                                        
                 
   
                                                                            
                                                                            
                                
   
static Snapshot
SnapBuildBuildSnapshot(SnapBuild *builder)
{
  Snapshot snapshot;
  Size ssize;

  Assert(builder->state >= SNAPBUILD_FULL_SNAPSHOT);

  ssize = sizeof(SnapshotData) + sizeof(TransactionId) * builder->committed.xcnt + sizeof(TransactionId) * 1                   ;

  snapshot = MemoryContextAllocZero(builder->context, ssize);

  snapshot->snapshot_type = SNAPSHOT_HISTORIC_MVCC;

     
                                                                            
                                             
     
                                                                         
                                                                         
                                                                           
                                                                   
     
                                                                            
                                                                         
                                                                           
                                                                           
                                                                      
                                                                       
                                                                           
                                                                           
                          
     
                                                                    
     
  Assert(TransactionIdIsNormal(builder->xmin));
  Assert(TransactionIdIsNormal(builder->xmax));

  snapshot->xmin = builder->xmin;
  snapshot->xmax = builder->xmax;

                                                                          
  snapshot->xip = (TransactionId *)((char *)snapshot + sizeof(SnapshotData));
  snapshot->xcnt = builder->committed.xcnt;
  memcpy(snapshot->xip, builder->committed.xip, builder->committed.xcnt * sizeof(TransactionId));

                                
  qsort(snapshot->xip, snapshot->xcnt, sizeof(TransactionId), xidComparator);

     
                                                                    
                                                                   
                                           
     
  snapshot->subxcnt = 0;
  snapshot->subxip = NULL;

  snapshot->suboverflowed = false;
  snapshot->takenDuringRecovery = false;
  snapshot->copied = false;
  snapshot->curcid = FirstCommandId;
  snapshot->active_count = 0;
  snapshot->regd_count = 0;

  return snapshot;
}

   
                                                                            
                                            
   
                                                                           
                                         
   
Snapshot
SnapBuildInitialSnapshot(SnapBuild *builder)
{
  Snapshot snap;
  TransactionId xid;
  TransactionId *newxip;
  int newxcnt = 0;

  Assert(!FirstSnapshotSet);
  Assert(XactIsoLevel == XACT_REPEATABLE_READ);

  if (builder->state != SNAPBUILD_CONSISTENT)
  {
    elog(ERROR, "cannot build an initial slot snapshot before reaching a consistent state");
  }

  if (!builder->committed.includes_all_transactions)
  {
    elog(ERROR, "cannot build an initial slot snapshot, not all transactions are monitored anymore");
  }

                                                
  if (TransactionIdIsValid(MyPgXact->xmin))
  {
    elog(ERROR, "cannot build an initial slot snapshot when MyPgXact->xmin already is valid");
  }

  snap = SnapBuildBuildSnapshot(builder);

     
                                                                    
                                                                     
                             
     
#ifdef USE_ASSERT_CHECKING
  {
    TransactionId safeXid;

    LWLockAcquire(ProcArrayLock, LW_SHARED);
    safeXid = GetOldestSafeDecodingTransactionId(false);
    LWLockRelease(ProcArrayLock);

    Assert(TransactionIdPrecedesOrEquals(safeXid, snap->xmin));
  }
#endif

  MyPgXact->xmin = snap->xmin;

                                       
  newxip = (TransactionId *)palloc(sizeof(TransactionId) * GetMaxSnapshotXidCount());

     
                                                                             
                                                                           
                                                                     
                                         
     
  for (xid = snap->xmin; NormalTransactionIdPrecedes(xid, snap->xmax);)
  {
    void *test;

       
                                                                       
                         
       
    test = bsearch(&xid, snap->xip, snap->xcnt, sizeof(TransactionId), xidComparator);

    if (test == NULL)
    {
      if (newxcnt >= GetMaxSnapshotXidCount())
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("initial slot snapshot too large")));
      }

      newxip[newxcnt++] = xid;
    }

    TransactionIdAdvance(xid);
  }

                                                  
  snap->snapshot_type = SNAPSHOT_MVCC;
  snap->xcnt = newxcnt;
  snap->xip = newxip;

  return snap;
}

   
                                                                              
             
   
                                                                         
                                                                              
                                                     
   
const char *
SnapBuildExportSnapshot(SnapBuild *builder)
{
  Snapshot snap;
  char *snapname;

  if (IsTransactionOrTransactionBlock())
  {
    elog(ERROR, "cannot export a snapshot from within a transaction");
  }

  if (SavedResourceOwnerDuringExport)
  {
    elog(ERROR, "can only export one snapshot at a time");
  }

  SavedResourceOwnerDuringExport = CurrentResourceOwner;
  ExportInProgress = true;

  StartTransactionCommand();

                                                     
  XactIsoLevel = XACT_REPEATABLE_READ;
  XactReadOnly = true;

  snap = SnapBuildInitialSnapshot(builder);

     
                                                                       
                                        
     
  snapname = ExportSnapshot(snap);

  ereport(LOG, (errmsg_plural("exported logical decoding snapshot: \"%s\" with %u transaction ID", "exported logical decoding snapshot: \"%s\" with %u transaction IDs", snap->xcnt, snapname, snap->xcnt)));
  return snapname;
}

   
                                                                            
   
Snapshot
SnapBuildGetOrBuildSnapshot(SnapBuild *builder, TransactionId xid)
{
  Assert(builder->state == SNAPBUILD_CONSISTENT);

                                                                 
  if (builder->snapshot == NULL)
  {
    builder->snapshot = SnapBuildBuildSnapshot(builder);
                                                    
    SnapBuildSnapIncRefcount(builder->snapshot);
  }

  return builder->snapshot;
}

   
                                                                        
                                                                          
                                     
   
void
SnapBuildClearExportedSnapshot(void)
{
  ResourceOwner tmpResOwner;

                                                
  if (!ExportInProgress)
  {
    return;
  }

  if (!IsTransactionState())
  {
    elog(ERROR, "clearing exported snapshot in wrong transaction state");
  }

     
                                                                           
                                                 
     
  tmpResOwner = SavedResourceOwnerDuringExport;

                                                  
  AbortCurrentTransaction();

  CurrentResourceOwner = tmpResOwner;
}

   
                                                         
   
void
SnapBuildResetExportedSnapshotState(void)
{
  SavedResourceOwnerDuringExport = NULL;
  ExportInProgress = false;
}

   
                                                                                
                                                                              
               
   
bool
SnapBuildProcessChange(SnapBuild *builder, TransactionId xid, XLogRecPtr lsn)
{
     
                                                                         
                               
     
  if (builder->state < SNAPBUILD_FULL_SNAPSHOT)
  {
    return false;
  }

     
                                                                             
                                                                             
                                                    
     
  if (builder->state < SNAPBUILD_CONSISTENT && TransactionIdPrecedes(xid, SnapBuildNextPhaseAt(builder)))
  {
    return false;
  }

     
                                                                            
                                                                
     
  if (!ReorderBufferXidHasBaseSnapshot(builder->reorder, xid))
  {
                                                                   
    if (builder->snapshot == NULL)
    {
      builder->snapshot = SnapBuildBuildSnapshot(builder);
                                                      
      SnapBuildSnapIncRefcount(builder->snapshot);
    }

       
                                                                        
               
       
    SnapBuildSnapIncRefcount(builder->snapshot);
    ReorderBufferSetBaseSnapshot(builder->reorder, xid, lsn, builder->snapshot);
  }

  return true;
}

   
                                                                           
                                                                         
             
   
void
SnapBuildProcessNewCid(SnapBuild *builder, TransactionId xid, XLogRecPtr lsn, xl_heap_new_cid *xlrec)
{
  CommandId cid;

     
                                                                        
                                                     
     
  ReorderBufferXidSetCatalogChanges(builder->reorder, xid, lsn);

  ReorderBufferAddNewTupleCids(builder->reorder, xlrec->top_xid, lsn, xlrec->target_node, xlrec->target_tid, xlrec->cmin, xlrec->cmax, xlrec->combocid);

                                 
  if (xlrec->cmin != InvalidCommandId && xlrec->cmax != InvalidCommandId)
  {
    cid = Max(xlrec->cmin, xlrec->cmax);
  }
  else if (xlrec->cmax != InvalidCommandId)
  {
    cid = xlrec->cmax;
  }
  else if (xlrec->cmin != InvalidCommandId)
  {
    cid = xlrec->cmin;
  }
  else
  {
    cid = InvalidCommandId;                       
    elog(ERROR, "xl_heap_new_cid record without a valid CommandId");
  }

  ReorderBufferAddNewCommandId(builder->reorder, xid, lsn, cid + 1);
}

   
                                                                            
                                                                            
                                                                    
                                                                              
                                                                      
              
   
static void
SnapBuildDistributeNewCatalogSnapshot(SnapBuild *builder, XLogRecPtr lsn)
{
  dlist_iter txn_i;
  ReorderBufferTXN *txn;

     
                                                                 
                                                                         
                                                              
     
  dlist_foreach(txn_i, &builder->reorder->toplevel_by_lsn)
  {
    txn = dlist_container(ReorderBufferTXN, node, txn_i.cur);

    Assert(TransactionIdIsValid(txn->xid));

       
                                                                          
                                                                         
                                                                    
       
                                                                 
                                                                           
                                                                         
                                             
       
    if (!ReorderBufferXidHasBaseSnapshot(builder->reorder, txn->xid))
    {
      continue;
    }

    elog(DEBUG2, "adding a new snapshot to %u at %X/%X", txn->xid, (uint32)(lsn >> 32), (uint32)lsn);

       
                                                                           
                 
       
    SnapBuildSnapIncRefcount(builder->snapshot);
    ReorderBufferAddSnapshot(builder->reorder, txn->xid, lsn, builder->snapshot);
  }
}

   
                                                                        
   
static void
SnapBuildAddCommittedTxn(SnapBuild *builder, TransactionId xid)
{
  Assert(TransactionIdIsValid(xid));

  if (builder->committed.xcnt == builder->committed.xcnt_space)
  {
    builder->committed.xcnt_space = builder->committed.xcnt_space * 2 + 1;

    elog(DEBUG1, "increasing space for committed transactions to %u", (uint32)builder->committed.xcnt_space);

    builder->committed.xip = repalloc(builder->committed.xip, builder->committed.xcnt_space * sizeof(TransactionId));
  }

     
                                                                        
                                                                         
                                                                             
     
  builder->committed.xip[builder->committed.xcnt++] = xid;
}

   
                                                                             
                                                                           
                                                                           
                                                               
   
                                                                        
                                                                               
                                            
   
static void
SnapBuildPurgeOlderTxn(SnapBuild *builder)
{
  int off;
  TransactionId *workspace;
  int surviving_xids = 0;

                     
  if (!TransactionIdIsNormal(builder->xmin))
  {
    return;
  }

                                                               
  workspace = MemoryContextAlloc(builder->context, builder->committed.xcnt * sizeof(TransactionId));

                                                         
  for (off = 0; off < builder->committed.xcnt; off++)
  {
    if (NormalTransactionIdPrecedes(builder->committed.xip[off], builder->xmin))
      ;             
    else
    {
      workspace[surviving_xids++] = builder->committed.xip[off];
    }
  }

                                               
  memcpy(builder->committed.xip, workspace, surviving_xids * sizeof(TransactionId));

  elog(DEBUG3, "purged committed transactions from %u to %u, xmin: %u, xmax: %u", (uint32)builder->committed.xcnt, (uint32)surviving_xids, builder->xmin, builder->xmax);
  builder->committed.xcnt = surviving_xids;

  pfree(workspace);

                                                              
  if (NInitialRunningXacts == 0)
  {
    return;
  }

                                                                  
  if (!NormalTransactionIdPrecedes(InitialRunningXacts[0], builder->xmin))
  {
    return;
  }

     
                                                                           
                                       
     
  workspace = MemoryContextAlloc(builder->context, NInitialRunningXacts * sizeof(TransactionId));
  surviving_xids = 0;
  for (off = 0; off < NInitialRunningXacts; off++)
  {
    if (NormalTransactionIdPrecedes(InitialRunningXacts[off], builder->xmin))
      ;             
    else
    {
      workspace[surviving_xids++] = InitialRunningXacts[off];
    }
  }

  if (surviving_xids > 0)
  {
    memcpy(InitialRunningXacts, workspace, sizeof(TransactionId) * surviving_xids);
  }
  else
  {
    pfree(InitialRunningXacts);
    InitialRunningXacts = NULL;
  }

  elog(DEBUG3, "purged initial running transactions from %u to %u, oldest running xid %u", (uint32)NInitialRunningXacts, (uint32)surviving_xids, builder->xmin);

  NInitialRunningXacts = surviving_xids;
  pfree(workspace);
}

   
                                                                      
   
void
SnapBuildCommitTxn(SnapBuild *builder, XLogRecPtr lsn, TransactionId xid, int nsubxacts, TransactionId *subxacts)
{
  int nxact;

  bool needs_snapshot = false;
  bool needs_timetravel = false;
  bool sub_needs_timetravel = false;

  TransactionId xmax = xid;

     
                                                                           
                                                                            
     
  if (builder->state == SNAPBUILD_START || (builder->state == SNAPBUILD_BUILDING_SNAPSHOT && TransactionIdPrecedes(xid, SnapBuildNextPhaseAt(builder))))
  {
                                                                  
    if (builder->start_decoding_at <= lsn)
    {
      builder->start_decoding_at = lsn + 1;
    }
    return;
  }

  if (builder->state < SNAPBUILD_CONSISTENT)
  {
                                                                  
    if (builder->start_decoding_at <= lsn)
    {
      builder->start_decoding_at = lsn + 1;
    }

       
                                                                         
                                                     
       
    if (builder->building_full_snapshot)
    {
      needs_timetravel = true;
    }
  }

  for (nxact = 0; nxact < nsubxacts; nxact++)
  {
    TransactionId subxid = subxacts[nxact];

       
                                                                          
                                                   
       
    if (ReorderBufferXidHasCatalogChanges(builder->reorder, subxid))
    {
      sub_needs_timetravel = true;
      needs_snapshot = true;

      elog(DEBUG1, "found subtransaction %u:%u with catalog changes", xid, subxid);

      SnapBuildAddCommittedTxn(builder, subxid);

      if (NormalTransactionIdFollows(subxid, xmax))
      {
        xmax = subxid;
      }
    }

       
                                                                       
                                                                           
                                                                         
                  
       
    else if (needs_timetravel)
    {
      SnapBuildAddCommittedTxn(builder, subxid);
      if (NormalTransactionIdFollows(subxid, xmax))
      {
        xmax = subxid;
      }
    }
  }

                                                            
  if (ReorderBufferXidHasCatalogChanges(builder->reorder, xid))
  {
    elog(DEBUG2, "found top level transaction %u, with catalog changes", xid);
    needs_snapshot = true;
    needs_timetravel = true;
    SnapBuildAddCommittedTxn(builder, xid);
  }
  else if (sub_needs_timetravel)
  {
                                                                    
    elog(DEBUG2, "forced transaction %u to do timetravel due to one of its subtransactions", xid);
    needs_timetravel = true;
    SnapBuildAddCommittedTxn(builder, xid);
  }
  else if (needs_timetravel)
  {
    elog(DEBUG2, "forced transaction %u to do timetravel", xid);

    SnapBuildAddCommittedTxn(builder, xid);
  }

  if (!needs_timetravel)
  {
                                                                 
    builder->committed.includes_all_transactions = false;
  }

  Assert(!needs_snapshot || needs_timetravel);

     
                                                                         
                                                                            
                                                       
     
  if (needs_timetravel && (!TransactionIdIsValid(builder->xmax) || TransactionIdFollowsOrEquals(xmax, builder->xmax)))
  {
    builder->xmax = xmax;
    TransactionIdAdvance(builder->xmax);
  }

                                                                     
  if (needs_snapshot)
  {
       
                                                                           
                                                          
       
    if (builder->state < SNAPBUILD_FULL_SNAPSHOT)
    {
      return;
    }

       
                                                                          
                                                                   
                              
       
    if (builder->snapshot)
    {
      SnapBuildSnapDecRefcount(builder->snapshot);
    }

    builder->snapshot = SnapBuildBuildSnapshot(builder);

                                                              
    if (!ReorderBufferXidHasBaseSnapshot(builder->reorder, xid))
    {
      SnapBuildSnapIncRefcount(builder->snapshot);
      ReorderBufferSetBaseSnapshot(builder->reorder, xid, lsn, builder->snapshot);
    }

                                                               
    SnapBuildSnapIncRefcount(builder->snapshot);

                                                                          
    SnapBuildDistributeNewCatalogSnapshot(builder, lsn);
  }
}

                                       
                                                         
                                       
   

   
                                                                            
                                                                       
            
   
void
SnapBuildProcessRunningXacts(SnapBuild *builder, XLogRecPtr lsn, xl_running_xacts *running)
{
  ReorderBufferTXN *txn;
  TransactionId xmin;

     
                                                                       
                                                                          
                                                                
     
  if (builder->state < SNAPBUILD_CONSISTENT)
  {
                                                                          
    if (!SnapBuildFindSnapshot(builder, lsn, running))
    {
      return;
    }
  }
  else
  {
    SnapBuildSerialize(builder, lsn);
  }

     
                                                                 
                                                                            
                                                                           
                                                                         
                              
     
                                                                            
                                                                        
                                                                             
                                               
     
  builder->xmin = running->oldestRunningXid;

                                                                   
  SnapBuildPurgeOlderTxn(builder);

     
                                                                       
                                                                  
     
                                                                      
                                                                           
                                                                             
                                   
     
  xmin = ReorderBufferGetOldestXmin(builder->reorder);
  if (xmin == InvalidTransactionId)
  {
    xmin = running->oldestRunningXid;
  }
  elog(DEBUG3, "xmin: %u, xmax: %u, oldest running: %u, oldest xmin: %u", builder->xmin, builder->xmax, running->oldestRunningXid, xmin);
  LogicalIncreaseXminForSlot(lsn, xmin);

     
                                                                             
                                                                          
                                                                         
                           
     
                                                                            
                                                                      
                                                                           
                                                         
     

     
                                                                    
                 
     
  if (builder->state < SNAPBUILD_CONSISTENT)
  {
    return;
  }

  txn = ReorderBufferGetOldestTXN(builder->reorder);

     
                                                                        
                                                                
     
  if (txn != NULL && txn->restart_decoding_lsn != InvalidXLogRecPtr)
  {
    LogicalIncreaseRestartDecodingForSlot(lsn, txn->restart_decoding_lsn);
  }

     
                                                                           
                  
     
  else if (txn == NULL && builder->reorder->current_restart_decoding_lsn != InvalidXLogRecPtr && builder->last_serialized_snapshot != InvalidXLogRecPtr)
  {
    LogicalIncreaseRestartDecodingForSlot(lsn, builder->last_serialized_snapshot);
  }
}

   
                                                                         
   
                                                                          
               
   
                                                                               
                                      
   
static bool
SnapBuildFindSnapshot(SnapBuild *builder, XLogRecPtr lsn, xl_running_xacts *running)
{
         
                                                                           
                                                                            
     
                                                                            
                                                                          
                                               
     
                                                                       
                                                                        
                                                                           
                                                                            
                               
     
                                                                
                                                                    
                                                                      
                                                                         
                                                                        
                                                                        
         
     

     
                                                                             
                                         
     
  if (TransactionIdIsNormal(builder->initial_xmin_horizon) && NormalTransactionIdPrecedes(running->oldestRunningXid, builder->initial_xmin_horizon))
  {
    ereport(DEBUG1, (errmsg_internal("skipping snapshot at %X/%X while building logical decoding snapshot, xmin horizon too low", (uint32)(lsn >> 32), (uint32)lsn), errdetail_internal("initial xmin horizon of %u vs the snapshot's %u", builder->initial_xmin_horizon, running->oldestRunningXid)));

    SnapBuildWaitSnapshot(running, builder->initial_xmin_horizon);

    return true;
  }

     
                                                                
     
                                                                           
                                                                        
     
                                                                             
                                                 
     
  if (running->oldestRunningXid == running->nextXid)
  {
    if (builder->start_decoding_at == InvalidXLogRecPtr || builder->start_decoding_at <= lsn)
    {
                                            
      builder->start_decoding_at = lsn + 1;
    }

                                                                         
    builder->xmin = running->nextXid;                     
    builder->xmax = running->nextXid;                     

                                                     
    Assert(TransactionIdIsNormal(builder->xmin));
    Assert(TransactionIdIsNormal(builder->xmax));

    builder->state = SNAPBUILD_CONSISTENT;
    SnapBuildStartNextPhaseAt(builder, InvalidTransactionId);

    ereport(LOG, (errmsg("logical decoding found consistent point at %X/%X", (uint32)(lsn >> 32), (uint32)lsn), errdetail("There are no running transactions.")));

    return false;
  }
                                                             
  else if (!builder->building_full_snapshot && SnapBuildRestore(builder, lsn))
  {
    int nxacts = running->subxcnt + running->xcnt;
    Size sz = sizeof(TransactionId) * nxacts;

       
                                                                       
                                                                        
                                                                      
                                                   
       
    NInitialRunningXacts = nxacts;
    InitialRunningXacts = MemoryContextAlloc(builder->context, sz);
    memcpy(InitialRunningXacts, running->xids, sz);
    qsort(InitialRunningXacts, nxacts, sizeof(TransactionId), xidComparator);

                                             
    return false;
  }

     
                                                    
     
                                                                         
                                                                        
                                                                             
                                                                             
                                                                          
                                                                            
                                                                        
                                             
     
  else if (builder->state == SNAPBUILD_START)
  {
    builder->state = SNAPBUILD_BUILDING_SNAPSHOT;
    SnapBuildStartNextPhaseAt(builder, running->nextXid);

       
                                                                       
                                                                       
                                                             
       
    builder->xmin = running->nextXid;                     
    builder->xmax = running->nextXid;                     

                                                     
    Assert(TransactionIdIsNormal(builder->xmin));
    Assert(TransactionIdIsNormal(builder->xmax));

    ereport(LOG, (errmsg("logical decoding found initial starting point at %X/%X", (uint32)(lsn >> 32), (uint32)lsn), errdetail("Waiting for transactions (approximately %d) older than %u to end.", running->xcnt, running->nextXid)));

    SnapBuildWaitSnapshot(running, running->nextXid);
  }

     
                                                            
     
                                                                             
                                                                          
                                                                           
                                           
     
  else if (builder->state == SNAPBUILD_BUILDING_SNAPSHOT && TransactionIdPrecedesOrEquals(SnapBuildNextPhaseAt(builder), running->oldestRunningXid))
  {
    builder->state = SNAPBUILD_FULL_SNAPSHOT;
    SnapBuildStartNextPhaseAt(builder, running->nextXid);

    ereport(LOG, (errmsg("logical decoding found initial consistent point at %X/%X", (uint32)(lsn >> 32), (uint32)lsn), errdetail("Waiting for transactions (approximately %d) older than %u to end.", running->xcnt, running->nextXid)));

    SnapBuildWaitSnapshot(running, running->nextXid);
  }

     
                                                     
     
                                                                  
                                                                  
                                                                       
                                                                       
                                       
     
  else if (builder->state == SNAPBUILD_FULL_SNAPSHOT && TransactionIdPrecedesOrEquals(SnapBuildNextPhaseAt(builder), running->oldestRunningXid))
  {
    builder->state = SNAPBUILD_CONSISTENT;
    SnapBuildStartNextPhaseAt(builder, InvalidTransactionId);

    ereport(LOG, (errmsg("logical decoding found consistent point at %X/%X", (uint32)(lsn >> 32), (uint32)lsn), errdetail("There are no old transactions anymore.")));
  }

     
                                                                        
                                                                             
                                                      
     
  return true;
}

       
                                                                         
                                                                  
   
                                                                
                                                                       
                
                                                                              
                                            
       
   
static void
SnapBuildWaitSnapshot(xl_running_xacts *running, TransactionId cutoff)
{
  int off;

  for (off = 0; off < running->xcnt; off++)
  {
    TransactionId xid = running->xids[off];

       
                                                                           
                                                                      
                                            
       
    if (TransactionIdIsCurrentTransactionId(xid))
    {
      elog(ERROR, "waiting for ourselves");
    }

    if (TransactionIdFollows(xid, cutoff))
    {
      continue;
    }

    XactLockTableWait(xid, NULL, NULL, XLTW_None);
  }

     
                                                                            
                                                                           
                                                                        
                                                
     
  if (!RecoveryInProgress())
  {
    LogStandbySnapshot();
  }
}

                                       
                                  
                                       
   

   
                                                                               
   
                           
                                       
                                                      
   
   
typedef struct SnapBuildOnDisk
{
                                                                 

                                    
  uint32 magic;
  pg_crc32c checksum;

                                

                                                      
  uint32 version;
                                                                        
  uint32 length;

                              
  SnapBuild builder;

                                                 
} SnapBuildOnDisk;

#define SnapBuildOnDiskConstantSize offsetof(SnapBuildOnDisk, builder)
#define SnapBuildOnDiskNotChecksummedSize offsetof(SnapBuildOnDisk, version)

#define SNAPBUILD_MAGIC 0x51A1E001
#define SNAPBUILD_VERSION 2

   
                                                                               
   
                                                                              
                                                                   
   
void
SnapBuildSerializationPoint(SnapBuild *builder, XLogRecPtr lsn)
{
  if (builder->state < SNAPBUILD_CONSISTENT)
  {
    SnapBuildRestore(builder, lsn);
  }
  else
  {
    SnapBuildSerialize(builder, lsn);
  }
}

   
                                                                               
                                          
   
static void
SnapBuildSerialize(SnapBuild *builder, XLogRecPtr lsn)
{
  Size needed_length;
  SnapBuildOnDisk *ondisk = NULL;
  char *ondisk_c;
  int fd;
  char tmppath[MAXPGPATH];
  char path[MAXPGPATH];
  int ret;
  struct stat stat_buf;
  Size sz;

  Assert(lsn != InvalidXLogRecPtr);
  Assert(builder->last_serialized_snapshot == InvalidXLogRecPtr || builder->last_serialized_snapshot <= lsn);

     
                                                                             
                            
     
  if (builder->state < SNAPBUILD_CONSISTENT)
  {
    return;
  }

     
                                                                           
                                                                            
                                                                            
                                          
     
  sprintf(path, "pg_logical/snapshots/%X-%X.snap", (uint32)(lsn >> 32), (uint32)lsn);

     
                                                                             
                                                                            
                                                               
     
  ret = stat(path, &stat_buf);

  if (ret != 0 && errno != ENOENT)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not stat file \"%s\": %m", path)));
  }

  else if (ret == 0)
  {
       
                                                                           
                                                                       
       
                                                                         
                                                                         
                                                                          
                          
       
    fsync_fname(path, false);
    fsync_fname("pg_logical/snapshots", true);

    builder->last_serialized_snapshot = lsn;
    goto out;
  }

     
                                                                             
                                                                     
                                                                            
                                                                            
                                        
     
  elog(DEBUG1, "serializing snapshot to %s", path);

                                                                     
  sprintf(tmppath, "pg_logical/snapshots/%X-%X.snap.%u.tmp", (uint32)(lsn >> 32), (uint32)lsn, MyProcPid);

     
                                                                             
                                                                        
                                                                             
                          
     
  if (unlink(tmppath) != 0 && errno != ENOENT)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", tmppath)));
  }

  needed_length = sizeof(SnapBuildOnDisk) + sizeof(TransactionId) * builder->committed.xcnt;

  ondisk_c = MemoryContextAllocZero(builder->context, needed_length);
  ondisk = (SnapBuildOnDisk *)ondisk_c;
  ondisk->magic = SNAPBUILD_MAGIC;
  ondisk->version = SNAPBUILD_VERSION;
  ondisk->length = needed_length;
  INIT_CRC32C(ondisk->checksum);
  COMP_CRC32C(ondisk->checksum, ((char *)ondisk) + SnapBuildOnDiskNotChecksummedSize, SnapBuildOnDiskConstantSize - SnapBuildOnDiskNotChecksummedSize);
  ondisk_c += sizeof(SnapBuildOnDisk);

  memcpy(&ondisk->builder, builder, sizeof(SnapBuild));
                                 
  ondisk->builder.context = NULL;
  ondisk->builder.snapshot = NULL;
  ondisk->builder.reorder = NULL;
  ondisk->builder.committed.xip = NULL;

  COMP_CRC32C(ondisk->checksum, &ondisk->builder, sizeof(SnapBuild));

                                            
  Assert(builder->was_running.was_xcnt == 0);

                            
  sz = sizeof(TransactionId) * builder->committed.xcnt;
  memcpy(ondisk_c, builder->committed.xip, sz);
  COMP_CRC32C(ondisk->checksum, ondisk_c, sz);
  ondisk_c += sz;

  FIN_CRC32C(ondisk->checksum);

                                                                
  fd = OpenTransientFile(tmppath, O_CREAT | O_EXCL | O_WRONLY | PG_BINARY);
  if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", tmppath)));
  }

  errno = 0;
  pgstat_report_wait_start(WAIT_EVENT_SNAPBUILD_WRITE);
  if ((write(fd, ondisk, needed_length)) != needed_length)
  {
    int save_errno = errno;

    CloseTransientFile(fd);

                                                                    
    errno = save_errno ? save_errno : ENOSPC;
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not write to file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

     
                                                                           
                                                
     
                                                                           
                                     
     
                                                                           
                                                                        
               
     
  pgstat_report_wait_start(WAIT_EVENT_SNAPBUILD_SYNC);
  if (pg_fsync(fd) != 0)
  {
    int save_errno = errno;

    CloseTransientFile(fd);
    errno = save_errno;
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not fsync file \"%s\": %m", tmppath)));
  }
  pgstat_report_wait_end();

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", tmppath)));
  }

  fsync_fname("pg_logical/snapshots", true);

     
                                                                           
                                                                            
     
  if (rename(tmppath, path) != 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not rename file \"%s\" to \"%s\": %m", tmppath, path)));
  }

                            
  fsync_fname(path, false);
  fsync_fname("pg_logical/snapshots", true);

     
                                                                             
                               
     
  builder->last_serialized_snapshot = lsn;

out:
  ReorderBufferSetRestartPoint(builder->reorder, builder->last_serialized_snapshot);
               
  if (ondisk)
  {
    pfree(ondisk);
  }
}

   
                                                                              
                                                                             
   
static bool
SnapBuildRestore(SnapBuild *builder, XLogRecPtr lsn)
{
  SnapBuildOnDisk ondisk;
  int fd;
  char path[MAXPGPATH];
  Size sz;
  int readBytes;
  pg_crc32c checksum;

                                                             
  if (builder->state == SNAPBUILD_CONSISTENT)
  {
    return false;
  }

  sprintf(path, "pg_logical/snapshots/%X-%X.snap", (uint32)(lsn >> 32), (uint32)lsn);

  fd = OpenTransientFile(path, O_RDONLY | PG_BINARY);

  if (fd < 0 && errno == ENOENT)
  {
    return false;
  }
  else if (fd < 0)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not open file \"%s\": %m", path)));
  }

          
                                                                            
            
                                                                         
                                                                           
               
          
     
  fsync_fname(path, false);
  fsync_fname("pg_logical/snapshots", true);

                                                 
  pgstat_report_wait_start(WAIT_EVENT_SNAPBUILD_READ);
  readBytes = read(fd, &ondisk, SnapBuildOnDiskConstantSize);
  pgstat_report_wait_end();
  if (readBytes != SnapBuildOnDiskConstantSize)
  {
    int save_errno = errno;

    CloseTransientFile(fd);

    if (readBytes < 0)
    {
      errno = save_errno;
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, (Size)SnapBuildOnDiskConstantSize)));
    }
  }

  if (ondisk.magic != SNAPBUILD_MAGIC)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("snapbuild state file \"%s\" has wrong magic number: %u instead of %u", path, ondisk.magic, SNAPBUILD_MAGIC)));
  }

  if (ondisk.version != SNAPBUILD_VERSION)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("snapbuild state file \"%s\" has unsupported version: %u instead of %u", path, ondisk.version, SNAPBUILD_VERSION)));
  }

  INIT_CRC32C(checksum);
  COMP_CRC32C(checksum, ((char *)&ondisk) + SnapBuildOnDiskNotChecksummedSize, SnapBuildOnDiskConstantSize - SnapBuildOnDiskNotChecksummedSize);

                      
  pgstat_report_wait_start(WAIT_EVENT_SNAPBUILD_READ);
  readBytes = read(fd, &ondisk.builder, sizeof(SnapBuild));
  pgstat_report_wait_end();
  if (readBytes != sizeof(SnapBuild))
  {
    int save_errno = errno;

    CloseTransientFile(fd);

    if (readBytes < 0)
    {
      errno = save_errno;
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, sizeof(SnapBuild))));
    }
  }
  COMP_CRC32C(checksum, &ondisk.builder, sizeof(SnapBuild));

                                                                  
  sz = sizeof(TransactionId) * ondisk.builder.was_running.was_xcnt_space;
  ondisk.builder.was_running.was_xip = MemoryContextAllocZero(builder->context, sz);
  pgstat_report_wait_start(WAIT_EVENT_SNAPBUILD_READ);
  readBytes = read(fd, ondisk.builder.was_running.was_xip, sz);
  pgstat_report_wait_end();
  if (readBytes != sz)
  {
    int save_errno = errno;

    CloseTransientFile(fd);

    if (readBytes < 0)
    {
      errno = save_errno;
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, sz)));
    }
  }
  COMP_CRC32C(checksum, ondisk.builder.was_running.was_xip, sz);

                                           
  sz = sizeof(TransactionId) * ondisk.builder.committed.xcnt;
  ondisk.builder.committed.xip = MemoryContextAllocZero(builder->context, sz);
  pgstat_report_wait_start(WAIT_EVENT_SNAPBUILD_READ);
  readBytes = read(fd, ondisk.builder.committed.xip, sz);
  pgstat_report_wait_end();
  if (readBytes != sz)
  {
    int save_errno = errno;

    CloseTransientFile(fd);

    if (readBytes < 0)
    {
      errno = save_errno;
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read file \"%s\": %m", path)));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("could not read file \"%s\": read %d of %zu", path, readBytes, sz)));
    }
  }
  COMP_CRC32C(checksum, ondisk.builder.committed.xip, sz);

  if (CloseTransientFile(fd))
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not close file \"%s\": %m", path)));
  }

  FIN_CRC32C(checksum);

                                          
  if (!EQ_CRC32C(checksum, ondisk.checksum))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("checksum mismatch for snapbuild state file \"%s\": is %u, should be %u", path, checksum, ondisk.checksum)));
  }

     
                                                                         
                               
     

     
                                                                       
                                                                    
                            
     
  if (ondisk.builder.state < SNAPBUILD_CONSISTENT)
  {
    goto snapshot_not_interesting;
  }

     
                                                                            
                   
     
  if (TransactionIdPrecedes(ondisk.builder.xmin, builder->initial_xmin_horizon))
  {
    goto snapshot_not_interesting;
  }

                                                                             
  builder->xmin = ondisk.builder.xmin;
  builder->xmax = ondisk.builder.xmax;
  builder->state = ondisk.builder.state;

  builder->committed.xcnt = ondisk.builder.committed.xcnt;
                                                            
                                                                        
  if (builder->committed.xcnt > 0)
  {
    pfree(builder->committed.xip);
    builder->committed.xcnt_space = ondisk.builder.committed.xcnt;
    builder->committed.xip = ondisk.builder.committed.xip;
  }
  ondisk.builder.committed.xip = NULL;

                                                                
  if (builder->snapshot != NULL)
  {
    SnapBuildSnapDecRefcount(builder->snapshot);
  }
  builder->snapshot = SnapBuildBuildSnapshot(builder);
  SnapBuildSnapIncRefcount(builder->snapshot);

  ReorderBufferSetRestartPoint(builder->reorder, lsn);

  Assert(builder->state == SNAPBUILD_CONSISTENT);

  ereport(LOG, (errmsg("logical decoding found consistent point at %X/%X", (uint32)(lsn >> 32), (uint32)lsn), errdetail("Logical decoding will begin using saved snapshot.")));
  return true;

snapshot_not_interesting:
  if (ondisk.builder.committed.xip != NULL)
  {
    pfree(ondisk.builder.committed.xip);
  }
  return false;
}

   
                                                                            
                                                                              
                                                 
   
                                                                              
                                                             
   
void
CheckPointSnapBuild(void)
{
  XLogRecPtr cutoff;
  XLogRecPtr redo;
  DIR *snap_dir;
  struct dirent *snap_de;
  char path[MAXPGPATH + 21];

     
                                                                  
                                                                           
                  
     
  redo = GetRedoRecPtr();

                                                          
  cutoff = ReplicationSlotsComputeLogicalRestartLSN();

                                                
  if (redo < cutoff)
  {
    cutoff = redo;
  }

  snap_dir = AllocateDir("pg_logical/snapshots");
  while ((snap_de = ReadDir(snap_dir, "pg_logical/snapshots")) != NULL)
  {
    uint32 hi;
    uint32 lo;
    XLogRecPtr lsn;
    struct stat statbuf;

    if (strcmp(snap_de->d_name, ".") == 0 || strcmp(snap_de->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(path, sizeof(path), "pg_logical/snapshots/%s", snap_de->d_name);

    if (lstat(path, &statbuf) == 0 && !S_ISREG(statbuf.st_mode))
    {
      elog(DEBUG1, "only regular files expected: %s", path);
      continue;
    }

       
                                                                         
                                                                          
                                                                  
                                                           
       
                                                                     
                                                           
       
    if (sscanf(snap_de->d_name, "%X-%X.snap", &hi, &lo) != 2)
    {
      ereport(LOG, (errmsg("could not parse file name \"%s\"", path)));
      continue;
    }

    lsn = ((uint64)hi) << 32 | lo;

                                        
    if (lsn < cutoff || cutoff == InvalidXLogRecPtr)
    {
      elog(DEBUG1, "removing snapbuild snapshot %s", path);

         
                                                                    
                                                                 
                                                              
         
      if (unlink(path) < 0)
      {
        ereport(LOG, (errcode_for_file_access(), errmsg("could not remove file \"%s\": %m", path)));
        continue;
      }
    }
  }
  FreeDir(snap_dir);
}

   
                                                                           
                                                                      
                                                                      
                                            
   
void
SnapBuildXidSetCatalogChanges(SnapBuild *builder, TransactionId xid, int subxcnt, TransactionId *subxacts, XLogRecPtr lsn)
{
  ReorderBufferXidSetCatalogChanges(builder->reorder, xid, lsn);

                                                             
  if (NInitialRunningXacts == 0)
  {
    return;
  }

  if (bsearch(&xid, InitialRunningXacts, NInitialRunningXacts, sizeof(TransactionId), xidComparator) != NULL)
  {
    for (int i = 0; i < subxcnt; i++)
    {
      ReorderBufferXidSetCatalogChanges(builder->reorder, subxacts[i], lsn);
    }
  }
}
