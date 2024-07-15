                                                                            
   
               
                                
                                                        
   
   
                                                                            
                                         
   
                                                          
                                                  
                                                     
                                                   
                                          
                                              
   
                                                       
   
                               
                                                  
                          
                                                             
                                   
   
   
                                                                        
                                                                          
                                                                     
                                                                         
                                                                     
                             
   
                                                                      
                                                                  
                                                                    
                                                           
   
                                                                       
                                                                      
                                                                
                                                                      
                                                                     
                                        
   
                                                                      
                                                                    
                                                           
   
                                                                       
                                                                  
                                                                  
                                                                 
                                                                     
                                                                   
                                  
   
                                                                    
                                                       
   
                                                                       
                                                                      
                    
   
                                                                        
                                                                     
                                                                     
                                                             
              
   
                                                                    
                                                                     
                                                                      
                                                               
                                                                    
                                                    
   
                                                                     
                                                                  
                                                                       
                                                                    
   
   
                                                                      
                                                                         
                  
   
                                
                                                                       
                                                                    
   
                                     
                                                                     
                                                                 
                                                                    
                                                                    
                                                                      
                                                                     
                                                                  
                                                                     
                                                                    
                       
                                                                
                                                                    
                                                                    
                                                                  
                                                                 
                                                                
                                                                   
                                                                    
                                                                     
                                                                    
                                                                     
                                                                 
                                                                 
                                     
                                                                     
                                                               
           
   
                                                     
                                                                     
                                                                 
                                                  
                                                            
   
                                            
                                                                     
                                             
                                                                        
                                                                        
                                                   
   
                            
                                                      
   
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
   
                      
   
                                                                       
                             
                                 
   
                            
                                     
                                                                
   
                              
                                                          
                                                          
                                                 
                                      
                                                                
                                                            
                           
                                                           
                           
                                                                    
                                  
                                                                      
                                 
                                                            
                                                              
   
                                                  
                                                                     
                                               
                               
                                                                          
                             
                                                           
   
                           
                                                 
   
                            
                                    
                                                   
                                                                   
                                                                   
                                          
   

#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/slru.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/predicate.h"
#include "storage/predicate_internals.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"

                                                                    
                            

   
                                                          
   
                                                   
                              
                              
                                                            
                               
                                                                       
                                                           
                                                    
   
#define TargetTagIsCoveredBy(covered_target, covering_target)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  ((GET_PREDICATELOCKTARGETTAG_RELATION(covered_target) ==                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
       GET_PREDICATELOCKTARGETTAG_RELATION(covering_target)) &&                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      (GET_PREDICATELOCKTARGETTAG_OFFSET(covering_target) == InvalidOffsetNumber)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      && (((GET_PREDICATELOCKTARGETTAG_OFFSET(covered_target) != InvalidOffsetNumber)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
              && (GET_PREDICATELOCKTARGETTAG_PAGE(covering_target) == GET_PREDICATELOCKTARGETTAG_PAGE(covered_target))) ||                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
             ((GET_PREDICATELOCKTARGETTAG_PAGE(covering_target) == InvalidBlockNumber)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
                 && (GET_PREDICATELOCKTARGETTAG_PAGE(covered_target) != InvalidBlockNumber))) &&                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
      (GET_PREDICATELOCKTARGETTAG_DB(covered_target) ==                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
          GET_PREDICATELOCKTARGETTAG_DB(covering_target)))

   
                                                                               
                                                                               
                                                                           
                              
                                                          
   
#define PredicateLockHashPartition(hashcode) ((hashcode) % NUM_PREDICATELOCK_PARTITIONS)
#define PredicateLockHashPartitionLock(hashcode) (&MainLWLockArray[PREDICATELOCK_MANAGER_LWLOCK_OFFSET + PredicateLockHashPartition(hashcode)].lock)
#define PredicateLockHashPartitionLockByIndex(i) (&MainLWLockArray[PREDICATELOCK_MANAGER_LWLOCK_OFFSET + (i)].lock)

#define NPREDICATELOCKTARGETENTS() mul_size(max_predicate_locks_per_xact, add_size(MaxBackends, max_prepared_xacts))

#define SxactIsOnFinishedList(sxact) (!SHMQueueIsDetached(&((sxact)->finishedLink)))

   
                                                             
                                                                  
                                                                
   
                                                                   
                            
   
#define SxactIsCommitted(sxact) (((sxact)->flags & SXACT_FLAG_COMMITTED) != 0)
#define SxactIsPrepared(sxact) (((sxact)->flags & SXACT_FLAG_PREPARED) != 0)
#define SxactIsRolledBack(sxact) (((sxact)->flags & SXACT_FLAG_ROLLED_BACK) != 0)
#define SxactIsDoomed(sxact) (((sxact)->flags & SXACT_FLAG_DOOMED) != 0)
#define SxactIsReadOnly(sxact) (((sxact)->flags & SXACT_FLAG_READ_ONLY) != 0)
#define SxactHasSummaryConflictIn(sxact) (((sxact)->flags & SXACT_FLAG_SUMMARY_CONFLICT_IN) != 0)
#define SxactHasSummaryConflictOut(sxact) (((sxact)->flags & SXACT_FLAG_SUMMARY_CONFLICT_OUT) != 0)
   
                                                                           
                                                                           
                                                   
   
#define SxactHasConflictOut(sxact) (((sxact)->flags & SXACT_FLAG_CONFLICT_OUT) != 0)
#define SxactIsDeferrableWaiting(sxact) (((sxact)->flags & SXACT_FLAG_DEFERRABLE_WAITING) != 0)
#define SxactIsROSafe(sxact) (((sxact)->flags & SXACT_FLAG_RO_SAFE) != 0)
#define SxactIsROUnsafe(sxact) (((sxact)->flags & SXACT_FLAG_RO_UNSAFE) != 0)
#define SxactIsPartiallyReleased(sxact) (((sxact)->flags & SXACT_FLAG_PARTIALLY_RELEASED) != 0)

   
                                                                   
   
                                                                           
                                                                          
                                                                         
                                                
   
#define PredicateLockTargetTagHashCode(predicatelocktargettag) get_hash_value(PredicateLockTargetHash, predicatelocktargettag)

   
                                                            
                          
   
                                                                         
                                                                 
                                                                      
                                                                
                                                                   
   
#define PredicateLockHashCodeFromTargetHashCode(predicatelocktag, targethash) ((targethash) ^ ((uint32)PointerGetDatum((predicatelocktag)->myXact)) << LOG2_NUM_PREDICATELOCK_PARTITIONS)

   
                                                              
   
static SlruCtlData OldSerXidSlruCtlData;

#define OldSerXidSlruCtl (&OldSerXidSlruCtlData)

#define OLDSERXID_PAGESIZE BLCKSZ
#define OLDSERXID_ENTRYSIZE sizeof(SerCommitSeqNo)
#define OLDSERXID_ENTRIESPERPAGE (OLDSERXID_PAGESIZE / OLDSERXID_ENTRYSIZE)

   
                                                                           
   
#define OLDSERXID_MAX_PAGE (MaxTransactionId / OLDSERXID_ENTRIESPERPAGE)

#define OldSerXidNextPage(page) (((page) >= OLDSERXID_MAX_PAGE) ? 0 : (page) + 1)

#define OldSerXidValue(slotno, xid) (*((SerCommitSeqNo *)(OldSerXidSlruCtl->shared->page_buffer[slotno] + ((((uint32)(xid)) % OLDSERXID_ENTRIESPERPAGE) * OLDSERXID_ENTRYSIZE))))

#define OldSerXidPage(xid) (((uint32)(xid)) / OLDSERXID_ENTRIESPERPAGE)

typedef struct OldSerXidControlData
{
  int headPage;                                       
  TransactionId headXid;                                   
  TransactionId tailXid;                                            
} OldSerXidControlData;

typedef struct OldSerXidControlData *OldSerXidControl;

static OldSerXidControl oldSerXidControl;

   
                                                                            
                                                                        
                                                                       
                        
   
static SERIALIZABLEXACT *OldCommittedSxact;

   
                                                                               
                                                                            
                                                                          
                                                       
   
int max_predicate_locks_per_xact;                       
int max_predicate_locks_per_relation;                   
int max_predicate_locks_per_page;                       

   
                                                                  
                                                                            
                                                                            
                                                                          
                                                                             
                                                                         
                        
   
static PredXactList PredXact;

   
                                                                             
                         
   
static RWConflictPoolHeader RWConflictPool;

   
                                                           
                                        
   
static HTAB *SerializableXidHash;
static HTAB *PredicateLockTargetHash;
static HTAB *PredicateLockHash;
static SHM_QUEUE *FinishedSerializableTransactions;

   
                                                                             
                                                                              
                                                                            
   
static const PREDICATELOCKTARGETTAG ScratchTargetTag = {0, 0, 0, 0};
static uint32 ScratchTargetTagHash;
static LWLock *ScratchPartitionLock;

   
                                                                         
                                                     
   
static HTAB *LocalPredicateLockHash = NULL;

   
                                                                             
                                                                              
                        
   
static SERIALIZABLEXACT *MySerializableXact = InvalidSerializableXact;
static bool MyXactDidWrite = false;

   
                                                                  
                                                                              
                                                                       
                                                                           
                                    
   
static SERIALIZABLEXACT *SavedSerializableXact = InvalidSerializableXact;

                     

static SERIALIZABLEXACT *
CreatePredXact(void);
static void
ReleasePredXact(SERIALIZABLEXACT *sxact);
static SERIALIZABLEXACT *
FirstPredXact(void);
static SERIALIZABLEXACT *
NextPredXact(SERIALIZABLEXACT *sxact);

static bool
RWConflictExists(const SERIALIZABLEXACT *reader, const SERIALIZABLEXACT *writer);
static void
SetRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer);
static void
SetPossibleUnsafeConflict(SERIALIZABLEXACT *roXact, SERIALIZABLEXACT *activeXact);
static void
ReleaseRWConflict(RWConflict conflict);
static void
FlagSxactUnsafe(SERIALIZABLEXACT *sxact);

static bool
OldSerXidPagePrecedesLogically(int page1, int page2);
static void
OldSerXidInit(void);
static void
OldSerXidAdd(TransactionId xid, SerCommitSeqNo minConflictCommitSeqNo);
static SerCommitSeqNo
OldSerXidGetMinConflictCommitSeqNo(TransactionId xid);
static void
OldSerXidSetActiveSerXmin(TransactionId xid);

static uint32
predicatelock_hash(const void *key, Size keysize);
static void
SummarizeOldestCommittedSxact(void);
static Snapshot
GetSafeSnapshot(Snapshot snapshot);
static Snapshot
GetSerializableTransactionSnapshotInt(Snapshot snapshot, VirtualTransactionId *sourcevxid, int sourcepid);
static bool
PredicateLockExists(const PREDICATELOCKTARGETTAG *targettag);
static bool
GetParentPredicateLockTag(const PREDICATELOCKTARGETTAG *tag, PREDICATELOCKTARGETTAG *parent);
static bool
CoarserLockCovers(const PREDICATELOCKTARGETTAG *newtargettag);
static void
RemoveScratchTarget(bool lockheld);
static void
RestoreScratchTarget(bool lockheld);
static void
RemoveTargetIfNoLongerUsed(PREDICATELOCKTARGET *target, uint32 targettaghash);
static void
DeleteChildTargetLocks(const PREDICATELOCKTARGETTAG *newtargettag);
static int
MaxPredicateChildLocks(const PREDICATELOCKTARGETTAG *tag);
static bool
CheckAndPromotePredicateLockRequest(const PREDICATELOCKTARGETTAG *reqtag);
static void
DecrementParentLocks(const PREDICATELOCKTARGETTAG *targettag);
static void
CreatePredicateLock(const PREDICATELOCKTARGETTAG *targettag, uint32 targettaghash, SERIALIZABLEXACT *sxact);
static void
DeleteLockTarget(PREDICATELOCKTARGET *target, uint32 targettaghash);
static bool
TransferPredicateLocksToNewTarget(PREDICATELOCKTARGETTAG oldtargettag, PREDICATELOCKTARGETTAG newtargettag, bool removeOld);
static void
PredicateLockAcquire(const PREDICATELOCKTARGETTAG *targettag);
static void
DropAllPredicateLocksFromTable(Relation relation, bool transfer);
static void
SetNewSxactGlobalXmin(void);
static void
ClearOldPredicateLocks(void);
static void
ReleaseOneSerializableXact(SERIALIZABLEXACT *sxact, bool partial, bool summarize);
static bool
XidIsConcurrent(TransactionId xid);
static void
CheckTargetForConflictsIn(PREDICATELOCKTARGETTAG *targettag);
static void
FlagRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer);
static void
OnConflict_CheckForSerializationFailure(const SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer);
static void
CreateLocalPredicateLockHash(void);
static void
ReleasePredicateLocksLocal(void);

                                                                            

   
                                                                             
                                                    
   
static inline bool
PredicateLockingNeededForRelation(Relation relation)
{
  return !(relation->rd_id < FirstBootstrapObjectId || RelationUsesLocalBuffers(relation) || relation->rd_rel->relkind == RELKIND_MATVIEW);
}

   
                                                                            
                                       
   
                                                                              
                                                                            
                                                                      
   
                                                                             
                                                 
   
static inline bool
SerializationNeededForRead(Relation relation, Snapshot snapshot)
{
                                                               
  if (MySerializableXact == InvalidSerializableXact)
  {
    return false;
  }

     
                                                                            
                                                                           
                                                          
                                                                            
                                                      
     
  if (!IsMVCCSnapshot(snapshot))
  {
    return false;
  }

     
                                                                             
                                                               
                                                                            
              
     
                                                                            
                                                                      
                                                                   
     
  if (SxactIsROSafe(MySerializableXact))
  {
    ReleasePredicateLocks(false, true);
    return false;
  }

                                                                      
  if (!PredicateLockingNeededForRelation(relation))
  {
    return false;
  }

  return true;                                          
}

   
                                                            
                                                                            
   
static inline bool
SerializationNeededForWrite(Relation relation)
{
                                                               
  if (MySerializableXact == InvalidSerializableXact)
  {
    return false;
  }

                                                                      
  if (!PredicateLockingNeededForRelation(relation))
  {
    return false;
  }

  return true;                                          
}

                                                                            

   
                                                                           
                                                                          
                                   
   
static SERIALIZABLEXACT *
CreatePredXact(void)
{
  PredXactListElement ptle;

  ptle = (PredXactListElement)SHMQueueNext(&PredXact->availableList, &PredXact->availableList, offsetof(PredXactListElementData, link));
  if (!ptle)
  {
    return NULL;
  }

  SHMQueueDelete(&ptle->link);
  SHMQueueInsertBefore(&PredXact->activeList, &ptle->link);
  return &ptle->sxact;
}

static void
ReleasePredXact(SERIALIZABLEXACT *sxact)
{
  PredXactListElement ptle;

  Assert(ShmemAddrIsValid(sxact));

  ptle = (PredXactListElement)(((char *)sxact) - offsetof(PredXactListElementData, sxact) + offsetof(PredXactListElementData, link));
  SHMQueueDelete(&ptle->link);
  SHMQueueInsertBefore(&PredXact->availableList, &ptle->link);
}

static SERIALIZABLEXACT *
FirstPredXact(void)
{
  PredXactListElement ptle;

  ptle = (PredXactListElement)SHMQueueNext(&PredXact->activeList, &PredXact->activeList, offsetof(PredXactListElementData, link));
  if (!ptle)
  {
    return NULL;
  }

  return &ptle->sxact;
}

static SERIALIZABLEXACT *
NextPredXact(SERIALIZABLEXACT *sxact)
{
  PredXactListElement ptle;

  Assert(ShmemAddrIsValid(sxact));

  ptle = (PredXactListElement)(((char *)sxact) - offsetof(PredXactListElementData, sxact) + offsetof(PredXactListElementData, link));
  ptle = (PredXactListElement)SHMQueueNext(&PredXact->activeList, &ptle->link, offsetof(PredXactListElementData, link));
  if (!ptle)
  {
    return NULL;
  }

  return &ptle->sxact;
}

                                                                            

   
                                                                             
   
static bool
RWConflictExists(const SERIALIZABLEXACT *reader, const SERIALIZABLEXACT *writer)
{
  RWConflict conflict;

  Assert(reader != writer);

                                                       
  if (SxactIsDoomed(reader) || SxactIsDoomed(writer) || SHMQueueEmpty(&reader->outConflicts) || SHMQueueEmpty(&writer->inConflicts))
  {
    return false;
  }

                                                          
  conflict = (RWConflict)SHMQueueNext(&reader->outConflicts, &reader->outConflicts, offsetof(RWConflictData, outLink));
  while (conflict)
  {
    if (conflict->sxactIn == writer)
    {
      return true;
    }
    conflict = (RWConflict)SHMQueueNext(&reader->outConflicts, &conflict->outLink, offsetof(RWConflictData, outLink));
  }

                          
  return false;
}

static void
SetRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer)
{
  RWConflict conflict;

  Assert(reader != writer);
  Assert(!RWConflictExists(reader, writer));

  conflict = (RWConflict)SHMQueueNext(&RWConflictPool->availableList, &RWConflictPool->availableList, offsetof(RWConflictData, outLink));
  if (!conflict)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("not enough elements in RWConflictPool to record a read/write conflict"), errhint("You might need to run fewer transactions at a time or increase max_connections.")));
  }

  SHMQueueDelete(&conflict->outLink);

  conflict->sxactOut = reader;
  conflict->sxactIn = writer;
  SHMQueueInsertBefore(&reader->outConflicts, &conflict->outLink);
  SHMQueueInsertBefore(&writer->inConflicts, &conflict->inLink);
}

static void
SetPossibleUnsafeConflict(SERIALIZABLEXACT *roXact, SERIALIZABLEXACT *activeXact)
{
  RWConflict conflict;

  Assert(roXact != activeXact);
  Assert(SxactIsReadOnly(roXact));
  Assert(!SxactIsReadOnly(activeXact));

  conflict = (RWConflict)SHMQueueNext(&RWConflictPool->availableList, &RWConflictPool->availableList, offsetof(RWConflictData, outLink));
  if (!conflict)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("not enough elements in RWConflictPool to record a potential read/write conflict"), errhint("You might need to run fewer transactions at a time or increase max_connections.")));
  }

  SHMQueueDelete(&conflict->outLink);

  conflict->sxactOut = activeXact;
  conflict->sxactIn = roXact;
  SHMQueueInsertBefore(&activeXact->possibleUnsafeConflicts, &conflict->outLink);
  SHMQueueInsertBefore(&roXact->possibleUnsafeConflicts, &conflict->inLink);
}

static void
ReleaseRWConflict(RWConflict conflict)
{
  SHMQueueDelete(&conflict->inLink);
  SHMQueueDelete(&conflict->outLink);
  SHMQueueInsertBefore(&RWConflictPool->availableList, &conflict->outLink);
}

static void
FlagSxactUnsafe(SERIALIZABLEXACT *sxact)
{
  RWConflict conflict, nextConflict;

  Assert(SxactIsReadOnly(sxact));
  Assert(!SxactIsROSafe(sxact));

  sxact->flags |= SXACT_FLAG_RO_UNSAFE;

     
                                                                          
                          
     
  conflict = (RWConflict)SHMQueueNext(&sxact->possibleUnsafeConflicts, &sxact->possibleUnsafeConflicts, offsetof(RWConflictData, inLink));
  while (conflict)
  {
    nextConflict = (RWConflict)SHMQueueNext(&sxact->possibleUnsafeConflicts, &conflict->inLink, offsetof(RWConflictData, inLink));

    Assert(!SxactIsReadOnly(conflict->sxactOut));
    Assert(sxact == conflict->sxactIn);

    ReleaseRWConflict(conflict);

    conflict = nextConflict;
  }
}

                                                                            

   
                                                                               
                                    
   
static bool
OldSerXidPagePrecedesLogically(int page1, int page2)
{
  TransactionId xid1;
  TransactionId xid2;

  xid1 = ((TransactionId)page1) * OLDSERXID_ENTRIESPERPAGE;
  xid1 += FirstNormalTransactionId + 1;
  xid2 = ((TransactionId)page2) * OLDSERXID_ENTRIESPERPAGE;
  xid2 += FirstNormalTransactionId + 1;

  return (TransactionIdPrecedes(xid1, xid2) && TransactionIdPrecedes(xid1, xid2 + OLDSERXID_ENTRIESPERPAGE - 1));
}

#ifdef USE_ASSERT_CHECKING
static void
OldSerXidPagePrecedesLogicallyUnitTests(void)
{
  int per_page = OLDSERXID_ENTRIESPERPAGE, offset = per_page / 2;
  int newestPage, oldestPage, headPage, targetPage;
  TransactionId newestXact, oldestXact;

                                                                          
  newestPage = 2 * SLRU_PAGES_PER_SEGMENT - 1;                      
  newestXact = newestPage * per_page + offset;
  Assert(newestXact / per_page == newestPage);
  oldestXact = newestXact + 1;
  oldestXact -= 1U << 31;
  oldestPage = oldestXact / per_page;

     
                                                                         
                                                                      
                                                                        
                                                                          
                                                                          
                                                                          
                                                 
     
  headPage = newestPage;
  targetPage = oldestPage;
  Assert(!OldSerXidPagePrecedesLogically(headPage, targetPage));

     
                                                                        
                                                                      
                                                                   
                                                                      
                                                                         
     
                                                                        
                                                                     
                                                                        
                                                                 
                                                                         
                                                                    
     
  headPage = oldestPage;
  targetPage = newestPage;
  Assert(OldSerXidPagePrecedesLogically(headPage, targetPage - 1));
#if 0
	Assert(OldSerXidPagePrecedesLogically(headPage, targetPage));
#endif
}
#endif

   
                                                                   
   
static void
OldSerXidInit(void)
{
  bool found;

     
                                                   
     
  OldSerXidSlruCtl->PagePrecedes = OldSerXidPagePrecedesLogically;
  SimpleLruInit(OldSerXidSlruCtl, "oldserxid", NUM_OLDSERXID_BUFFERS, 0, OldSerXidLock, "pg_serial", LWTRANCHE_OLDSERXID_BUFFERS);
                                                                 
  OldSerXidSlruCtl->do_fsync = false;
#ifdef USE_ASSERT_CHECKING
  OldSerXidPagePrecedesLogicallyUnitTests();
#endif
  SlruPagePrecedesUnitTests(OldSerXidSlruCtl, OLDSERXID_ENTRIESPERPAGE);

     
                                                         
     
  oldSerXidControl = (OldSerXidControl)ShmemInitStruct("OldSerXidControlData", sizeof(OldSerXidControlData), &found);

  Assert(found == IsUnderPostmaster);
  if (!found)
  {
       
                                                      
       
    oldSerXidControl->headPage = -1;
    oldSerXidControl->headXid = InvalidTransactionId;
    oldSerXidControl->tailXid = InvalidTransactionId;
  }
}

   
                                                                  
                                                                            
                                                                     
   
static void
OldSerXidAdd(TransactionId xid, SerCommitSeqNo minConflictCommitSeqNo)
{
  TransactionId tailXid;
  int targetPage;
  int slotno;
  int firstZeroPage;
  bool isNewPage;

  Assert(TransactionIdIsValid(xid));

  targetPage = OldSerXidPage(xid);

  LWLockAcquire(OldSerXidLock, LW_EXCLUSIVE);

     
                                                                             
                                                                      
                                                     
     
  tailXid = oldSerXidControl->tailXid;
  Assert(TransactionIdIsValid(tailXid));

     
                                                                            
                                                                           
                                                                      
              
     
  if (oldSerXidControl->headPage < 0)
  {
    firstZeroPage = OldSerXidPage(tailXid);
    isNewPage = true;
  }
  else
  {
    firstZeroPage = OldSerXidNextPage(oldSerXidControl->headPage);
    isNewPage = OldSerXidPagePrecedesLogically(oldSerXidControl->headPage, targetPage);
  }

  if (!TransactionIdIsValid(oldSerXidControl->headXid) || TransactionIdFollows(xid, oldSerXidControl->headXid))
  {
    oldSerXidControl->headXid = xid;
  }
  if (isNewPage)
  {
    oldSerXidControl->headPage = targetPage;
  }

  if (isNewPage)
  {
                                       
    while (firstZeroPage != targetPage)
    {
      (void)SimpleLruZeroPage(OldSerXidSlruCtl, firstZeroPage);
      firstZeroPage = OldSerXidNextPage(firstZeroPage);
    }
    slotno = SimpleLruZeroPage(OldSerXidSlruCtl, targetPage);
  }
  else
  {
    slotno = SimpleLruReadPage(OldSerXidSlruCtl, targetPage, true, xid);
  }

  OldSerXidValue(slotno, xid) = minConflictCommitSeqNo;
  OldSerXidSlruCtl->shared->page_dirty[slotno] = true;

  LWLockRelease(OldSerXidLock);
}

   
                                                                            
                                                                             
                     
   
static SerCommitSeqNo
OldSerXidGetMinConflictCommitSeqNo(TransactionId xid)
{
  TransactionId headXid;
  TransactionId tailXid;
  SerCommitSeqNo val;
  int slotno;

  Assert(TransactionIdIsValid(xid));

  LWLockAcquire(OldSerXidLock, LW_SHARED);
  headXid = oldSerXidControl->headXid;
  tailXid = oldSerXidControl->tailXid;
  LWLockRelease(OldSerXidLock);

  if (!TransactionIdIsValid(headXid))
  {
    return 0;
  }

  Assert(TransactionIdIsValid(tailXid));

  if (TransactionIdPrecedes(xid, tailXid) || TransactionIdFollows(xid, headXid))
  {
    return 0;
  }

     
                                                                          
                                                                       
     
  slotno = SimpleLruReadPage_ReadOnly(OldSerXidSlruCtl, OldSerXidPage(xid), xid);
  val = OldSerXidValue(slotno, xid);
  LWLockRelease(OldSerXidLock);
  return val;
}

   
                                                                  
                                                                          
                                                                           
                              
   
static void
OldSerXidSetActiveSerXmin(TransactionId xid)
{
  LWLockAcquire(OldSerXidLock, LW_EXCLUSIVE);

     
                                                                        
                                                                             
                                                                             
                                        
     
  if (!TransactionIdIsValid(xid))
  {
    oldSerXidControl->tailXid = InvalidTransactionId;
    oldSerXidControl->headXid = InvalidTransactionId;
    LWLockRelease(OldSerXidLock);
    return;
  }

     
                                                                             
                                                                             
                                                                          
                                                    
     
  if (RecoveryInProgress())
  {
    Assert(oldSerXidControl->headPage < 0);
    if (!TransactionIdIsValid(oldSerXidControl->tailXid) || TransactionIdPrecedes(xid, oldSerXidControl->tailXid))
    {
      oldSerXidControl->tailXid = xid;
    }
    LWLockRelease(OldSerXidLock);
    return;
  }

  Assert(!TransactionIdIsValid(oldSerXidControl->tailXid) || TransactionIdFollows(xid, oldSerXidControl->tailXid));

  oldSerXidControl->tailXid = xid;

  LWLockRelease(OldSerXidLock);
}

   
                                                                  
   
                                                                         
                                          
   
void
CheckPointPredicate(void)
{
  int tailPage;

  LWLockAcquire(OldSerXidLock, LW_EXCLUSIVE);

                                                         
  if (oldSerXidControl->headPage < 0)
  {
    LWLockRelease(OldSerXidLock);
    return;
  }

  if (TransactionIdIsValid(oldSerXidControl->tailXid))
  {
                                                                    
    tailPage = OldSerXidPage(oldSerXidControl->tailXid);
  }
  else
  {
                 
                                                                         
                
       
                                                                      
                                                                         
                                                                         
                                                                     
                      
       
                                                                           
                                                                          
                                                                   
                                           
                                                                           
                        
                                             
                                                                        
                                                                       
                                                                           
                                  
                                       
                                                             
                                                                  
                                                                   
                                                                  
                                                                         
                              
       
    tailPage = oldSerXidControl->headPage;
    oldSerXidControl->headPage = -1;
  }

  LWLockRelease(OldSerXidLock);

                                                       
  SimpleLruTruncate(OldSerXidSlruCtl, tailPage);

     
                                    
     
                                                                            
                                   
     
                                                                        
                                                                           
                
     
  SimpleLruFlush(OldSerXidSlruCtl, true);
}

                                                                            

   
                                                                           
   
                                                                        
                                                                         
                                                    
                                                                    
                                                                            
                       
   
void
InitPredicateLocks(void)
{
  HASHCTL info;
  long max_table_size;
  Size requestSize;
  bool found;

#ifndef EXEC_BACKEND
  Assert(!IsUnderPostmaster);
#endif

     
                                                                 
                                                          
     
  max_table_size = NPREDICATELOCKTARGETENTS();

     
                                                                       
                                            
     
  MemSet(&info, 0, sizeof(info));
  info.keysize = sizeof(PREDICATELOCKTARGETTAG);
  info.entrysize = sizeof(PREDICATELOCKTARGET);
  info.num_partitions = NUM_PREDICATELOCK_PARTITIONS;

  PredicateLockTargetHash = ShmemInitHash("PREDICATELOCKTARGET hash", max_table_size, max_table_size, &info, HASH_ELEM | HASH_BLOBS | HASH_PARTITION | HASH_FIXED_SIZE);

     
                                                                             
                                                                         
                                                              
                                   
     
  if (!IsUnderPostmaster)
  {
    (void)hash_search(PredicateLockTargetHash, &ScratchTargetTag, HASH_ENTER, &found);
    Assert(!found);
  }

                                                                      
  ScratchTargetTagHash = PredicateLockTargetTagHashCode(&ScratchTargetTag);
  ScratchPartitionLock = PredicateLockHashPartitionLock(ScratchTargetTagHash);

     
                                                                     
                                        
     
  MemSet(&info, 0, sizeof(info));
  info.keysize = sizeof(PREDICATELOCKTAG);
  info.entrysize = sizeof(PREDICATELOCK);
  info.hash = predicatelock_hash;
  info.num_partitions = NUM_PREDICATELOCK_PARTITIONS;

                                               
  max_table_size *= 2;

  PredicateLockHash = ShmemInitHash("PREDICATELOCK hash", max_table_size, max_table_size, &info, HASH_ELEM | HASH_FUNCTION | HASH_PARTITION | HASH_FIXED_SIZE);

     
                                                                     
                                                          
     
  max_table_size = (MaxBackends + max_prepared_xacts);

     
                                                                          
                        
     
                                                                         
                                                                             
                                                                    
     
  max_table_size *= 10;

  PredXact = ShmemInitStruct("PredXactList", PredXactListDataSize, &found);
  Assert(found == IsUnderPostmaster);
  if (!found)
  {
    int i;

    SHMQueueInit(&PredXact->availableList);
    SHMQueueInit(&PredXact->activeList);
    PredXact->SxactGlobalXmin = InvalidTransactionId;
    PredXact->SxactGlobalXminCount = 0;
    PredXact->WritableSxactCount = 0;
    PredXact->LastSxactCommitSeqNo = FirstNormalSerCommitSeqNo - 1;
    PredXact->CanPartialClearThrough = 0;
    PredXact->HavePartialClearedThrough = 0;
    requestSize = mul_size((Size)max_table_size, PredXactListElementDataSize);
    PredXact->element = ShmemAlloc(requestSize);
                                                    
    memset(PredXact->element, 0, requestSize);
    for (i = 0; i < max_table_size; i++)
    {
      LWLockInitialize(&PredXact->element[i].sxact.predicateLockListLock, LWTRANCHE_SXACT);
      SHMQueueInsertBefore(&(PredXact->availableList), &(PredXact->element[i].link));
    }
    PredXact->OldCommittedSxact = CreatePredXact();
    SetInvalidVirtualTransactionId(PredXact->OldCommittedSxact->vxid);
    PredXact->OldCommittedSxact->prepareSeqNo = 0;
    PredXact->OldCommittedSxact->commitSeqNo = 0;
    PredXact->OldCommittedSxact->SeqNo.lastCommitBeforeSnapshot = 0;
    SHMQueueInit(&PredXact->OldCommittedSxact->outConflicts);
    SHMQueueInit(&PredXact->OldCommittedSxact->inConflicts);
    SHMQueueInit(&PredXact->OldCommittedSxact->predicateLocks);
    SHMQueueInit(&PredXact->OldCommittedSxact->finishedLink);
    SHMQueueInit(&PredXact->OldCommittedSxact->possibleUnsafeConflicts);
    PredXact->OldCommittedSxact->topXid = InvalidTransactionId;
    PredXact->OldCommittedSxact->finishedBefore = InvalidTransactionId;
    PredXact->OldCommittedSxact->xmin = InvalidTransactionId;
    PredXact->OldCommittedSxact->flags = SXACT_FLAG_COMMITTED;
    PredXact->OldCommittedSxact->pid = 0;
  }
                                                       
  OldCommittedSxact = PredXact->OldCommittedSxact;

     
                                                                           
                                                                         
     
  MemSet(&info, 0, sizeof(info));
  info.keysize = sizeof(SERIALIZABLEXIDTAG);
  info.entrysize = sizeof(SERIALIZABLEXID);

  SerializableXidHash = ShmemInitHash("SERIALIZABLEXID hash", max_table_size, max_table_size, &info, HASH_ELEM | HASH_BLOBS | HASH_FIXED_SIZE);

     
                                                                       
                   
     
                                                                             
                                                                          
                                                                             
                                                                         
                                                                            
                  
     
  max_table_size *= 5;

  RWConflictPool = ShmemInitStruct("RWConflictPool", RWConflictPoolHeaderDataSize, &found);
  Assert(found == IsUnderPostmaster);
  if (!found)
  {
    int i;

    SHMQueueInit(&RWConflictPool->availableList);
    requestSize = mul_size((Size)max_table_size, RWConflictDataSize);
    RWConflictPool->element = ShmemAlloc(requestSize);
                                                    
    memset(RWConflictPool->element, 0, requestSize);
    for (i = 0; i < max_table_size; i++)
    {
      SHMQueueInsertBefore(&(RWConflictPool->availableList), &(RWConflictPool->element[i].outLink));
    }
  }

     
                                                                          
                   
     
  FinishedSerializableTransactions = (SHM_QUEUE *)ShmemInitStruct("FinishedSerializableTransactions", sizeof(SHM_QUEUE), &found);
  Assert(found == IsUnderPostmaster);
  if (!found)
  {
    SHMQueueInit(FinishedSerializableTransactions);
  }

     
                                                                
                   
     
  OldSerXidInit();
}

   
                                                              
   
Size
PredicateLockShmemSize(void)
{
  Size size = 0;
  long max_table_size;

                                        
  max_table_size = NPREDICATELOCKTARGETENTS();
  size = add_size(size, hash_estimate_size(max_table_size, sizeof(PREDICATELOCKTARGET)));

                                 
  max_table_size *= 2;
  size = add_size(size, hash_estimate_size(max_table_size, sizeof(PREDICATELOCK)));

     
                                                                        
             
     
  size = add_size(size, size / 10);

                        
  max_table_size = MaxBackends + max_prepared_xacts;
  max_table_size *= 10;
  size = add_size(size, PredXactListDataSize);
  size = add_size(size, mul_size((Size)max_table_size, PredXactListElementDataSize));

                             
  size = add_size(size, hash_estimate_size(max_table_size, sizeof(SERIALIZABLEXID)));

                        
  max_table_size *= 5;
  size = add_size(size, RWConflictPoolHeaderDataSize);
  size = add_size(size, mul_size((Size)max_table_size, RWConflictDataSize));

                                                            
  size = add_size(size, sizeof(SHM_QUEUE));

                                                                         
  size = add_size(size, sizeof(OldSerXidControlData));
  size = add_size(size, SimpleLruShmemSize(NUM_OLDSERXID_BUFFERS, 0));

  return size;
}

   
                                                             
   
                                                                       
                                                                           
                                                                    
                                                                             
                                                              
                                                                         
                                                                             
                              
   
static uint32
predicatelock_hash(const void *key, Size keysize)
{
  const PREDICATELOCKTAG *predicatelocktag = (const PREDICATELOCKTAG *)key;
  uint32 targethash;

  Assert(keysize == sizeof(PREDICATELOCKTAG));

                                                                         
  targethash = PredicateLockTargetTagHashCode(&predicatelocktag->myTarget->tag);

  return PredicateLockHashCodeFromTargetHashCode(predicatelocktag, targethash);
}

   
                              
                                                                  
                                            
   
                                                                             
                                                                       
                                                                         
                                                                       
                                        
   
PredicateLockData *
GetPredicateLockStatusData(void)
{
  PredicateLockData *data;
  int i;
  int els, el;
  HASH_SEQ_STATUS seqstat;
  PREDICATELOCK *predlock;

  data = (PredicateLockData *)palloc(sizeof(PredicateLockData));

     
                                                                           
                                                        
     
  for (i = 0; i < NUM_PREDICATELOCK_PARTITIONS; i++)
  {
    LWLockAcquire(PredicateLockHashPartitionLockByIndex(i), LW_SHARED);
  }
  LWLockAcquire(SerializableXactHashLock, LW_SHARED);

                                                                    
  els = hash_get_num_entries(PredicateLockHash);
  data->nelements = els;
  data->locktags = (PREDICATELOCKTARGETTAG *)palloc(sizeof(PREDICATELOCKTARGETTAG) * els);
  data->xacts = (SERIALIZABLEXACT *)palloc(sizeof(SERIALIZABLEXACT) * els);

                                                        
  hash_seq_init(&seqstat, PredicateLockHash);

  el = 0;

  while ((predlock = (PREDICATELOCK *)hash_seq_search(&seqstat)))
  {
    data->locktags[el] = predlock->tag.myTarget->tag;
    data->xacts[el] = *predlock->tag.myXact;
    el++;
  }

  Assert(el == els);

                                      
  LWLockRelease(SerializableXactHashLock);
  for (i = NUM_PREDICATELOCK_PARTITIONS - 1; i >= 0; i--)
  {
    LWLockRelease(PredicateLockHashPartitionLockByIndex(i));
  }

  return data;
}

   
                                                                            
                                                                            
                                                                           
                                                                         
                                        
   
static void
SummarizeOldestCommittedSxact(void)
{
  SERIALIZABLEXACT *sxact;

  LWLockAcquire(SerializableFinishedListLock, LW_EXCLUSIVE);

     
                                                                         
                                                                        
                                                                             
                                                                            
                                                                          
                                                                          
                                                                            
                                                          
     
  if (SHMQueueEmpty(FinishedSerializableTransactions))
  {
    LWLockRelease(SerializableFinishedListLock);
    return;
  }

     
                                                                             
                                       
     
  sxact = (SERIALIZABLEXACT *)SHMQueueNext(FinishedSerializableTransactions, FinishedSerializableTransactions, offsetof(SERIALIZABLEXACT, finishedLink));
  SHMQueueDelete(&(sxact->finishedLink));

                                        
  if (TransactionIdIsValid(sxact->topXid) && !SxactIsReadOnly(sxact))
  {
    OldSerXidAdd(sxact->topXid, SxactHasConflictOut(sxact) ? sxact->SeqNo.earliestOutConflictCommit : InvalidSerCommitSeqNo);
  }

                                         
  ReleaseOneSerializableXact(sxact, false, true);

  LWLockRelease(SerializableFinishedListLock);
}

   
                   
                                                              
                                                             
                                                                 
                                                                 
                                                                  
                                                  
   
                                                                           
                                                                        
                                                       
   
static Snapshot
GetSafeSnapshot(Snapshot origSnapshot)
{
  Snapshot snapshot;

  Assert(XactReadOnly && XactDeferrable);

  while (true)
  {
       
                                                              
                                                                          
                                                                           
                                                          
       
    snapshot = GetSerializableTransactionSnapshotInt(origSnapshot, NULL, InvalidPid);

    if (MySerializableXact == InvalidSerializableXact)
    {
      return snapshot;                                         
    }

    LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

       
                                                                        
                                     
       
    MySerializableXact->flags |= SXACT_FLAG_DEFERRABLE_WAITING;
    while (!(SHMQueueEmpty(&MySerializableXact->possibleUnsafeConflicts) || SxactIsROUnsafe(MySerializableXact)))
    {
      LWLockRelease(SerializableXactHashLock);
      ProcWaitForSignal(WAIT_EVENT_SAFE_SNAPSHOT);
      LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);
    }
    MySerializableXact->flags &= ~SXACT_FLAG_DEFERRABLE_WAITING;

    if (!SxactIsROUnsafe(MySerializableXact))
    {
      LWLockRelease(SerializableXactHashLock);
      break;              
    }

    LWLockRelease(SerializableXactHashLock);

                                
    ereport(DEBUG2, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("deferrable snapshot was unsafe; trying a new one")));
    ReleasePredicateLocks(false, false);
  }

     
                                                                             
     
  Assert(SxactIsROSafe(MySerializableXact));
  ReleasePredicateLocks(false, true);

  return snapshot;
}

   
                               
                                                                      
                                                                 
                                                                        
                                                                   
                                                                      
                        
   
int
GetSafeSnapshotBlockingPids(int blocked_pid, int *output, int output_size)
{
  int num_written = 0;
  SERIALIZABLEXACT *sxact;

  LWLockAcquire(SerializableXactHashLock, LW_SHARED);

                                                             
  for (sxact = FirstPredXact(); sxact != NULL; sxact = NextPredXact(sxact))
  {
    if (sxact->pid == blocked_pid)
    {
      break;
    }
  }

                                                                       
  if (sxact != NULL && SxactIsDeferrableWaiting(sxact))
  {
    RWConflict possibleUnsafeConflict;

                                                                         
    possibleUnsafeConflict = (RWConflict)SHMQueueNext(&sxact->possibleUnsafeConflicts, &sxact->possibleUnsafeConflicts, offsetof(RWConflictData, inLink));

    while (possibleUnsafeConflict != NULL && num_written < output_size)
    {
      output[num_written++] = possibleUnsafeConflict->sxactOut->pid;
      possibleUnsafeConflict = (RWConflict)SHMQueueNext(&sxact->possibleUnsafeConflicts, &possibleUnsafeConflict->inLink, offsetof(RWConflictData, inLink));
    }
  }

  LWLockRelease(SerializableXactHashLock);

  return num_written;
}

   
                                                                    
   
                                                                         
                                                                       
   
                                                                           
                                                                          
                                                                         
                         
   
Snapshot
GetSerializableTransactionSnapshot(Snapshot snapshot)
{
  Assert(IsolationIsSerializable());

     
                                                                           
                                                                            
                                                                     
                                                   
     
  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot use serializable mode in a hot standby"), errdetail("\"default_transaction_isolation\" is set to \"serializable\"."), errhint("You can use \"SET default_transaction_isolation = 'repeatable read'\" to change the default.")));
  }

     
                                                                    
                                                                        
                                                       
     
  if (XactReadOnly && XactDeferrable)
  {
    return GetSafeSnapshot(snapshot);
  }

  return GetSerializableTransactionSnapshotInt(snapshot, NULL, InvalidPid);
}

   
                                                             
   
                                                                              
                                                                       
   
                                                                            
                                                                            
              
   
void
SetSerializableTransactionSnapshot(Snapshot snapshot, VirtualTransactionId *sourcevxid, int sourcepid)
{
  Assert(IsolationIsSerializable());

     
                                                                            
                                                             
                                                                           
                                                                         
                                                                          
                                                               
     
  if (IsParallelWorker())
  {
    return;
  }

     
                                                                       
                                                                             
                                                                             
                                                    
     
  if (XactReadOnly && XactDeferrable)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("a snapshot-importing transaction must not be READ ONLY DEFERRABLE")));
  }

  (void)GetSerializableTransactionSnapshotInt(snapshot, sourcevxid, sourcepid);
}

   
                                              
   
                                                                             
                                                                           
                                                                         
                                                                           
                                                       
   
static Snapshot
GetSerializableTransactionSnapshotInt(Snapshot snapshot, VirtualTransactionId *sourcevxid, int sourcepid)
{
  PGPROC *proc;
  VirtualTransactionId vxid;
  SERIALIZABLEXACT *sxact, *othersxact;

                                                             
  Assert(MySerializableXact == InvalidSerializableXact);

  Assert(!RecoveryInProgress());

     
                                                                     
                                                                          
                
     
  if (IsInParallelMode())
  {
    elog(ERROR, "cannot establish serializable snapshot during a parallel operation");
  }

  proc = MyProc;
  Assert(proc != NULL);
  GET_VXID_FROM_PGPROC(vxid, *proc);

     
                                                                            
                                                         
     
                                                                             
                                                              
                                                                      
                                                                             
                                                                           
                                                                          
                                                      
     
#ifdef TEST_OLDSERXID
  SummarizeOldestCommittedSxact();
#endif
  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);
  do
  {
    sxact = CreatePredXact();
                                                                    
    if (!sxact)
    {
      LWLockRelease(SerializableXactHashLock);
      SummarizeOldestCommittedSxact();
      LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);
    }
  } while (!sxact);

                                                        
  if (!sourcevxid)
  {
    snapshot = GetSnapshotData(snapshot);
  }
  else if (!ProcArrayInstallImportedXmin(snapshot->xmin, sourcevxid))
  {
    ReleasePredXact(sxact);
    LWLockRelease(SerializableXactHashLock);
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not import the requested snapshot"), errdetail("The source process with PID %d is not running anymore.", sourcepid)));
  }

     
                                                                           
                                                                    
                            
     
                                                                             
                                                                         
                                                                          
                                                                        
                                                                            
                                                            
     
  if (XactReadOnly && PredXact->WritableSxactCount == 0)
  {
    ReleasePredXact(sxact);
    LWLockRelease(SerializableXactHashLock);
    return snapshot;
  }

                                               
  if (!TransactionIdIsValid(PredXact->SxactGlobalXmin))
  {
    Assert(PredXact->SxactGlobalXminCount == 0);
    PredXact->SxactGlobalXmin = snapshot->xmin;
    PredXact->SxactGlobalXminCount = 1;
    OldSerXidSetActiveSerXmin(snapshot->xmin);
  }
  else if (TransactionIdEquals(snapshot->xmin, PredXact->SxactGlobalXmin))
  {
    Assert(PredXact->SxactGlobalXminCount > 0);
    PredXact->SxactGlobalXminCount++;
  }
  else
  {
    Assert(TransactionIdFollows(snapshot->xmin, PredXact->SxactGlobalXmin));
  }

                                 
  sxact->vxid = vxid;
  sxact->SeqNo.lastCommitBeforeSnapshot = PredXact->LastSxactCommitSeqNo;
  sxact->prepareSeqNo = InvalidSerCommitSeqNo;
  sxact->commitSeqNo = InvalidSerCommitSeqNo;
  SHMQueueInit(&(sxact->outConflicts));
  SHMQueueInit(&(sxact->inConflicts));
  SHMQueueInit(&(sxact->possibleUnsafeConflicts));
  sxact->topXid = GetTopTransactionIdIfAny();
  sxact->finishedBefore = InvalidTransactionId;
  sxact->xmin = snapshot->xmin;
  sxact->pid = MyProcPid;
  SHMQueueInit(&(sxact->predicateLocks));
  SHMQueueElemInit(&(sxact->finishedLink));
  sxact->flags = 0;
  if (XactReadOnly)
  {
    sxact->flags |= SXACT_FLAG_READ_ONLY;

       
                                                                          
                                                                    
                                                                          
                                          
       
    for (othersxact = FirstPredXact(); othersxact != NULL; othersxact = NextPredXact(othersxact))
    {
      if (!SxactIsCommitted(othersxact) && !SxactIsDoomed(othersxact) && !SxactIsReadOnly(othersxact))
      {
        SetPossibleUnsafeConflict(sxact, othersxact);
      }
    }
  }
  else
  {
    ++(PredXact->WritableSxactCount);
    Assert(PredXact->WritableSxactCount <= (MaxBackends + max_prepared_xacts));
  }

  MySerializableXact = sxact;
  MyXactDidWrite = false;                                   

  LWLockRelease(SerializableXactHashLock);

  CreateLocalPredicateLockHash();

  return snapshot;
}

static void
CreateLocalPredicateLockHash(void)
{
  HASHCTL hash_ctl;

                                                               
  Assert(LocalPredicateLockHash == NULL);
  MemSet(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(PREDICATELOCKTARGETTAG);
  hash_ctl.entrysize = sizeof(LOCALPREDICATELOCK);
  LocalPredicateLockHash = hash_create("Local predicate lock", max_predicate_locks_per_xact, &hash_ctl, HASH_ELEM | HASH_BLOBS);
}

   
                                                      
                                                           
   
void
RegisterPredicateLockingXid(TransactionId xid)
{
  SERIALIZABLEXIDTAG sxidtag;
  SERIALIZABLEXID *sxid;
  bool found;

     
                                                                        
                                                   
     
  if (MySerializableXact == InvalidSerializableXact)
  {
    return;
  }

                                                           
  Assert(TransactionIdIsValid(xid));

  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

                                                      
  Assert(MySerializableXact->topXid == InvalidTransactionId);

  MySerializableXact->topXid = xid;

  sxidtag.xid = xid;
  sxid = (SERIALIZABLEXID *)hash_search(SerializableXidHash, &sxidtag, HASH_ENTER, &found);
  Assert(!found);

                                 
  sxid->myXact = MySerializableXact;
  LWLockRelease(SerializableXactHashLock);
}

   
                                                                       
                                           
   
                                                                     
                                                                    
                                                                       
   
                                                                      
                                               
   
                                                                   
   
bool
PageIsPredicateLocked(Relation relation, BlockNumber blkno)
{
  PREDICATELOCKTARGETTAG targettag;
  uint32 targettaghash;
  LWLock *partitionLock;
  PREDICATELOCKTARGET *target;

  SET_PREDICATELOCKTARGETTAG_PAGE(targettag, relation->rd_node.dbNode, relation->rd_id, blkno);

  targettaghash = PredicateLockTargetTagHashCode(&targettag);
  partitionLock = PredicateLockHashPartitionLock(targettaghash);
  LWLockAcquire(partitionLock, LW_SHARED);
  target = (PREDICATELOCKTARGET *)hash_search_with_hash_value(PredicateLockTargetHash, &targettag, targettaghash, HASH_FIND, NULL);
  LWLockRelease(partitionLock);

  return (target != NULL);
}

   
                                                                
   
                                                                      
                                                                 
                                                                  
                                                                
                                                                      
                                                                    
               
   
static bool
PredicateLockExists(const PREDICATELOCKTARGETTAG *targettag)
{
  LOCALPREDICATELOCK *lock;

                              
  lock = (LOCALPREDICATELOCK *)hash_search(LocalPredicateLockHash, targettag, HASH_FIND, NULL);

  if (!lock)
  {
    return false;
  }

     
                                                                             
                                                          
     
  return lock->held;
}

   
                                                                      
                                      
   
                                                                  
                                 
   
static bool
GetParentPredicateLockTag(const PREDICATELOCKTARGETTAG *tag, PREDICATELOCKTARGETTAG *parent)
{
  switch (GET_PREDICATELOCKTARGETTAG_TYPE(*tag))
  {
  case PREDLOCKTAG_RELATION:
                                            
    return false;

  case PREDLOCKTAG_PAGE:
                                      
    SET_PREDICATELOCKTARGETTAG_RELATION(*parent, GET_PREDICATELOCKTARGETTAG_DB(*tag), GET_PREDICATELOCKTARGETTAG_RELATION(*tag));

    return true;

  case PREDLOCKTAG_TUPLE:
                                  
    SET_PREDICATELOCKTARGETTAG_PAGE(*parent, GET_PREDICATELOCKTARGETTAG_DB(*tag), GET_PREDICATELOCKTARGETTAG_RELATION(*tag), GET_PREDICATELOCKTARGETTAG_PAGE(*tag));
    return true;
  }

                     
  Assert(false);
  return false;
}

   
                                                                     
                                     
   
                                                                
                                                        
   
static bool
CoarserLockCovers(const PREDICATELOCKTARGETTAG *newtargettag)
{
  PREDICATELOCKTARGETTAG targettag, parenttag;

  targettag = *newtargettag;

                                               
  while (GetParentPredicateLockTag(&targettag, &parenttag))
  {
    targettag = parenttag;
    if (PredicateLockExists(&targettag))
    {
      return true;
    }
  }

                                                     
  return false;
}

   
                                                                               
                                                                                
                                                                               
         
   
                                                                         
                                                  
   
static void
RemoveScratchTarget(bool lockheld)
{
  bool found;

  Assert(LWLockHeldByMe(SerializablePredicateLockListLock));

  if (!lockheld)
  {
    LWLockAcquire(ScratchPartitionLock, LW_EXCLUSIVE);
  }
  hash_search_with_hash_value(PredicateLockTargetHash, &ScratchTargetTag, ScratchTargetTagHash, HASH_REMOVE, &found);
  Assert(found);
  if (!lockheld)
  {
    LWLockRelease(ScratchPartitionLock);
  }
}

   
                                                            
   
static void
RestoreScratchTarget(bool lockheld)
{
  bool found;

  Assert(LWLockHeldByMe(SerializablePredicateLockListLock));

  if (!lockheld)
  {
    LWLockAcquire(ScratchPartitionLock, LW_EXCLUSIVE);
  }
  hash_search_with_hash_value(PredicateLockTargetHash, &ScratchTargetTag, ScratchTargetTagHash, HASH_ENTER, &found);
  Assert(!found);
  if (!lockheld)
  {
    LWLockRelease(ScratchPartitionLock);
  }
}

   
                                                                    
                                                          
   
static void
RemoveTargetIfNoLongerUsed(PREDICATELOCKTARGET *target, uint32 targettaghash)
{
  PREDICATELOCKTARGET *rmtarget PG_USED_FOR_ASSERTS_ONLY;

  Assert(LWLockHeldByMe(SerializablePredicateLockListLock));

                                                      
  if (!SHMQueueEmpty(&target->predicateLocks))
  {
    return;
  }

                                   
  rmtarget = hash_search_with_hash_value(PredicateLockTargetHash, &target->tag, targettaghash, HASH_REMOVE, NULL);
  Assert(rmtarget == target);
}

   
                                                    
                                                                           
                                                               
   
                                                                     
                                                                      
                                                              
                                                                         
                                                                       
          
   
static void
DeleteChildTargetLocks(const PREDICATELOCKTARGETTAG *newtargettag)
{
  SERIALIZABLEXACT *sxact;
  PREDICATELOCK *predlock;

  LWLockAcquire(SerializablePredicateLockListLock, LW_SHARED);
  sxact = MySerializableXact;
  if (IsInParallelMode())
  {
    LWLockAcquire(&sxact->predicateLockListLock, LW_EXCLUSIVE);
  }
  predlock = (PREDICATELOCK *)SHMQueueNext(&(sxact->predicateLocks), &(sxact->predicateLocks), offsetof(PREDICATELOCK, xactLink));
  while (predlock)
  {
    SHM_QUEUE *predlocksxactlink;
    PREDICATELOCK *nextpredlock;
    PREDICATELOCKTAG oldlocktag;
    PREDICATELOCKTARGET *oldtarget;
    PREDICATELOCKTARGETTAG oldtargettag;

    predlocksxactlink = &(predlock->xactLink);
    nextpredlock = (PREDICATELOCK *)SHMQueueNext(&(sxact->predicateLocks), predlocksxactlink, offsetof(PREDICATELOCK, xactLink));

    oldlocktag = predlock->tag;
    Assert(oldlocktag.myXact == sxact);
    oldtarget = oldlocktag.myTarget;
    oldtargettag = oldtarget->tag;

    if (TargetTagIsCoveredBy(oldtargettag, *newtargettag))
    {
      uint32 oldtargettaghash;
      LWLock *partitionLock;
      PREDICATELOCK *rmpredlock PG_USED_FOR_ASSERTS_ONLY;

      oldtargettaghash = PredicateLockTargetTagHashCode(&oldtargettag);
      partitionLock = PredicateLockHashPartitionLock(oldtargettaghash);

      LWLockAcquire(partitionLock, LW_EXCLUSIVE);

      SHMQueueDelete(predlocksxactlink);
      SHMQueueDelete(&(predlock->targetLink));
      rmpredlock = hash_search_with_hash_value(PredicateLockHash, &oldlocktag, PredicateLockHashCodeFromTargetHashCode(&oldlocktag, oldtargettaghash), HASH_REMOVE, NULL);
      Assert(rmpredlock == predlock);

      RemoveTargetIfNoLongerUsed(oldtarget, oldtargettaghash);

      LWLockRelease(partitionLock);

      DecrementParentLocks(&oldtargettag);
    }

    predlock = nextpredlock;
  }
  if (IsInParallelMode())
  {
    LWLockRelease(&sxact->predicateLockListLock);
  }
  LWLockRelease(SerializablePredicateLockListLock);
}

   
                                                                               
                                                                            
                                                                               
                                   
   
                                                                              
                                                                             
                                       
   
                                                                             
                                                                             
                                                                      
                                                                           
                                                                          
                                          
   
static int
MaxPredicateChildLocks(const PREDICATELOCKTARGETTAG *tag)
{
  switch (GET_PREDICATELOCKTARGETTAG_TYPE(*tag))
  {
  case PREDLOCKTAG_RELATION:
    return max_predicate_locks_per_relation < 0 ? (max_predicate_locks_per_xact / (-max_predicate_locks_per_relation)) - 1 : max_predicate_locks_per_relation;

  case PREDLOCKTAG_PAGE:
    return max_predicate_locks_per_page;

  case PREDLOCKTAG_TUPLE:

       
                                                                       
                                          
       
    Assert(false);
    return 0;
  }

                     
  Assert(false);
  return 0;
}

   
                                                                   
                                                                   
                                                                
                       
   
                                                                   
   
static bool
CheckAndPromotePredicateLockRequest(const PREDICATELOCKTARGETTAG *reqtag)
{
  PREDICATELOCKTARGETTAG targettag, nexttag, promotiontag;
  LOCALPREDICATELOCK *parentlock;
  bool found, promote;

  promote = false;

  targettag = *reqtag;

                                 
  while (GetParentPredicateLockTag(&targettag, &nexttag))
  {
    targettag = nexttag;
    parentlock = (LOCALPREDICATELOCK *)hash_search(LocalPredicateLockHash, &targettag, HASH_ENTER, &found);
    if (!found)
    {
      parentlock->held = false;
      parentlock->childLocks = 1;
    }
    else
    {
      parentlock->childLocks++;
    }

    if (parentlock->childLocks > MaxPredicateChildLocks(&targettag))
    {
         
                                                                      
                                                                         
                                                                     
               
         
      promotiontag = targettag;
      promote = true;
    }
  }

  if (promote)
  {
                                                          
    PredicateLockAcquire(&promotiontag);
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                                    
          
   
                                                 
                                                                      
                                                                       
                                                                    
                                                                      
                                         
   
static void
DecrementParentLocks(const PREDICATELOCKTARGETTAG *targettag)
{
  PREDICATELOCKTARGETTAG parenttag, nexttag;

  parenttag = *targettag;

  while (GetParentPredicateLockTag(&parenttag, &nexttag))
  {
    uint32 targettaghash;
    LOCALPREDICATELOCK *parentlock, *rmlock PG_USED_FOR_ASSERTS_ONLY;

    parenttag = nexttag;
    targettaghash = PredicateLockTargetTagHashCode(&parenttag);
    parentlock = (LOCALPREDICATELOCK *)hash_search_with_hash_value(LocalPredicateLockHash, &parenttag, targettaghash, HASH_FIND, NULL);

       
                                                                        
                                                                      
                                                        
       
    if (parentlock == NULL)
    {
      continue;
    }

    parentlock->childLocks--;

       
                                                                       
                                                                        
                                      
       
    if (parentlock->childLocks < 0)
    {
      Assert(parentlock->held);
      parentlock->childLocks = 0;
    }

    if ((parentlock->childLocks == 0) && (!parentlock->held))
    {
      rmlock = (LOCALPREDICATELOCK *)hash_search_with_hash_value(LocalPredicateLockHash, &parenttag, targettaghash, HASH_REMOVE, NULL);
      Assert(rmlock == parentlock);
    }
  }
}

   
                                                                     
                                                                     
   
                                                                      
                                                                       
                                                      
                                  
   
static void
CreatePredicateLock(const PREDICATELOCKTARGETTAG *targettag, uint32 targettaghash, SERIALIZABLEXACT *sxact)
{
  PREDICATELOCKTARGET *target;
  PREDICATELOCKTAG locktag;
  PREDICATELOCK *lock;
  LWLock *partitionLock;
  bool found;

  partitionLock = PredicateLockHashPartitionLock(targettaghash);

  LWLockAcquire(SerializablePredicateLockListLock, LW_SHARED);
  if (IsInParallelMode())
  {
    LWLockAcquire(&sxact->predicateLockListLock, LW_EXCLUSIVE);
  }
  LWLockAcquire(partitionLock, LW_EXCLUSIVE);

                                                 
  target = (PREDICATELOCKTARGET *)hash_search_with_hash_value(PredicateLockTargetHash, targettag, targettaghash, HASH_ENTER_NULL, &found);
  if (!target)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_pred_locks_per_transaction.")));
  }
  if (!found)
  {
    SHMQueueInit(&(target->predicateLocks));
  }

                                                                 
  locktag.myTarget = target;
  locktag.myXact = sxact;
  lock = (PREDICATELOCK *)hash_search_with_hash_value(PredicateLockHash, &locktag, PredicateLockHashCodeFromTargetHashCode(&locktag, targettaghash), HASH_ENTER_NULL, &found);
  if (!lock)
  {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_pred_locks_per_transaction.")));
  }

  if (!found)
  {
    SHMQueueInsertBefore(&(target->predicateLocks), &(lock->targetLink));
    SHMQueueInsertBefore(&(sxact->predicateLocks), &(lock->xactLink));
    lock->commitSeqNo = InvalidSerCommitSeqNo;
  }

  LWLockRelease(partitionLock);
  if (IsInParallelMode())
  {
    LWLockRelease(&sxact->predicateLockListLock);
  }
  LWLockRelease(SerializablePredicateLockListLock);
}

   
                                                                    
                                                                     
                                                                       
                                                                     
                                                   
   
static void
PredicateLockAcquire(const PREDICATELOCKTARGETTAG *targettag)
{
  uint32 targettaghash;
  bool found;
  LOCALPREDICATELOCK *locallock;

                                                        
  if (PredicateLockExists(targettag))
  {
    return;
  }

  if (CoarserLockCovers(targettag))
  {
    return;
  }

                                                                              
  targettaghash = PredicateLockTargetTagHashCode(targettag);

                                   
  locallock = (LOCALPREDICATELOCK *)hash_search_with_hash_value(LocalPredicateLockHash, targettag, targettaghash, HASH_ENTER, &found);
  locallock->held = true;
  if (!found)
  {
    locallock->childLocks = 0;
  }

                                
  CreatePredicateLock(targettag, targettaghash, MySerializableXact);

     
                                                                      
                                                                          
               
     
  if (CheckAndPromotePredicateLockRequest(targettag))
  {
       
                                                                         
                                                                  
                                
       
  }
  else
  {
                                              
    if (GET_PREDICATELOCKTARGETTAG_TYPE(*targettag) != PREDLOCKTAG_TUPLE)
    {
      DeleteChildTargetLocks(targettag);
    }
  }
}

   
                          
   
                                                
                                                                 
                                      
                                                                             
   
void
PredicateLockRelation(Relation relation, Snapshot snapshot)
{
  PREDICATELOCKTARGETTAG tag;

  if (!SerializationNeededForRead(relation, snapshot))
  {
    return;
  }

  SET_PREDICATELOCKTARGETTAG_RELATION(tag, relation->rd_node.dbNode, relation->rd_id);
  PredicateLockAcquire(&tag);
}

   
                      
   
                                            
                                                                 
                                      
                                                              
                                                                             
   
void
PredicateLockPage(Relation relation, BlockNumber blkno, Snapshot snapshot)
{
  PREDICATELOCKTARGETTAG tag;

  if (!SerializationNeededForRead(relation, snapshot))
  {
    return;
  }

  SET_PREDICATELOCKTARGETTAG_PAGE(tag, relation->rd_node.dbNode, relation->rd_id, blkno);
  PredicateLockAcquire(&tag);
}

   
                       
   
                                             
                                                                 
                                      
   
void
PredicateLockTuple(Relation relation, HeapTuple tuple, Snapshot snapshot)
{
  PREDICATELOCKTARGETTAG tag;
  ItemPointer tid;
  TransactionId targetxmin;

  if (!SerializationNeededForRead(relation, snapshot))
  {
    return;
  }

     
                                                         
     
  if (relation->rd_index == NULL)
  {
    TransactionId myxid;

    targetxmin = HeapTupleHeaderGetXmin(tuple->t_data);

    myxid = GetTopTransactionIdIfAny();
    if (TransactionIdIsValid(myxid))
    {
      if (TransactionIdFollowsOrEquals(targetxmin, TransactionXmin))
      {
        TransactionId xid = SubTransGetTopmostTransaction(targetxmin);

        if (TransactionIdEquals(xid, myxid))
        {
                                                          
          return;
        }
      }
    }
  }

     
                                                                            
                                                                      
                                                                           
                 
     
  SET_PREDICATELOCKTARGETTAG_RELATION(tag, relation->rd_node.dbNode, relation->rd_id);
  if (PredicateLockExists(&tag))
  {
    return;
  }

  tid = &(tuple->t_self);
  SET_PREDICATELOCKTARGETTAG_TUPLE(tag, relation->rd_node.dbNode, relation->rd_id, ItemPointerGetBlockNumber(tid), ItemPointerGetOffsetNumber(tid));
  PredicateLockAcquire(&tag);
}

   
                     
   
                                                                    
   
                                                              
                                                   
   
static void
DeleteLockTarget(PREDICATELOCKTARGET *target, uint32 targettaghash)
{
  PREDICATELOCK *predlock;
  SHM_QUEUE *predlocktargetlink;
  PREDICATELOCK *nextpredlock;
  bool found;

  Assert(LWLockHeldByMeInMode(SerializablePredicateLockListLock, LW_EXCLUSIVE));
  Assert(LWLockHeldByMe(PredicateLockHashPartitionLock(targettaghash)));

  predlock = (PREDICATELOCK *)SHMQueueNext(&(target->predicateLocks), &(target->predicateLocks), offsetof(PREDICATELOCK, targetLink));
  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);
  while (predlock)
  {
    predlocktargetlink = &(predlock->targetLink);
    nextpredlock = (PREDICATELOCK *)SHMQueueNext(&(target->predicateLocks), predlocktargetlink, offsetof(PREDICATELOCK, targetLink));

    SHMQueueDelete(&(predlock->xactLink));
    SHMQueueDelete(&(predlock->targetLink));

    hash_search_with_hash_value(PredicateLockHash, &predlock->tag, PredicateLockHashCodeFromTargetHashCode(&predlock->tag, targettaghash), HASH_REMOVE, &found);
    Assert(found);

    predlock = nextpredlock;
  }
  LWLockRelease(SerializableXactHashLock);

                                              
  RemoveTargetIfNoLongerUsed(target, targettaghash);
}

   
                                      
   
                                                                      
                                                                      
                                                                      
                    
   
                                                                       
                                                                     
                                                                           
                       
   
                                                                  
                                                                 
                                                               
                                                                     
                                                             
                                                                       
                                                                    
                                                                     
                                                                       
                                     
   
                                                                   
   
static bool
TransferPredicateLocksToNewTarget(PREDICATELOCKTARGETTAG oldtargettag, PREDICATELOCKTARGETTAG newtargettag, bool removeOld)
{
  uint32 oldtargettaghash;
  LWLock *oldpartitionLock;
  PREDICATELOCKTARGET *oldtarget;
  uint32 newtargettaghash;
  LWLock *newpartitionLock;
  bool found;
  bool outOfShmem = false;

  Assert(LWLockHeldByMeInMode(SerializablePredicateLockListLock, LW_EXCLUSIVE));

  oldtargettaghash = PredicateLockTargetTagHashCode(&oldtargettag);
  newtargettaghash = PredicateLockTargetTagHashCode(&newtargettag);
  oldpartitionLock = PredicateLockHashPartitionLock(oldtargettaghash);
  newpartitionLock = PredicateLockHashPartitionLock(newtargettaghash);

  if (removeOld)
  {
       
                                                                         
                                              
       
    RemoveScratchTarget(false);
  }

     
                                                                    
                                                                            
                     
     
  if (oldpartitionLock < newpartitionLock)
  {
    LWLockAcquire(oldpartitionLock, (removeOld ? LW_EXCLUSIVE : LW_SHARED));
    LWLockAcquire(newpartitionLock, LW_EXCLUSIVE);
  }
  else if (oldpartitionLock > newpartitionLock)
  {
    LWLockAcquire(newpartitionLock, LW_EXCLUSIVE);
    LWLockAcquire(oldpartitionLock, (removeOld ? LW_EXCLUSIVE : LW_SHARED));
  }
  else
  {
    LWLockAcquire(newpartitionLock, LW_EXCLUSIVE);
  }

     
                                                                           
                                                                         
                                                                       
             
     
  oldtarget = hash_search_with_hash_value(PredicateLockTargetHash, &oldtargettag, oldtargettaghash, HASH_FIND, NULL);

  if (oldtarget)
  {
    PREDICATELOCKTARGET *newtarget;
    PREDICATELOCK *oldpredlock;
    PREDICATELOCKTAG newpredlocktag;

    newtarget = hash_search_with_hash_value(PredicateLockTargetHash, &newtargettag, newtargettaghash, HASH_ENTER_NULL, &found);

    if (!newtarget)
    {
                                                        
      outOfShmem = true;
      goto exit;
    }

                                                  
    if (!found)
    {
      SHMQueueInit(&(newtarget->predicateLocks));
    }

    newpredlocktag.myTarget = newtarget;

       
                                                                         
                                
       
    oldpredlock = (PREDICATELOCK *)SHMQueueNext(&(oldtarget->predicateLocks), &(oldtarget->predicateLocks), offsetof(PREDICATELOCK, targetLink));
    LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);
    while (oldpredlock)
    {
      SHM_QUEUE *predlocktargetlink;
      PREDICATELOCK *nextpredlock;
      PREDICATELOCK *newpredlock;
      SerCommitSeqNo oldCommitSeqNo = oldpredlock->commitSeqNo;

      predlocktargetlink = &(oldpredlock->targetLink);
      nextpredlock = (PREDICATELOCK *)SHMQueueNext(&(oldtarget->predicateLocks), predlocktargetlink, offsetof(PREDICATELOCK, targetLink));
      newpredlocktag.myXact = oldpredlock->tag.myXact;

      if (removeOld)
      {
        SHMQueueDelete(&(oldpredlock->xactLink));
        SHMQueueDelete(&(oldpredlock->targetLink));

        hash_search_with_hash_value(PredicateLockHash, &oldpredlock->tag, PredicateLockHashCodeFromTargetHashCode(&oldpredlock->tag, oldtargettaghash), HASH_REMOVE, &found);
        Assert(found);
      }

      newpredlock = (PREDICATELOCK *)hash_search_with_hash_value(PredicateLockHash, &newpredlocktag, PredicateLockHashCodeFromTargetHashCode(&newpredlocktag, newtargettaghash), HASH_ENTER_NULL, &found);
      if (!newpredlock)
      {
                                                                
        LWLockRelease(SerializableXactHashLock);
        DeleteLockTarget(newtarget, newtargettaghash);
        outOfShmem = true;
        goto exit;
      }
      if (!found)
      {
        SHMQueueInsertBefore(&(newtarget->predicateLocks), &(newpredlock->targetLink));
        SHMQueueInsertBefore(&(newpredlocktag.myXact->predicateLocks), &(newpredlock->xactLink));
        newpredlock->commitSeqNo = oldCommitSeqNo;
      }
      else
      {
        if (newpredlock->commitSeqNo < oldCommitSeqNo)
        {
          newpredlock->commitSeqNo = oldCommitSeqNo;
        }
      }

      Assert(newpredlock->commitSeqNo != 0);
      Assert((newpredlock->commitSeqNo == InvalidSerCommitSeqNo) || (newpredlock->tag.myXact == OldCommittedSxact));

      oldpredlock = nextpredlock;
    }
    LWLockRelease(SerializableXactHashLock);

    if (removeOld)
    {
      Assert(SHMQueueEmpty(&oldtarget->predicateLocks));
      RemoveTargetIfNoLongerUsed(oldtarget, oldtargettaghash);
    }
  }

exit:
                                                                
  if (oldpartitionLock < newpartitionLock)
  {
    LWLockRelease(newpartitionLock);
    LWLockRelease(oldpartitionLock);
  }
  else if (oldpartitionLock > newpartitionLock)
  {
    LWLockRelease(oldpartitionLock);
    LWLockRelease(newpartitionLock);
  }
  else
  {
    LWLockRelease(newpartitionLock);
  }

  if (removeOld)
  {
                                                              
    Assert(!outOfShmem);

                                    
    RestoreScratchTarget(false);
  }

  return !outOfShmem;
}

   
                                                                            
                                                                              
                                                                             
                              
   
                                                                         
                                                                       
                                                                           
                                                                            
                                                                         
   
                                                                          
                                                                         
                                                     
   
                                                                       
                                          
   
                                                                               
                                                                           
                                                                              
   
static void
DropAllPredicateLocksFromTable(Relation relation, bool transfer)
{
  HASH_SEQ_STATUS seqstat;
  PREDICATELOCKTARGET *oldtarget;
  PREDICATELOCKTARGET *heaptarget;
  Oid dbId;
  Oid relId;
  Oid heapId;
  int i;
  bool isIndex;
  bool found;
  uint32 heaptargettaghash;

     
                                                                         
                                                                        
                                                                           
                                                           
     
  if (!TransactionIdIsValid(PredXact->SxactGlobalXmin))
  {
    return;
  }

  if (!PredicateLockingNeededForRelation(relation))
  {
    return;
  }

  dbId = relation->rd_node.dbNode;
  relId = relation->rd_id;
  if (relation->rd_index == NULL)
  {
    isIndex = false;
    heapId = relId;
  }
  else
  {
    isIndex = true;
    heapId = relation->rd_index->indrelid;
  }
  Assert(heapId != InvalidOid);
  Assert(transfer || !isIndex);                                    
                                              

                                              
  heaptargettaghash = 0;
  heaptarget = NULL;

                                            
  LWLockAcquire(SerializablePredicateLockListLock, LW_EXCLUSIVE);
  for (i = 0; i < NUM_PREDICATELOCK_PARTITIONS; i++)
  {
    LWLockAcquire(PredicateLockHashPartitionLockByIndex(i), LW_EXCLUSIVE);
  }
  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

     
                                                                          
                                         
     
  if (transfer)
  {
    RemoveScratchTarget(true);
  }

                               
  hash_seq_init(&seqstat, PredicateLockTargetHash);

  while ((oldtarget = (PREDICATELOCKTARGET *)hash_seq_search(&seqstat)))
  {
    PREDICATELOCK *oldpredlock;

       
                                                             
       
    if (GET_PREDICATELOCKTARGETTAG_RELATION(oldtarget->tag) != relId)
    {
      continue;                        
    }
    if (GET_PREDICATELOCKTARGETTAG_DB(oldtarget->tag) != dbId)
    {
      continue;                        
    }
    if (transfer && !isIndex && GET_PREDICATELOCKTARGETTAG_TYPE(oldtarget->tag) == PREDLOCKTAG_RELATION)
    {
      continue;                             
    }

       
                                                                      
                                                                          
                                                                           
                                         
       

       
                                                                          
                     
       
    if (transfer && heaptarget == NULL)
    {
      PREDICATELOCKTARGETTAG heaptargettag;

      SET_PREDICATELOCKTARGETTAG_RELATION(heaptargettag, dbId, heapId);
      heaptargettaghash = PredicateLockTargetTagHashCode(&heaptargettag);
      heaptarget = hash_search_with_hash_value(PredicateLockTargetHash, &heaptargettag, heaptargettaghash, HASH_ENTER, &found);
      if (!found)
      {
        SHMQueueInit(&heaptarget->predicateLocks);
      }
    }

       
                                                                         
                                
       
    oldpredlock = (PREDICATELOCK *)SHMQueueNext(&(oldtarget->predicateLocks), &(oldtarget->predicateLocks), offsetof(PREDICATELOCK, targetLink));
    while (oldpredlock)
    {
      PREDICATELOCK *nextpredlock;
      PREDICATELOCK *newpredlock;
      SerCommitSeqNo oldCommitSeqNo;
      SERIALIZABLEXACT *oldXact;

      nextpredlock = (PREDICATELOCK *)SHMQueueNext(&(oldtarget->predicateLocks), &(oldpredlock->targetLink), offsetof(PREDICATELOCK, targetLink));

         
                                                                      
                                                           
         
      oldCommitSeqNo = oldpredlock->commitSeqNo;
      oldXact = oldpredlock->tag.myXact;

      SHMQueueDelete(&(oldpredlock->xactLink));

         
                                                                       
                                  
         
      hash_search(PredicateLockHash, &oldpredlock->tag, HASH_REMOVE, &found);
      Assert(found);

      if (transfer)
      {
        PREDICATELOCKTAG newpredlocktag;

        newpredlocktag.myTarget = heaptarget;
        newpredlocktag.myXact = oldXact;
        newpredlock = (PREDICATELOCK *)hash_search_with_hash_value(PredicateLockHash, &newpredlocktag, PredicateLockHashCodeFromTargetHashCode(&newpredlocktag, heaptargettaghash), HASH_ENTER, &found);
        if (!found)
        {
          SHMQueueInsertBefore(&(heaptarget->predicateLocks), &(newpredlock->targetLink));
          SHMQueueInsertBefore(&(newpredlocktag.myXact->predicateLocks), &(newpredlock->xactLink));
          newpredlock->commitSeqNo = oldCommitSeqNo;
        }
        else
        {
          if (newpredlock->commitSeqNo < oldCommitSeqNo)
          {
            newpredlock->commitSeqNo = oldCommitSeqNo;
          }
        }

        Assert(newpredlock->commitSeqNo != 0);
        Assert((newpredlock->commitSeqNo == InvalidSerCommitSeqNo) || (newpredlock->tag.myXact == OldCommittedSxact));
      }

      oldpredlock = nextpredlock;
    }

    hash_search(PredicateLockTargetHash, &oldtarget->tag, HASH_REMOVE, &found);
    Assert(found);
  }

                                  
  if (transfer)
  {
    RestoreScratchTarget(true);
  }

                                      
  LWLockRelease(SerializableXactHashLock);
  for (i = NUM_PREDICATELOCK_PARTITIONS - 1; i >= 0; i--)
  {
    LWLockRelease(PredicateLockHashPartitionLockByIndex(i));
  }
  LWLockRelease(SerializablePredicateLockListLock);
}

   
                                        
                                                                     
                                                    
   
void
TransferPredicateLocksToHeapRelation(Relation relation)
{
  DropAllPredicateLocksFromTable(relation, true);
}

   
                           
   
                                                                
                                                     
   
                                                                           
                                                                            
   
                                                                   
                                                                            
                                                                              
                                                 
   
void
PredicateLockPageSplit(Relation relation, BlockNumber oldblkno, BlockNumber newblkno)
{
  PREDICATELOCKTARGETTAG oldtargettag;
  PREDICATELOCKTARGETTAG newtargettag;
  bool success;

     
                                                                         
     
                                                                             
                                                                           
                                                                            
                                                                            
                                                                        
                                                
     
  if (!TransactionIdIsValid(PredXact->SxactGlobalXmin))
  {
    return;
  }

  if (!PredicateLockingNeededForRelation(relation))
  {
    return;
  }

  Assert(oldblkno != newblkno);
  Assert(BlockNumberIsValid(oldblkno));
  Assert(BlockNumberIsValid(newblkno));

  SET_PREDICATELOCKTARGETTAG_PAGE(oldtargettag, relation->rd_node.dbNode, relation->rd_id, oldblkno);
  SET_PREDICATELOCKTARGETTAG_PAGE(newtargettag, relation->rd_node.dbNode, relation->rd_id, newblkno);

  LWLockAcquire(SerializablePredicateLockListLock, LW_EXCLUSIVE);

     
                                                                      
                
     
  success = TransferPredicateLocksToNewTarget(oldtargettag, newtargettag, false);

  if (!success)
  {
       
                                                                      
                                                                 
       

                                                 
    success = GetParentPredicateLockTag(&oldtargettag, &newtargettag);
    Assert(success);

       
                                                          
       
                                                                    
                                                                           
                                                                         
                
       
    success = TransferPredicateLocksToNewTarget(oldtargettag, newtargettag, true);
    Assert(success);
  }

  LWLockRelease(SerializablePredicateLockListLock);
}

   
                             
   
                                                    
                                                     
   
                                                                          
                                                                 
   
void
PredicateLockPageCombine(Relation relation, BlockNumber oldblkno, BlockNumber newblkno)
{
     
                                                                          
                                                                         
                                                                             
                                                                         
                                                                          
                                                                          
                                                                            
                                                                            
                          
     
  PredicateLockPageSplit(relation, oldblkno, newblkno);
}

   
                                                                           
         
   
static void
SetNewSxactGlobalXmin(void)
{
  SERIALIZABLEXACT *sxact;

  Assert(LWLockHeldByMe(SerializableXactHashLock));

  PredXact->SxactGlobalXmin = InvalidTransactionId;
  PredXact->SxactGlobalXminCount = 0;

  for (sxact = FirstPredXact(); sxact != NULL; sxact = NextPredXact(sxact))
  {
    if (!SxactIsRolledBack(sxact) && !SxactIsCommitted(sxact) && sxact != OldCommittedSxact)
    {
      Assert(sxact->xmin != InvalidTransactionId);
      if (!TransactionIdIsValid(PredXact->SxactGlobalXmin) || TransactionIdPrecedes(sxact->xmin, PredXact->SxactGlobalXmin))
      {
        PredXact->SxactGlobalXmin = sxact->xmin;
        PredXact->SxactGlobalXminCount = 1;
      }
      else if (TransactionIdEquals(sxact->xmin, PredXact->SxactGlobalXmin))
      {
        PredXact->SxactGlobalXminCount++;
      }
    }
  }

  OldSerXidSetActiveSerXmin(PredXact->SxactGlobalXmin);
}

   
                          
   
                                                                            
                                                                            
                                                                        
                                  
   
                                                            
   
                                                                      
                                         
   
                                                                         
                                                                           
                  
   
                                                                             
                                                                             
                                                                             
                                                                          
                                                                               
                                                                            
         
   
void
ReleasePredicateLocks(bool isCommit, bool isReadOnlySafe)
{
  bool needToClear;
  RWConflict conflict, nextConflict, possibleUnsafeConflict;
  SERIALIZABLEXACT *roXact;

     
                                                                           
                                                             
                                                                        
                                                                             
                                                                             
                                                                            
                                              
     
  bool topLevelIsDeclaredReadOnly;

                                                                       
  Assert(!(isCommit && isReadOnlySafe));

                                                                       
  if (!isReadOnlySafe)
  {
       
                                                                      
                                                                     
                    
       
    if (IsParallelWorker())
    {
      ReleasePredicateLocksLocal();
      return;
    }

       
                                                                 
                                                           
       
    Assert(!ParallelContextActive());

       
                                                                     
                                                                          
                                                                       
                           
       
    if (SavedSerializableXact != InvalidSerializableXact)
    {
      Assert(MySerializableXact == InvalidSerializableXact);
      MySerializableXact = SavedSerializableXact;
      SavedSerializableXact = InvalidSerializableXact;
      Assert(SxactIsPartiallyReleased(MySerializableXact));
    }
  }

  if (MySerializableXact == InvalidSerializableXact)
  {
    Assert(LocalPredicateLockHash == NULL);
    return;
  }

  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

     
                                                                          
                                                                             
     
  if (isCommit && SxactIsPartiallyReleased(MySerializableXact))
  {
    isCommit = false;
  }

     
                                                                          
                                                                            
                                                                         
                                                                          
     
  if (isReadOnlySafe && IsInParallelMode())
  {
       
                                                                 
                                                    
       
    if (!IsParallelWorker())
    {
      SavedSerializableXact = MySerializableXact;
    }

       
                                                                        
                                                               
                                                                           
                           
       
    if (SxactIsPartiallyReleased(MySerializableXact))
    {
      LWLockRelease(SerializableXactHashLock);
      ReleasePredicateLocksLocal();
      return;
    }
    else
    {
      MySerializableXact->flags |= SXACT_FLAG_PARTIALLY_RELEASED;
                                                                 
    }
  }
  Assert(!isCommit || SxactIsPrepared(MySerializableXact));
  Assert(!isCommit || !SxactIsDoomed(MySerializableXact));
  Assert(!SxactIsCommitted(MySerializableXact));
  Assert(SxactIsPartiallyReleased(MySerializableXact) || !SxactIsRolledBack(MySerializableXact));

                                                               
  Assert(MySerializableXact->pid == 0 || IsolationIsSerializable());

                                                       
  Assert(!SxactIsOnFinishedList(MySerializableXact));

  topLevelIsDeclaredReadOnly = SxactIsReadOnly(MySerializableXact);

     
                                                                        
             
     
                                                                           
                                                              
                                                                        
                                                                            
                                                                             
                               
     
  MySerializableXact->finishedBefore = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);

     
                                                                            
                                                                         
     
  if (isCommit)
  {
    MySerializableXact->flags |= SXACT_FLAG_COMMITTED;
    MySerializableXact->commitSeqNo = ++(PredXact->LastSxactCommitSeqNo);
                                                                          
    if (!MyXactDidWrite)
    {
      MySerializableXact->flags |= SXACT_FLAG_READ_ONLY;
    }
  }
  else
  {
       
                                                                  
                                                                         
                                                                           
                                                                      
              
       
                                                                         
                                                                    
                                                                        
                        
       
    MySerializableXact->flags |= SXACT_FLAG_DOOMED;
    MySerializableXact->flags |= SXACT_FLAG_ROLLED_BACK;

       
                                                                          
                                                                       
                                                                   
                 
       
    MySerializableXact->flags &= ~SXACT_FLAG_PREPARED;
  }

  if (!topLevelIsDeclaredReadOnly)
  {
    Assert(PredXact->WritableSxactCount > 0);
    if (--(PredXact->WritableSxactCount) == 0)
    {
         
                                                                       
                                                                         
                                                                       
                                                                        
                                                                     
                                                                
                                        
         
      PredXact->CanPartialClearThrough = PredXact->LastSxactCommitSeqNo;
    }
  }
  else
  {
       
                                                                         
                                                                      
                                               
       
    possibleUnsafeConflict = (RWConflict)SHMQueueNext(&MySerializableXact->possibleUnsafeConflicts, &MySerializableXact->possibleUnsafeConflicts, offsetof(RWConflictData, inLink));
    while (possibleUnsafeConflict)
    {
      nextConflict = (RWConflict)SHMQueueNext(&MySerializableXact->possibleUnsafeConflicts, &possibleUnsafeConflict->inLink, offsetof(RWConflictData, inLink));

      Assert(!SxactIsReadOnly(possibleUnsafeConflict->sxactOut));
      Assert(MySerializableXact == possibleUnsafeConflict->sxactIn);

      ReleaseRWConflict(possibleUnsafeConflict);

      possibleUnsafeConflict = nextConflict;
    }
  }

                                                             
  if (isCommit && !SxactIsReadOnly(MySerializableXact) && SxactHasSummaryConflictOut(MySerializableXact))
  {
       
                                                                         
                                                                 
       
    MySerializableXact->SeqNo.earliestOutConflictCommit = FirstNormalSerCommitSeqNo;
    MySerializableXact->flags |= SXACT_FLAG_CONFLICT_OUT;
  }

     
                                                                           
                                                                       
                                        
     
  conflict = (RWConflict)SHMQueueNext(&MySerializableXact->outConflicts, &MySerializableXact->outConflicts, offsetof(RWConflictData, outLink));
  while (conflict)
  {
    nextConflict = (RWConflict)SHMQueueNext(&MySerializableXact->outConflicts, &conflict->outLink, offsetof(RWConflictData, outLink));

    if (isCommit && !SxactIsReadOnly(MySerializableXact) && SxactIsCommitted(conflict->sxactIn))
    {
      if ((MySerializableXact->flags & SXACT_FLAG_CONFLICT_OUT) == 0 || conflict->sxactIn->prepareSeqNo < MySerializableXact->SeqNo.earliestOutConflictCommit)
      {
        MySerializableXact->SeqNo.earliestOutConflictCommit = conflict->sxactIn->prepareSeqNo;
      }
      MySerializableXact->flags |= SXACT_FLAG_CONFLICT_OUT;
    }

    if (!isCommit || SxactIsCommitted(conflict->sxactIn) || (conflict->sxactIn->SeqNo.lastCommitBeforeSnapshot >= PredXact->LastSxactCommitSeqNo))
    {
      ReleaseRWConflict(conflict);
    }

    conflict = nextConflict;
  }

     
                                                                           
                                         
     
  conflict = (RWConflict)SHMQueueNext(&MySerializableXact->inConflicts, &MySerializableXact->inConflicts, offsetof(RWConflictData, inLink));
  while (conflict)
  {
    nextConflict = (RWConflict)SHMQueueNext(&MySerializableXact->inConflicts, &conflict->inLink, offsetof(RWConflictData, inLink));

    if (!isCommit || SxactIsCommitted(conflict->sxactOut) || SxactIsReadOnly(conflict->sxactOut))
    {
      ReleaseRWConflict(conflict);
    }

    conflict = nextConflict;
  }

  if (!topLevelIsDeclaredReadOnly)
  {
       
                                                                           
                                                                    
                                                                           
                                                  
       
    possibleUnsafeConflict = (RWConflict)SHMQueueNext(&MySerializableXact->possibleUnsafeConflicts, &MySerializableXact->possibleUnsafeConflicts, offsetof(RWConflictData, outLink));
    while (possibleUnsafeConflict)
    {
      nextConflict = (RWConflict)SHMQueueNext(&MySerializableXact->possibleUnsafeConflicts, &possibleUnsafeConflict->outLink, offsetof(RWConflictData, outLink));

      roXact = possibleUnsafeConflict->sxactIn;
      Assert(MySerializableXact == possibleUnsafeConflict->sxactOut);
      Assert(SxactIsReadOnly(roXact));

                                         
      if (isCommit && MyXactDidWrite && SxactHasConflictOut(MySerializableXact) && (MySerializableXact->SeqNo.earliestOutConflictCommit <= roXact->SeqNo.lastCommitBeforeSnapshot))
      {
           
                                                                      
                                          
           
        FlagSxactUnsafe(roXact);
      }
      else
      {
        ReleaseRWConflict(possibleUnsafeConflict);

           
                                                                    
                                                                       
                                                              
           
        if (SHMQueueEmpty(&roXact->possibleUnsafeConflicts))
        {
          roXact->flags |= SXACT_FLAG_RO_SAFE;
        }
      }

         
                                                                        
                                                  
         
      if (SxactIsDeferrableWaiting(roXact) && (SxactIsROUnsafe(roXact) || SxactIsROSafe(roXact)))
      {
        ProcSendSignal(roXact->pid);
      }

      possibleUnsafeConflict = nextConflict;
    }
  }

     
                                                                            
                                                                            
                                                                         
                                                                            
                   
     
  needToClear = false;
  if (TransactionIdEquals(MySerializableXact->xmin, PredXact->SxactGlobalXmin))
  {
    Assert(PredXact->SxactGlobalXminCount > 0);
    if (--(PredXact->SxactGlobalXminCount) == 0)
    {
      SetNewSxactGlobalXmin();
      needToClear = true;
    }
  }

  LWLockRelease(SerializableXactHashLock);

  LWLockAcquire(SerializableFinishedListLock, LW_EXCLUSIVE);

                                                                        
  if (isCommit)
  {
    SHMQueueInsertBefore(FinishedSerializableTransactions, &MySerializableXact->finishedLink);
  }

     
                                                                           
                                                                             
                                                                             
                                                                       
     
  if (!isCommit)
  {
    ReleaseOneSerializableXact(MySerializableXact, isReadOnlySafe && IsInParallelMode(), false);
  }

  LWLockRelease(SerializableFinishedListLock);

  if (needToClear)
  {
    ClearOldPredicateLocks();
  }

  ReleasePredicateLocksLocal();
}

static void
ReleasePredicateLocksLocal(void)
{
  MySerializableXact = InvalidSerializableXact;
  MyXactDidWrite = false;

                                         
  if (LocalPredicateLockHash != NULL)
  {
    hash_destroy(LocalPredicateLockHash);
    LocalPredicateLockHash = NULL;
  }
}

   
                                                                              
                                                      
   
static void
ClearOldPredicateLocks(void)
{
  SERIALIZABLEXACT *finishedSxact;
  PREDICATELOCK *predlock;

     
                                                                             
                                                           
     
  LWLockAcquire(SerializableFinishedListLock, LW_EXCLUSIVE);
  finishedSxact = (SERIALIZABLEXACT *)SHMQueueNext(FinishedSerializableTransactions, FinishedSerializableTransactions, offsetof(SERIALIZABLEXACT, finishedLink));
  LWLockAcquire(SerializableXactHashLock, LW_SHARED);
  while (finishedSxact)
  {
    SERIALIZABLEXACT *nextSxact;

    nextSxact = (SERIALIZABLEXACT *)SHMQueueNext(FinishedSerializableTransactions, &(finishedSxact->finishedLink), offsetof(SERIALIZABLEXACT, finishedLink));
    if (!TransactionIdIsValid(PredXact->SxactGlobalXmin) || TransactionIdPrecedesOrEquals(finishedSxact->finishedBefore, PredXact->SxactGlobalXmin))
    {
         
                                                                       
                                                        
         
      LWLockRelease(SerializableXactHashLock);
      SHMQueueDelete(&(finishedSxact->finishedLink));
      ReleaseOneSerializableXact(finishedSxact, false, false);
      LWLockAcquire(SerializableXactHashLock, LW_SHARED);
    }
    else if (finishedSxact->commitSeqNo > PredXact->HavePartialClearedThrough && finishedSxact->commitSeqNo <= PredXact->CanPartialClearThrough)
    {
         
                                                                      
                                                                      
                    
         
      LWLockRelease(SerializableXactHashLock);

      if (SxactIsReadOnly(finishedSxact))
      {
                                                             
        SHMQueueDelete(&(finishedSxact->finishedLink));
        ReleaseOneSerializableXact(finishedSxact, false, false);
      }
      else
      {
           
                                                                      
                                                                 
                                          
           
        ReleaseOneSerializableXact(finishedSxact, true, false);
      }

      PredXact->HavePartialClearedThrough = finishedSxact->commitSeqNo;
      LWLockAcquire(SerializableXactHashLock, LW_SHARED);
    }
    else
    {
                              
      break;
    }
    finishedSxact = nextSxact;
  }
  LWLockRelease(SerializableXactHashLock);

     
                                                                            
     
  LWLockAcquire(SerializablePredicateLockListLock, LW_SHARED);
  predlock = (PREDICATELOCK *)SHMQueueNext(&OldCommittedSxact->predicateLocks, &OldCommittedSxact->predicateLocks, offsetof(PREDICATELOCK, xactLink));
  while (predlock)
  {
    PREDICATELOCK *nextpredlock;
    bool canDoPartialCleanup;

    nextpredlock = (PREDICATELOCK *)SHMQueueNext(&OldCommittedSxact->predicateLocks, &predlock->xactLink, offsetof(PREDICATELOCK, xactLink));

    LWLockAcquire(SerializableXactHashLock, LW_SHARED);
    Assert(predlock->commitSeqNo != 0);
    Assert(predlock->commitSeqNo != InvalidSerCommitSeqNo);
    canDoPartialCleanup = (predlock->commitSeqNo <= PredXact->CanPartialClearThrough);
    LWLockRelease(SerializableXactHashLock);

       
                                                                         
                       
       
    if (canDoPartialCleanup)
    {
      PREDICATELOCKTAG tag;
      PREDICATELOCKTARGET *target;
      PREDICATELOCKTARGETTAG targettag;
      uint32 targettaghash;
      LWLock *partitionLock;

      tag = predlock->tag;
      target = tag.myTarget;
      targettag = target->tag;
      targettaghash = PredicateLockTargetTagHashCode(&targettag);
      partitionLock = PredicateLockHashPartitionLock(targettaghash);

      LWLockAcquire(partitionLock, LW_EXCLUSIVE);

      SHMQueueDelete(&(predlock->targetLink));
      SHMQueueDelete(&(predlock->xactLink));

      hash_search_with_hash_value(PredicateLockHash, &tag, PredicateLockHashCodeFromTargetHashCode(&tag, targettaghash), HASH_REMOVE, NULL);
      RemoveTargetIfNoLongerUsed(target, targettaghash);

      LWLockRelease(partitionLock);
    }

    predlock = nextpredlock;
  }

  LWLockRelease(SerializablePredicateLockListLock);
  LWLockRelease(SerializableFinishedListLock);
}

   
                                                                       
                                                                           
                                                                         
                                                                           
                                                                        
                           
   
                                                                        
                                                                         
                                                                            
                                                                         
   
                                                                          
                                                                         
                                                                    
                                                                        
                           
   
static void
ReleaseOneSerializableXact(SERIALIZABLEXACT *sxact, bool partial, bool summarize)
{
  PREDICATELOCK *predlock;
  SERIALIZABLEXIDTAG sxidtag;
  RWConflict conflict, nextConflict;

  Assert(sxact != NULL);
  Assert(SxactIsRolledBack(sxact) || SxactIsCommitted(sxact));
  Assert(partial || !SxactIsOnFinishedList(sxact));
  Assert(LWLockHeldByMe(SerializableFinishedListLock));

     
                                                                          
                                                     
     
  LWLockAcquire(SerializablePredicateLockListLock, LW_SHARED);
  if (IsInParallelMode())
  {
    LWLockAcquire(&sxact->predicateLockListLock, LW_EXCLUSIVE);
  }
  predlock = (PREDICATELOCK *)SHMQueueNext(&(sxact->predicateLocks), &(sxact->predicateLocks), offsetof(PREDICATELOCK, xactLink));
  while (predlock)
  {
    PREDICATELOCK *nextpredlock;
    PREDICATELOCKTAG tag;
    SHM_QUEUE *targetLink;
    PREDICATELOCKTARGET *target;
    PREDICATELOCKTARGETTAG targettag;
    uint32 targettaghash;
    LWLock *partitionLock;

    nextpredlock = (PREDICATELOCK *)SHMQueueNext(&(sxact->predicateLocks), &(predlock->xactLink), offsetof(PREDICATELOCK, xactLink));

    tag = predlock->tag;
    targetLink = &(predlock->targetLink);
    target = tag.myTarget;
    targettag = target->tag;
    targettaghash = PredicateLockTargetTagHashCode(&targettag);
    partitionLock = PredicateLockHashPartitionLock(targettaghash);

    LWLockAcquire(partitionLock, LW_EXCLUSIVE);

    SHMQueueDelete(targetLink);

    hash_search_with_hash_value(PredicateLockHash, &tag, PredicateLockHashCodeFromTargetHashCode(&tag, targettaghash), HASH_REMOVE, NULL);
    if (summarize)
    {
      bool found;

                                             
      tag.myXact = OldCommittedSxact;
      predlock = hash_search_with_hash_value(PredicateLockHash, &tag, PredicateLockHashCodeFromTargetHashCode(&tag, targettaghash), HASH_ENTER_NULL, &found);
      if (!predlock)
      {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory"), errhint("You might need to increase max_pred_locks_per_transaction.")));
      }
      if (found)
      {
        Assert(predlock->commitSeqNo != 0);
        Assert(predlock->commitSeqNo != InvalidSerCommitSeqNo);
        if (predlock->commitSeqNo < sxact->commitSeqNo)
        {
          predlock->commitSeqNo = sxact->commitSeqNo;
        }
      }
      else
      {
        SHMQueueInsertBefore(&(target->predicateLocks), &(predlock->targetLink));
        SHMQueueInsertBefore(&(OldCommittedSxact->predicateLocks), &(predlock->xactLink));
        predlock->commitSeqNo = sxact->commitSeqNo;
      }
    }
    else
    {
      RemoveTargetIfNoLongerUsed(target, targettaghash);
    }

    LWLockRelease(partitionLock);

    predlock = nextpredlock;
  }

     
                                                                       
                       
     
  SHMQueueInit(&sxact->predicateLocks);

  if (IsInParallelMode())
  {
    LWLockRelease(&sxact->predicateLockListLock);
  }
  LWLockRelease(SerializablePredicateLockListLock);

  sxidtag.xid = sxact->topXid;
  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

                                                           
  if (!partial)
  {
    conflict = (RWConflict)SHMQueueNext(&sxact->outConflicts, &sxact->outConflicts, offsetof(RWConflictData, outLink));
    while (conflict)
    {
      nextConflict = (RWConflict)SHMQueueNext(&sxact->outConflicts, &conflict->outLink, offsetof(RWConflictData, outLink));
      if (summarize)
      {
        conflict->sxactIn->flags |= SXACT_FLAG_SUMMARY_CONFLICT_IN;
      }
      ReleaseRWConflict(conflict);
      conflict = nextConflict;
    }
  }

                                
  conflict = (RWConflict)SHMQueueNext(&sxact->inConflicts, &sxact->inConflicts, offsetof(RWConflictData, inLink));
  while (conflict)
  {
    nextConflict = (RWConflict)SHMQueueNext(&sxact->inConflicts, &conflict->inLink, offsetof(RWConflictData, inLink));
    if (summarize)
    {
      conflict->sxactOut->flags |= SXACT_FLAG_SUMMARY_CONFLICT_OUT;
    }
    ReleaseRWConflict(conflict);
    conflict = nextConflict;
  }

                                                                             
  if (!partial)
  {
    if (sxidtag.xid != InvalidTransactionId)
    {
      hash_search(SerializableXidHash, &sxidtag, HASH_REMOVE, NULL);
    }
    ReleasePredXact(sxact);
  }

  LWLockRelease(SerializableXactHashLock);
}

   
                                                                    
                                       
   
                                                                          
                                                                         
                 
   
static bool
XidIsConcurrent(TransactionId xid)
{
  Snapshot snap;
  uint32 i;

  Assert(TransactionIdIsValid(xid));
  Assert(!TransactionIdEquals(xid, GetTopTransactionIdIfAny()));

  snap = GetTransactionSnapshot();

  if (TransactionIdPrecedes(xid, snap->xmin))
  {
    return false;
  }

  if (TransactionIdFollowsOrEquals(xid, snap->xmax))
  {
    return true;
  }

  for (i = 0; i < snap->xcnt; i++)
  {
    if (xid == snap->xip[i])
    {
      return true;
    }
  }

  return false;
}

   
                                   
                                                                         
                                                                        
                                                              
                                                              
   
                                                                             
                                                                          
                                                                            
                                
   
                                                                          
                                                                           
                                                                            
                                                                     
   
void
CheckForSerializableConflictOut(bool visible, Relation relation, HeapTuple tuple, Buffer buffer, Snapshot snapshot)
{
  TransactionId xid;
  SERIALIZABLEXIDTAG sxidtag;
  SERIALIZABLEXID *sxid;
  SERIALIZABLEXACT *sxact;
  HTSV_Result htsvResult;

  if (!SerializationNeededForRead(relation, snapshot))
  {
    return;
  }

                                                                     
  if (SxactIsDoomed(MySerializableXact))
  {
    ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on identification as a pivot, during conflict out checking."), errhint("The transaction might succeed if retried.")));
  }

     
                                                                        
                                                                         
                                                                          
                                                                             
                          
     
                                                                             
                                                                            
                                                  
     
  htsvResult = HeapTupleSatisfiesVacuum(tuple, TransactionXmin, buffer);
  switch (htsvResult)
  {
  case HEAPTUPLE_LIVE:
    if (visible)
    {
      return;
    }
    xid = HeapTupleHeaderGetXmin(tuple->t_data);
    break;
  case HEAPTUPLE_RECENTLY_DEAD:
  case HEAPTUPLE_DELETE_IN_PROGRESS:
    if (visible)
    {
      xid = HeapTupleHeaderGetUpdateXid(tuple->t_data);
    }
    else
    {
      xid = HeapTupleHeaderGetXmin(tuple->t_data);
    }

    if (TransactionIdPrecedes(xid, TransactionXmin))
    {
                                                
      Assert(!visible);
      return;
    }
    break;
  case HEAPTUPLE_INSERT_IN_PROGRESS:
    xid = HeapTupleHeaderGetXmin(tuple->t_data);
    break;
  case HEAPTUPLE_DEAD:
    Assert(!visible);
    return;
  default:

       
                                                                       
                                                               
                                          
       
    elog(ERROR, "unrecognized return value from HeapTupleSatisfiesVacuum: %u", htsvResult);

       
                                                                      
                                                                    
                                                                   
                     
       
    xid = InvalidTransactionId;
  }
  Assert(TransactionIdIsValid(xid));
  Assert(TransactionIdFollowsOrEquals(xid, TransactionXmin));

     
                                                                            
                          
     
  if (TransactionIdEquals(xid, GetTopTransactionIdIfAny()))
  {
    return;
  }
  xid = SubTransGetTopmostTransaction(xid);
  if (TransactionIdPrecedes(xid, TransactionXmin))
  {
    return;
  }
  if (TransactionIdEquals(xid, GetTopTransactionIdIfAny()))
  {
    return;
  }

     
                                                          
     
  sxidtag.xid = xid;
  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);
  sxid = (SERIALIZABLEXID *)hash_search(SerializableXidHash, &sxidtag, HASH_FIND, NULL);
  if (!sxid)
  {
       
                                                                           
                                                                        
       
    SerCommitSeqNo conflictCommitSeqNo;

    conflictCommitSeqNo = OldSerXidGetMinConflictCommitSeqNo(xid);
    if (conflictCommitSeqNo != 0)
    {
      if (conflictCommitSeqNo != InvalidSerCommitSeqNo && (!SxactIsReadOnly(MySerializableXact) || conflictCommitSeqNo <= MySerializableXact->SeqNo.lastCommitBeforeSnapshot))
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on conflict out to old pivot %u.", xid), errhint("The transaction might succeed if retried.")));
      }

      if (SxactHasSummaryConflictIn(MySerializableXact) || !SHMQueueEmpty(&MySerializableXact->inConflicts))
      {
        ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on identification as a pivot, with conflict out to old committed transaction %u.", xid), errhint("The transaction might succeed if retried.")));
      }

      MySerializableXact->flags |= SXACT_FLAG_SUMMARY_CONFLICT_OUT;
    }

                                                           
    LWLockRelease(SerializableXactHashLock);
    return;
  }
  sxact = sxid->myXact;
  Assert(TransactionIdEquals(sxact->topXid, xid));
  if (sxact == MySerializableXact || SxactIsDoomed(sxact))
  {
                                                                           
    LWLockRelease(SerializableXactHashLock);
    return;
  }

     
                                                                           
                                                                    
                                                                             
                                                              
     
  if (SxactHasSummaryConflictOut(sxact))
  {
    if (!SxactIsPrepared(sxact))
    {
      sxact->flags |= SXACT_FLAG_DOOMED;
      LWLockRelease(SerializableXactHashLock);
      return;
    }
    else
    {
      LWLockRelease(SerializableXactHashLock);
      ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on conflict out to old pivot."), errhint("The transaction might succeed if retried.")));
    }
  }

     
                                                                        
                                                                         
                                       
     
  if (SxactIsReadOnly(MySerializableXact) && SxactIsCommitted(sxact) && !SxactHasSummaryConflictOut(sxact) && (!SxactHasConflictOut(sxact) || MySerializableXact->SeqNo.lastCommitBeforeSnapshot < sxact->SeqNo.earliestOutConflictCommit))
  {
                                                                       
    LWLockRelease(SerializableXactHashLock);
    return;
  }

  if (!XidIsConcurrent(xid))
  {
                                                              
    LWLockRelease(SerializableXactHashLock);
    return;
  }

  if (RWConflictExists(MySerializableXact, sxact))
  {
                                                               
    LWLockRelease(SerializableXactHashLock);
    return;
  }

     
                                                                         
                                  
     
  FlagRWConflict(MySerializableXact, sxact);
  LWLockRelease(SerializableXactHashLock);
}

   
                                                                            
                                     
   
static void
CheckTargetForConflictsIn(PREDICATELOCKTARGETTAG *targettag)
{
  uint32 targettaghash;
  LWLock *partitionLock;
  PREDICATELOCKTARGET *target;
  PREDICATELOCK *predlock;
  PREDICATELOCK *mypredlock = NULL;
  PREDICATELOCKTAG mypredlocktag;

  Assert(MySerializableXact != InvalidSerializableXact);

     
                                                                             
     
  targettaghash = PredicateLockTargetTagHashCode(targettag);
  partitionLock = PredicateLockHashPartitionLock(targettaghash);
  LWLockAcquire(partitionLock, LW_SHARED);
  target = (PREDICATELOCKTARGET *)hash_search_with_hash_value(PredicateLockTargetHash, targettag, targettaghash, HASH_FIND, NULL);
  if (!target)
  {
                                                          
    LWLockRelease(partitionLock);
    return;
  }

     
                                                                       
                                           
     
  predlock = (PREDICATELOCK *)SHMQueueNext(&(target->predicateLocks), &(target->predicateLocks), offsetof(PREDICATELOCK, targetLink));
  LWLockAcquire(SerializableXactHashLock, LW_SHARED);
  while (predlock)
  {
    SHM_QUEUE *predlocktargetlink;
    PREDICATELOCK *nextpredlock;
    SERIALIZABLEXACT *sxact;

    predlocktargetlink = &(predlock->targetLink);
    nextpredlock = (PREDICATELOCK *)SHMQueueNext(&(target->predicateLocks), predlocktargetlink, offsetof(PREDICATELOCK, targetLink));

    sxact = predlock->tag.myXact;
    if (sxact == MySerializableXact)
    {
         
                                                                   
                                                                         
                                                                        
                                                                         
         
                                                                        
                                                                  
                                            
         
      if (!IsSubTransaction() && GET_PREDICATELOCKTARGETTAG_OFFSET(*targettag))
      {
        mypredlock = predlock;
        mypredlocktag = predlock->tag;
      }
    }
    else if (!SxactIsDoomed(sxact) && (!SxactIsCommitted(sxact) || TransactionIdPrecedes(GetTransactionSnapshot()->xmin, sxact->finishedBefore)) && !RWConflictExists(sxact, MySerializableXact))
    {
      LWLockRelease(SerializableXactHashLock);
      LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

         
                                                                 
                                                  
         
      if (!SxactIsDoomed(sxact) && (!SxactIsCommitted(sxact) || TransactionIdPrecedes(GetTransactionSnapshot()->xmin, sxact->finishedBefore)) && !RWConflictExists(sxact, MySerializableXact))
      {
        FlagRWConflict(sxact, MySerializableXact);
      }

      LWLockRelease(SerializableXactHashLock);
      LWLockAcquire(SerializableXactHashLock, LW_SHARED);
    }

    predlock = nextpredlock;
  }
  LWLockRelease(SerializableXactHashLock);
  LWLockRelease(partitionLock);

     
                                                                       
     
                                                                          
                                                                        
                                                                            
                                                         
     
  if (mypredlock != NULL)
  {
    uint32 predlockhashcode;
    PREDICATELOCK *rmpredlock;

    LWLockAcquire(SerializablePredicateLockListLock, LW_SHARED);
    if (IsInParallelMode())
    {
      LWLockAcquire(&MySerializableXact->predicateLockListLock, LW_EXCLUSIVE);
    }
    LWLockAcquire(partitionLock, LW_EXCLUSIVE);
    LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

       
                                                                          
                                                                         
                                        
       
    predlockhashcode = PredicateLockHashCodeFromTargetHashCode(&mypredlocktag, targettaghash);
    rmpredlock = (PREDICATELOCK *)hash_search_with_hash_value(PredicateLockHash, &mypredlocktag, predlockhashcode, HASH_FIND, NULL);
    if (rmpredlock != NULL)
    {
      Assert(rmpredlock == mypredlock);

      SHMQueueDelete(&(mypredlock->targetLink));
      SHMQueueDelete(&(mypredlock->xactLink));

      rmpredlock = (PREDICATELOCK *)hash_search_with_hash_value(PredicateLockHash, &mypredlocktag, predlockhashcode, HASH_REMOVE, NULL);
      Assert(rmpredlock == mypredlock);

      RemoveTargetIfNoLongerUsed(target, targettaghash);
    }

    LWLockRelease(SerializableXactHashLock);
    LWLockRelease(partitionLock);
    if (IsInParallelMode())
    {
      LWLockRelease(&MySerializableXact->predicateLockListLock);
    }
    LWLockRelease(SerializablePredicateLockListLock);

    if (rmpredlock != NULL)
    {
         
                                                                      
                                                                     
                                        
         
      hash_search_with_hash_value(LocalPredicateLockHash, targettag, targettaghash, HASH_REMOVE, NULL);

      DecrementParentLocks(targettag);
    }
  }
}

   
                                  
                                                                     
                                                                       
   
                                                                       
   
                                                                       
                                                                          
                 
   
void
CheckForSerializableConflictIn(Relation relation, HeapTuple tuple, Buffer buffer)
{
  PREDICATELOCKTARGETTAG targettag;

  if (!SerializationNeededForWrite(relation))
  {
    return;
  }

                                                                     
  if (SxactIsDoomed(MySerializableXact))
  {
    ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on identification as a pivot, during conflict in checking."), errhint("The transaction might succeed if retried.")));
  }

     
                                                                      
                         
     
  MyXactDidWrite = true;

     
                                                                            
                                                                           
                                                                            
                                     
     
                                                                          
                                                                         
     
  if (tuple != NULL)
  {
    SET_PREDICATELOCKTARGETTAG_TUPLE(targettag, relation->rd_node.dbNode, relation->rd_id, ItemPointerGetBlockNumber(&(tuple->t_self)), ItemPointerGetOffsetNumber(&(tuple->t_self)));
    CheckTargetForConflictsIn(&targettag);
  }

  if (BufferIsValid(buffer))
  {
    SET_PREDICATELOCKTARGETTAG_PAGE(targettag, relation->rd_node.dbNode, relation->rd_id, BufferGetBlockNumber(buffer));
    CheckTargetForConflictsIn(&targettag);
  }

  SET_PREDICATELOCKTARGETTAG_RELATION(targettag, relation->rd_node.dbNode, relation->rd_id);
  CheckTargetForConflictsIn(&targettag);
}

   
                                       
                                                                      
                                                                       
                                                               
   
                                                                       
                                                                             
                                                                            
                                                                         
        
   
                                                                             
                                                                            
                                                                             
                                                                            
                                                                          
                                                                         
   
                                                                            
                                                                         
                                                                              
                                                                              
                                                                        
                                                                            
                                                                         
   
void
CheckTableForSerializableConflictIn(Relation relation)
{
  HASH_SEQ_STATUS seqstat;
  PREDICATELOCKTARGET *target;
  Oid dbId;
  Oid heapId;
  int i;

     
                                                                         
                                                                        
                                                                           
                                                           
     
  if (!TransactionIdIsValid(PredXact->SxactGlobalXmin))
  {
    return;
  }

  if (!SerializationNeededForWrite(relation))
  {
    return;
  }

     
                                                                      
                         
     
  MyXactDidWrite = true;

  Assert(relation->rd_index == NULL);                            

  dbId = relation->rd_node.dbNode;
  heapId = relation->rd_id;

  LWLockAcquire(SerializablePredicateLockListLock, LW_EXCLUSIVE);
  for (i = 0; i < NUM_PREDICATELOCK_PARTITIONS; i++)
  {
    LWLockAcquire(PredicateLockHashPartitionLockByIndex(i), LW_SHARED);
  }
  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

                                
  hash_seq_init(&seqstat, PredicateLockTargetHash);

  while ((target = (PREDICATELOCKTARGET *)hash_seq_search(&seqstat)))
  {
    PREDICATELOCK *predlock;

       
                                                             
       
    if (GET_PREDICATELOCKTARGETTAG_RELATION(target->tag) != heapId)
    {
      continue;                        
    }
    if (GET_PREDICATELOCKTARGETTAG_DB(target->tag) != dbId)
    {
      continue;                        
    }

       
                                                              
       
    predlock = (PREDICATELOCK *)SHMQueueNext(&(target->predicateLocks), &(target->predicateLocks), offsetof(PREDICATELOCK, targetLink));
    while (predlock)
    {
      PREDICATELOCK *nextpredlock;

      nextpredlock = (PREDICATELOCK *)SHMQueueNext(&(target->predicateLocks), &(predlock->targetLink), offsetof(PREDICATELOCK, targetLink));

      if (predlock->tag.myXact != MySerializableXact && !RWConflictExists(predlock->tag.myXact, MySerializableXact))
      {
        FlagRWConflict(predlock->tag.myXact, MySerializableXact);
      }

      predlock = nextpredlock;
    }
  }

                                      
  LWLockRelease(SerializableXactHashLock);
  for (i = NUM_PREDICATELOCK_PARTITIONS - 1; i >= 0; i--)
  {
    LWLockRelease(PredicateLockHashPartitionLockByIndex(i));
  }
  LWLockRelease(SerializablePredicateLockListLock);
}

   
                                                               
   
                                                                    
                               
   
static void
FlagRWConflict(SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer)
{
  Assert(reader != writer);

                                                   
  OnConflict_CheckForSerializationFailure(reader, writer);

                                          
  if (reader == OldCommittedSxact)
  {
    writer->flags |= SXACT_FLAG_SUMMARY_CONFLICT_IN;
  }
  else if (writer == OldCommittedSxact)
  {
    reader->flags |= SXACT_FLAG_SUMMARY_CONFLICT_OUT;
  }
  else
  {
    SetRWConflict(reader, writer);
  }
}

                                                                               
                                                                               
                                                                     
                       
   
                                                                            
                            
   
                                    
                
   
                                        
   
                                                                          
                                                                             
                          
                                                                               
   
static void
OnConflict_CheckForSerializationFailure(const SERIALIZABLEXACT *reader, SERIALIZABLEXACT *writer)
{
  bool failure;
  RWConflict conflict;

  Assert(LWLockHeldByMe(SerializableXactHashLock));

  failure = false;

                                                                             
                                                                     
                                                            
     
                             
                
     
                                                                        
                                                   
                                                                             
     
  if (SxactIsCommitted(writer) && (SxactHasConflictOut(writer) || SxactHasSummaryConflictOut(writer)))
  {
    failure = true;
  }

                                                                             
                                                                      
                                                         
     
                             
                
     
                                                                 
                                      
                                      
                                                                           
                                                                   
     
                                                                       
                                                                       
                                                                         
               
                                                                             
     
  if (!failure)
  {
    if (SxactHasSummaryConflictOut(writer))
    {
      failure = true;
      conflict = NULL;
    }
    else
    {
      conflict = (RWConflict)SHMQueueNext(&writer->outConflicts, &writer->outConflicts, offsetof(RWConflictData, outLink));
    }
    while (conflict)
    {
      SERIALIZABLEXACT *t2 = conflict->sxactIn;

      if (SxactIsPrepared(t2) && (!SxactIsCommitted(reader) || t2->prepareSeqNo <= reader->commitSeqNo) && (!SxactIsCommitted(writer) || t2->prepareSeqNo <= writer->commitSeqNo) && (!SxactIsReadOnly(reader) || t2->prepareSeqNo <= reader->SeqNo.lastCommitBeforeSnapshot))
      {
        failure = true;
        break;
      }
      conflict = (RWConflict)SHMQueueNext(&writer->outConflicts, &conflict->outLink, offsetof(RWConflictData, outLink));
    }
  }

                                                                             
                                                               
                                     
     
                             
                  
     
                                                                            
                 
                                      
                                                
                                                                             
     
  if (!failure && SxactIsPrepared(writer) && !SxactIsReadOnly(reader))
  {
    if (SxactHasSummaryConflictIn(reader))
    {
      failure = true;
      conflict = NULL;
    }
    else
    {
      conflict = (RWConflict)SHMQueueNext(&reader->inConflicts, &reader->inConflicts, offsetof(RWConflictData, inLink));
    }
    while (conflict)
    {
      SERIALIZABLEXACT *t0 = conflict->sxactOut;

      if (!SxactIsDoomed(t0) && (!SxactIsCommitted(t0) || t0->commitSeqNo >= writer->prepareSeqNo) && (!SxactIsReadOnly(t0) || t0->SeqNo.lastCommitBeforeSnapshot >= writer->prepareSeqNo))
      {
        failure = true;
        break;
      }
      conflict = (RWConflict)SHMQueueNext(&reader->inConflicts, &conflict->inLink, offsetof(RWConflictData, inLink));
    }
  }

  if (failure)
  {
       
                                                                      
                                                                        
                                                                        
                                                                           
                                                                      
                                                       
       
    if (MySerializableXact == writer)
    {
      LWLockRelease(SerializableXactHashLock);
      ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on identification as a pivot, during write."), errhint("The transaction might succeed if retried.")));
    }
    else if (SxactIsPrepared(writer))
    {
      LWLockRelease(SerializableXactHashLock);

                                                             
      Assert(MySerializableXact == reader);
      ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on conflict out to pivot %u, during read.", writer->topXid), errhint("The transaction might succeed if retried.")));
    }
    writer->flags |= SXACT_FLAG_DOOMED;
  }
}

   
                                           
                                                                 
               
   
                                                                          
                                                                          
                                                                    
              
   
                                                                       
                                                                         
                                                                    
                                                                    
                                                                        
   
void
PreCommit_CheckForSerializationFailure(void)
{
  RWConflict nearConflict;

  if (MySerializableXact == InvalidSerializableXact)
  {
    return;
  }

  Assert(IsolationIsSerializable());

  LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);

                                                                     
  if (SxactIsDoomed(MySerializableXact))
  {
    Assert(!SxactIsPartiallyReleased(MySerializableXact));
    LWLockRelease(SerializableXactHashLock);
    ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on identification as a pivot, during commit attempt."), errhint("The transaction might succeed if retried.")));
  }

  nearConflict = (RWConflict)SHMQueueNext(&MySerializableXact->inConflicts, &MySerializableXact->inConflicts, offsetof(RWConflictData, inLink));
  while (nearConflict)
  {
    if (!SxactIsCommitted(nearConflict->sxactOut) && !SxactIsDoomed(nearConflict->sxactOut))
    {
      RWConflict farConflict;

      farConflict = (RWConflict)SHMQueueNext(&nearConflict->sxactOut->inConflicts, &nearConflict->sxactOut->inConflicts, offsetof(RWConflictData, inLink));
      while (farConflict)
      {
        if (farConflict->sxactOut == MySerializableXact || (!SxactIsCommitted(farConflict->sxactOut) && !SxactIsReadOnly(farConflict->sxactOut) && !SxactIsDoomed(farConflict->sxactOut)))
        {
             
                                                                     
                                                                  
                                                                    
                                                     
             
          if (SxactIsPrepared(nearConflict->sxactOut))
          {
            LWLockRelease(SerializableXactHashLock);
            ereport(ERROR, (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE), errmsg("could not serialize access due to read/write dependencies among transactions"), errdetail_internal("Reason code: Canceled on commit attempt with conflict in from prepared pivot."), errhint("The transaction might succeed if retried.")));
          }
          nearConflict->sxactOut->flags |= SXACT_FLAG_DOOMED;
          break;
        }
        farConflict = (RWConflict)SHMQueueNext(&nearConflict->sxactOut->inConflicts, &farConflict->inLink, offsetof(RWConflictData, inLink));
      }
    }

    nearConflict = (RWConflict)SHMQueueNext(&MySerializableXact->inConflicts, &nearConflict->inLink, offsetof(RWConflictData, inLink));
  }

  MySerializableXact->prepareSeqNo = ++(PredXact->LastSxactCommitSeqNo);
  MySerializableXact->flags |= SXACT_FLAG_PREPARED;

  LWLockRelease(SerializableXactHashLock);
}

                                                                            

   
                            
   

   
                   
                                                               
                                                    
   
void
AtPrepare_PredicateLocks(void)
{
  PREDICATELOCK *predlock;
  SERIALIZABLEXACT *sxact;
  TwoPhasePredicateRecord record;
  TwoPhasePredicateXactRecord *xactRecord;
  TwoPhasePredicateLockRecord *lockRecord;

  sxact = MySerializableXact;
  xactRecord = &(record.data.xactRecord);
  lockRecord = &(record.data.lockRecord);

  if (MySerializableXact == InvalidSerializableXact)
  {
    return;
  }

                                                        
  record.type = TWOPHASEPREDICATERECORD_XACT;
  xactRecord->xmin = MySerializableXact->xmin;
  xactRecord->flags = MySerializableXact->flags;

     
                                                                        
                                                                  
                                                                            
                       
     

  RegisterTwoPhaseRecord(TWOPHASE_RM_PREDICATELOCK_ID, 0, &record, sizeof(record));

     
                                           
     
                                                                             
                                                                         
                                
     
  LWLockAcquire(SerializablePredicateLockListLock, LW_SHARED);

     
                                                                           
                                                                           
                  
     
  Assert(!IsParallelWorker() && !ParallelContextActive());

  predlock = (PREDICATELOCK *)SHMQueueNext(&(sxact->predicateLocks), &(sxact->predicateLocks), offsetof(PREDICATELOCK, xactLink));

  while (predlock != NULL)
  {
    record.type = TWOPHASEPREDICATERECORD_LOCK;
    lockRecord->target = predlock->tag.myTarget->tag;

    RegisterTwoPhaseRecord(TWOPHASE_RM_PREDICATELOCK_ID, 0, &record, sizeof(record));

    predlock = (PREDICATELOCK *)SHMQueueNext(&(sxact->predicateLocks), &(predlock->xactLink), offsetof(PREDICATELOCK, xactLink));
  }

  LWLockRelease(SerializablePredicateLockListLock);
}

   
                     
                                                                
                                                              
                                                         
                                                      
   
void
PostPrepare_PredicateLocks(TransactionId xid)
{
  if (MySerializableXact == InvalidSerializableXact)
  {
    return;
  }

  Assert(SxactIsPrepared(MySerializableXact));

  MySerializableXact->pid = 0;

  hash_destroy(LocalPredicateLockHash);
  LocalPredicateLockHash = NULL;

  MySerializableXact = InvalidSerializableXact;
  MyXactDidWrite = false;
}

   
                               
                                                             
                       
   
void
PredicateLockTwoPhaseFinish(TransactionId xid, bool isCommit)
{
  SERIALIZABLEXID *sxid;
  SERIALIZABLEXIDTAG sxidtag;

  sxidtag.xid = xid;

  LWLockAcquire(SerializableXactHashLock, LW_SHARED);
  sxid = (SERIALIZABLEXID *)hash_search(SerializableXidHash, &sxidtag, HASH_FIND, NULL);
  LWLockRelease(SerializableXactHashLock);

                                                                     
  if (sxid == NULL)
  {
    return;
  }

                         
  MySerializableXact = sxid->myXact;
  MyXactDidWrite = true;                                        
                                        
  ReleasePredicateLocks(isCommit, false);
}

   
                                                                             
   
void
predicatelock_twophase_recover(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  TwoPhasePredicateRecord *record;

  Assert(len == sizeof(TwoPhasePredicateRecord));

  record = (TwoPhasePredicateRecord *)recdata;

  Assert((record->type == TWOPHASEPREDICATERECORD_XACT) || (record->type == TWOPHASEPREDICATERECORD_LOCK));

  if (record->type == TWOPHASEPREDICATERECORD_XACT)
  {
                                                            
    TwoPhasePredicateXactRecord *xactRecord;
    SERIALIZABLEXACT *sxact;
    SERIALIZABLEXID *sxid;
    SERIALIZABLEXIDTAG sxidtag;
    bool found;

    xactRecord = (TwoPhasePredicateXactRecord *)&record->data.xactRecord;

    LWLockAcquire(SerializableXactHashLock, LW_EXCLUSIVE);
    sxact = CreatePredXact();
    if (!sxact)
    {
      ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of shared memory")));
    }

                                                                  
    sxact->vxid.backendId = InvalidBackendId;
    sxact->vxid.localTransactionId = (LocalTransactionId)xid;
    sxact->pid = 0;

                                              
    sxact->prepareSeqNo = RecoverySerCommitSeqNo;
    sxact->commitSeqNo = InvalidSerCommitSeqNo;
    sxact->finishedBefore = InvalidTransactionId;

    sxact->SeqNo.lastCommitBeforeSnapshot = RecoverySerCommitSeqNo;

       
                                                                         
                                                                      
                                                                          
       
    SHMQueueInit(&(sxact->possibleUnsafeConflicts));

    SHMQueueInit(&(sxact->predicateLocks));
    SHMQueueElemInit(&(sxact->finishedLink));

    sxact->topXid = xid;
    sxact->xmin = xactRecord->xmin;
    sxact->flags = xactRecord->flags;
    Assert(SxactIsPrepared(sxact));
    if (!SxactIsReadOnly(sxact))
    {
      ++(PredXact->WritableSxactCount);
      Assert(PredXact->WritableSxactCount <= (MaxBackends + max_prepared_xacts));
    }

       
                                                                          
                                                                        
                                                                         
       
    SHMQueueInit(&(sxact->outConflicts));
    SHMQueueInit(&(sxact->inConflicts));
    sxact->flags |= SXACT_FLAG_SUMMARY_CONFLICT_IN;
    sxact->flags |= SXACT_FLAG_SUMMARY_CONFLICT_OUT;

                                        
    sxidtag.xid = xid;
    sxid = (SERIALIZABLEXID *)hash_search(SerializableXidHash, &sxidtag, HASH_ENTER, &found);
    Assert(sxid != NULL);
    Assert(!found);
    sxid->myXact = (SERIALIZABLEXACT *)sxact;

       
                                                                        
                                                                          
                                                                      
                                                                     
                                                            
       
    if ((!TransactionIdIsValid(PredXact->SxactGlobalXmin)) || (TransactionIdFollows(PredXact->SxactGlobalXmin, sxact->xmin)))
    {
      PredXact->SxactGlobalXmin = sxact->xmin;
      PredXact->SxactGlobalXminCount = 1;
      OldSerXidSetActiveSerXmin(sxact->xmin);
    }
    else if (TransactionIdEquals(sxact->xmin, PredXact->SxactGlobalXmin))
    {
      Assert(PredXact->SxactGlobalXminCount > 0);
      PredXact->SxactGlobalXminCount++;
    }

    LWLockRelease(SerializableXactHashLock);
  }
  else if (record->type == TWOPHASEPREDICATERECORD_LOCK)
  {
                                                 
    TwoPhasePredicateLockRecord *lockRecord;
    SERIALIZABLEXID *sxid;
    SERIALIZABLEXACT *sxact;
    SERIALIZABLEXIDTAG sxidtag;
    uint32 targettaghash;

    lockRecord = (TwoPhasePredicateLockRecord *)&record->data.lockRecord;
    targettaghash = PredicateLockTargetTagHashCode(&lockRecord->target);

    LWLockAcquire(SerializableXactHashLock, LW_SHARED);
    sxidtag.xid = xid;
    sxid = (SERIALIZABLEXID *)hash_search(SerializableXidHash, &sxidtag, HASH_FIND, NULL);
    LWLockRelease(SerializableXactHashLock);

    Assert(sxid != NULL);
    sxact = sxid->myXact;
    Assert(sxact != InvalidSerializableXact);

    CreatePredicateLock(&lockRecord->target, targettaghash, sxact);
  }
}

   
                                                                        
                                                                            
                    
   
SerializableXactHandle
ShareSerializableXact(void)
{
  return MySerializableXact;
}

   
                                                                   
   
void
AttachSerializableXact(SerializableXactHandle handle)
{

  Assert(MySerializableXact == InvalidSerializableXact);

  MySerializableXact = (SERIALIZABLEXACT *)handle;
  if (MySerializableXact != InvalidSerializableXact)
  {
    CreateLocalPredicateLockHash();
  }
}
