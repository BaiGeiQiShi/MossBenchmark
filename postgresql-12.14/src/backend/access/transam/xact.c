                                                                            
   
          
                                                   
   
                                                               
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <time.h>
#include <unistd.h>

#include "access/commit_ts.h"
#include "access/multixact.h"
#include "access/parallel.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/pg_enum.h"
#include "catalog/storage.h"
#include "commands/async.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "executor/spi.h"
#include "libpq/be-fsstubs.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/logical.h"
#include "replication/logicallauncher.h"
#include "replication/origin.h"
#include "replication/snapbuild.h"
#include "replication/syncrep.h"
#include "replication/walsender.h"
#include "storage/condition_variable.h"
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/md.h"
#include "storage/predicate.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/combocid.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/relmapper.h"
#include "utils/snapmgr.h"
#include "utils/timeout.h"
#include "utils/timestamp.h"
#include "pg_trace.h"

   
                             
   
int DefaultXactIsoLevel = XACT_READ_COMMITTED;
int XactIsoLevel;

bool DefaultXactReadOnly = false;
bool XactReadOnly;

bool DefaultXactDeferrable = false;
bool XactDeferrable;

int synchronous_commit = SYNCHRONOUS_COMMIT_ON;

   
                                                             
                                                                          
                                                                    
                                                                       
                                                                   
                                                                             
                                                                
   
                                                                              
                                                                            
                                                                      
                                                                              
                           
   
                                                                              
                                                                             
                                                                              
                                                                               
                                                                            
                                                                             
                                
   
FullTransactionId XactTopFullTransactionId = {InvalidTransactionId};
int nParallelCurrentXids = 0;
TransactionId *ParallelCurrentXids;

   
                                                                         
                                                                               
                                                                            
                                                                              
                    
   
int MyXactFlags;

   
                                                                  
   
typedef enum TransState
{
  TRANS_DEFAULT,              
  TRANS_START,                                
  TRANS_INPROGRESS,                                 
  TRANS_COMMIT,                             
  TRANS_ABORT,                             
  TRANS_PREPARE                              
} TransState;

   
                                                                  
   
                                                                 
                                                                    
   
typedef enum TBlockState
{
                                       
  TBLOCK_DEFAULT,           
  TBLOCK_STARTED,                                       

                                
  TBLOCK_BEGIN,                                               
  TBLOCK_INPROGRESS,                                
  TBLOCK_IMPLICIT_INPROGRESS,                                            
  TBLOCK_PARALLEL_INPROGRESS,                                              
  TBLOCK_END,                                      
  TBLOCK_ABORT,                                                   
  TBLOCK_ABORT_END,                                               
  TBLOCK_ABORT_PENDING,                                         
  TBLOCK_PREPARE,                                              

                             
  TBLOCK_SUBBEGIN,                                        
  TBLOCK_SUBINPROGRESS,                             
  TBLOCK_SUBRELEASE,                             
  TBLOCK_SUBCOMMIT,                                                        
  TBLOCK_SUBABORT,                                                
  TBLOCK_SUBABORT_END,                                            
  TBLOCK_SUBABORT_PENDING,                                      
  TBLOCK_SUBRESTART,                                               
  TBLOCK_SUBABORT_RESTART                                            
} TBlockState;

   
                               
   
typedef struct TransactionStateData
{
  FullTransactionId fullTransactionId;                           
  SubTransactionId subTransactionId;                      
  char *name;                                                      
  int savepointLevel;                                       
  TransState state;                                         
  TBlockState blockState;                                    
  int nestingLevel;                                                   
  int gucNestLevel;                                                   
  MemoryContext curTransactionContext;                               
  ResourceOwner curTransactionOwner;                           
  TransactionId *childXids;                                                       
  int nChildXids;                                                        
  int maxChildXids;                                                       
  Oid prevUser;                                                            
  int prevSecContext;                                                           
  bool prevXactReadOnly;                                              
  bool startedInRecovery;                                             
  bool didLogXid;                                                                
  int parallelModeLevel;                                                   
  bool chain;                                                                
  struct TransactionStateData *parent;                          
} TransactionStateData;

typedef TransactionStateData *TransactionState;

   
                                                                            
                                  
   
typedef struct SerializedTransactionState
{
  int xactIsoLevel;
  bool xactDeferrable;
  FullTransactionId topFullTransactionId;
  FullTransactionId currentFullTransactionId;
  CommandId currentCommandId;
  int nParallelCurrentXids;
  TransactionId parallelCurrentXids[FLEXIBLE_ARRAY_MEMBER];
} SerializedTransactionState;

                                                                            
#define SerializedTransactionStateHeaderSize offsetof(SerializedTransactionState, parallelCurrentXids)

   
                                                                          
                                                                  
                                                           
   
static TransactionStateData TopTransactionStateData = {
    .state = TRANS_DEFAULT,
    .blockState = TBLOCK_DEFAULT,
};

   
                                                                           
                                               
   
static int nUnreportedXids;
static TransactionId unreportedXids[PGPROC_MAX_CACHED_SUBXIDS];

static TransactionState CurrentTransactionState = &TopTransactionStateData;

   
                                                                       
                                                                      
   
static SubTransactionId currentSubTransactionId;
static CommandId currentCommandId;
static bool currentCommandIdUsed;

   
                                                               
                                                             
                                                                               
                                                                         
                                                
   
static TimestampTz xactStartTimestamp;
static TimestampTz stmtStartTimestamp;
static TimestampTz xactStopTimestamp;

   
                                                                       
                                                                          
   
static char *prepareGID;

   
                                                   
   
static bool forceSyncCommit = false;

                                                   
bool xact_is_sampled = false;

   
                                                                            
                                                                               
                                 
   
static MemoryContext TransactionAbortContext = NULL;

   
                                                   
   
typedef struct XactCallbackItem
{
  struct XactCallbackItem *next;
  XactCallback callback;
  void *arg;
} XactCallbackItem;

static XactCallbackItem *Xact_callbacks = NULL;

   
                                                      
   
typedef struct SubXactCallbackItem
{
  struct SubXactCallbackItem *next;
  SubXactCallback callback;
  void *arg;
} SubXactCallbackItem;

static SubXactCallbackItem *SubXact_callbacks = NULL;

                               
static void
AssignTransactionId(TransactionState s);
static void
AbortTransaction(void);
static void
AtAbort_Memory(void);
static void
AtCleanup_Memory(void);
static void
AtAbort_ResourceOwner(void);
static void
AtCCI_LocalCache(void);
static void
AtCommit_Memory(void);
static void
AtStart_Cache(void);
static void
AtStart_Memory(void);
static void
AtStart_ResourceOwner(void);
static void
CallXactCallbacks(XactEvent event);
static void
CallSubXactCallbacks(SubXactEvent event, SubTransactionId mySubid, SubTransactionId parentSubid);
static void
CleanupTransaction(void);
static void
CheckTransactionBlock(bool isTopLevel, bool throwError, const char *stmtType);
static void
CommitTransaction(void);
static TransactionId
RecordTransactionAbort(bool isSubXact);
static void
StartTransaction(void);

static void
StartSubTransaction(void);
static void
CommitSubTransaction(void);
static void
AbortSubTransaction(void);
static void
CleanupSubTransaction(void);
static void
PushTransaction(void);
static void
PopTransaction(void);

static void
AtSubAbort_Memory(void);
static void
AtSubCleanup_Memory(void);
static void
AtSubAbort_ResourceOwner(void);
static void
AtSubCommit_Memory(void);
static void
AtSubStart_Memory(void);
static void
AtSubStart_ResourceOwner(void);

static void
ShowTransactionState(const char *str);
static void
ShowTransactionStateRec(const char *str, TransactionState state);
static const char *
BlockStateAsString(TBlockState blockState);
static const char *
TransStateAsString(TransState state);

                                                                    
                               
                                                                    
   

   
                      
   
                                                                    
                                                                        
   
bool
IsTransactionState(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                             
                                                                        
                                                                       
                                                                            
                                
     
  return (s->state == TRANS_INPROGRESS);
}

   
                                  
   
                                                                    
   
bool
IsAbortedTransactionBlockState(void)
{
  TransactionState s = CurrentTransactionState;

  if (s->blockState == TBLOCK_ABORT || s->blockState == TBLOCK_SUBABORT)
  {
    return true;
  }

  return false;
}

   
                       
   
                                                                      
                                                                        
   
TransactionId
GetTopTransactionId(void)
{
  if (!FullTransactionIdIsValid(XactTopFullTransactionId))
  {
    AssignTransactionId(&TopTransactionStateData);
  }
  return XidFromFullTransactionId(XactTopFullTransactionId);
}

   
                            
   
                                                                         
                                                                        
                                                                              
   
TransactionId
GetTopTransactionIdIfAny(void)
{
  return XidFromFullTransactionId(XactTopFullTransactionId);
}

   
                           
   
                                                                    
                                                                             
                             
   
TransactionId
GetCurrentTransactionId(void)
{
  TransactionState s = CurrentTransactionState;

  if (!FullTransactionIdIsValid(s->fullTransactionId))
  {
    AssignTransactionId(s);
  }
  return XidFromFullTransactionId(s->fullTransactionId);
}

   
                                
   
                                                                         
                                                                        
                                                                              
   
TransactionId
GetCurrentTransactionIdIfAny(void)
{
  return XidFromFullTransactionId(CurrentTransactionState->fullTransactionId);
}

   
                           
   
                                                                             
                                                                               
   
FullTransactionId
GetTopFullTransactionId(void)
{
  if (!FullTransactionIdIsValid(XactTopFullTransactionId))
  {
    AssignTransactionId(&TopTransactionStateData);
  }
  return XactTopFullTransactionId;
}

   
                                
   
                                                                             
                                                                              
                                                                               
        
   
FullTransactionId
GetTopFullTransactionIdIfAny(void)
{
  return XactTopFullTransactionId;
}

   
                               
   
                                                                              
                                                                            
                                  
   
FullTransactionId
GetCurrentFullTransactionId(void)
{
  TransactionState s = CurrentTransactionState;

  if (!FullTransactionIdIsValid(s->fullTransactionId))
  {
    AssignTransactionId(s);
  }
  return s->fullTransactionId;
}

   
                                    
   
                                                                             
                                                                              
                                                                               
        
   
FullTransactionId
GetCurrentFullTransactionIdIfAny(void)
{
  return CurrentTransactionState->fullTransactionId;
}

   
                                       
   
                                                                                
   
void
MarkCurrentTransactionIdLoggedIfAny(void)
{
  if (FullTransactionIdIsValid(CurrentTransactionState->fullTransactionId))
  {
    CurrentTransactionState->didLogXid = true;
  }
}

   
                                
   
                                                                              
                                                                               
                                                                              
                                                                               
   
TransactionId
GetStableLatestTransactionId(void)
{
  static LocalTransactionId lxid = InvalidLocalTransactionId;
  static TransactionId stablexid = InvalidTransactionId;

  if (lxid != MyProc->lxid)
  {
    lxid = MyProc->lxid;
    stablexid = GetTopTransactionIdIfAny();
    if (!TransactionIdIsValid(stablexid))
    {
      stablexid = ReadNewTransactionId();
    }
  }

  Assert(TransactionIdIsValid(stablexid));

  return stablexid;
}

   
                       
   
                                                                            
                                                                      
                                                                            
                                                                         
                           
   
static void
AssignTransactionId(TransactionState s)
{
  bool isSubXact = (s->parent != NULL);
  ResourceOwner currentOwner;
  bool log_unknown_top = false;

                                          
  Assert(!FullTransactionIdIsValid(s->fullTransactionId));
  Assert(s->state == TRANS_INPROGRESS);

     
                                                                             
                                                                
     
  if (IsInParallelMode() || IsParallelWorker())
  {
    elog(ERROR, "cannot assign XIDs during a parallel operation");
  }

     
                                                                         
                                                                     
                                                                             
                             
     
  if (isSubXact && !FullTransactionIdIsValid(s->parent->fullTransactionId))
  {
    TransactionState p = s->parent;
    TransactionState *parents;
    size_t parentOffset = 0;

    parents = palloc(sizeof(TransactionState) * s->nestingLevel);
    while (p != NULL && !FullTransactionIdIsValid(p->fullTransactionId))
    {
      parents[parentOffset++] = p;
      p = p->parent;
    }

       
                                                                          
                                    
       
    while (parentOffset != 0)
    {
      AssignTransactionId(parents[--parentOffset]);
    }

    pfree(parents);
  }

     
                                                                            
                                                                           
                                                                   
                                                                            
                                                                             
                                                                          
                                                                           
                       
     
  if (isSubXact && XLogLogicalInfoActive() && !TopTransactionStateData.didLogXid)
  {
    log_unknown_top = true;
  }

     
                                                                        
                  
     
                                                                            
                                                                             
                                                                             
                                                     
     
  s->fullTransactionId = GetNewTransactionId(isSubXact);
  if (!isSubXact)
  {
    XactTopFullTransactionId = s->fullTransactionId;
  }

  if (isSubXact)
  {
    SubTransSetParent(XidFromFullTransactionId(s->fullTransactionId), XidFromFullTransactionId(s->parent->fullTransactionId));
  }

     
                                                                            
                           
     
  if (!isSubXact)
  {
    RegisterPredicateLockingXid(XidFromFullTransactionId(s->fullTransactionId));
  }

     
                                                                             
                                                                       
                    
     
  currentOwner = CurrentResourceOwner;
  CurrentResourceOwner = s->curTransactionOwner;

  XactLockTableInsert(XidFromFullTransactionId(s->fullTransactionId));

  CurrentResourceOwner = currentOwner;

     
                                                                          
                                                                        
                                                                          
                                                  
     
                                                                             
                                                                     
                                          
     
                                                                          
                                                                            
                                                                      
             
     
                                                                            
                                                                
     
  if (isSubXact && XLogStandbyInfoActive())
  {
    unreportedXids[nUnreportedXids] = XidFromFullTransactionId(s->fullTransactionId);
    nUnreportedXids++;

       
                                               
                                     
       
    if (nUnreportedXids >= PGPROC_MAX_CACHED_SUBXIDS || log_unknown_top)
    {
      xl_xact_assignment xlrec;

         
                                                                     
                                                                     
         
      xlrec.xtop = GetTopTransactionId();
      Assert(TransactionIdIsValid(xlrec.xtop));
      xlrec.nsubxacts = nUnreportedXids;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, MinSizeOfXactAssignment);
      XLogRegisterData((char *)unreportedXids, nUnreportedXids * sizeof(TransactionId));

      (void)XLogInsert(RM_XACT_ID, XLOG_XACT_ASSIGNMENT);

      nUnreportedXids = 0;
                                                            
      TopTransactionStateData.didLogXid = true;
    }
  }
}

   
                              
   
SubTransactionId
GetCurrentSubTransactionId(void)
{
  TransactionState s = CurrentTransactionState;

  return s->subTransactionId;
}

   
                          
   
                                                                     
                                                                             
   
bool
SubTransactionIsActive(SubTransactionId subxid)
{
  TransactionState s;

  for (s = CurrentTransactionState; s != NULL; s = s->parent)
  {
    if (s->state == TRANS_ABORT)
    {
      continue;
    }
    if (s->subTransactionId == subxid)
    {
      return true;
    }
  }
  return false;
}

   
                       
   
                                                                           
                                                                         
                                                                    
                                             
   
CommandId
GetCurrentCommandId(bool used)
{
                                                                 
  if (used)
  {
       
                                                                         
                                                                           
                                                                          
                                                    
       
    Assert(!IsParallelWorker());
    currentCommandIdUsed = true;
  }
  return currentCommandId;
}

   
                              
   
                                                                    
                                                                
                                                                
                                                                      
   
void
SetParallelStartTimestamps(TimestampTz xact_ts, TimestampTz stmt_ts)
{
  Assert(IsParallelWorker());
  xactStartTimestamp = xact_ts;
  stmtStartTimestamp = stmt_ts;
}

   
                                       
   
TimestampTz
GetCurrentTransactionStartTimestamp(void)
{
  return xactStartTimestamp;
}

   
                                     
   
TimestampTz
GetCurrentStatementStartTimestamp(void)
{
  return stmtStartTimestamp;
}

   
                                      
   
                                                                       
                                                                        
   
TimestampTz
GetCurrentTransactionStopTimestamp(void)
{
  if (xactStopTimestamp != 0)
  {
    return xactStopTimestamp;
  }
  return GetCurrentTimestamp();
}

   
                                     
   
                                                                          
                                    
   
void
SetCurrentStatementStartTimestamp(void)
{
  if (!IsParallelWorker())
  {
    stmtStartTimestamp = GetCurrentTimestamp();
  }
  else
  {
    Assert(stmtStartTimestamp != 0);
  }
}

   
                                      
   
static inline void
SetCurrentTransactionStopTimestamp(void)
{
  xactStopTimestamp = GetCurrentTimestamp();
}

   
                                  
   
                                                                         
                                        
   
int
GetCurrentTransactionNestLevel(void)
{
  TransactionState s = CurrentTransactionState;

  return s->nestingLevel;
}

   
                                       
   
bool
TransactionIdIsCurrentTransactionId(TransactionId xid)
{
  TransactionState s;

     
                                                                          
                                                                       
                                                                          
                                                                       
                                                                         
                                                                          
                                        
     
                                                                          
                                                                          
                         
     
  if (!TransactionIdIsNormal(xid))
  {
    return false;
  }

     
                                                                             
                                                                             
                                                                            
                                  
     
  if (nParallelCurrentXids > 0)
  {
    int low, high;

    low = 0;
    high = nParallelCurrentXids - 1;
    while (low <= high)
    {
      int middle;
      TransactionId probe;

      middle = low + (high - low) / 2;
      probe = ParallelCurrentXids[middle];
      if (probe == xid)
      {
        return true;
      }
      else if (probe < xid)
      {
        low = middle + 1;
      }
      else
      {
        high = middle - 1;
      }
    }
    return false;
  }

     
                                                                           
                                                                    
                                                                             
                                                                           
                  
     
  for (s = CurrentTransactionState; s != NULL; s = s->parent)
  {
    int low, high;

    if (s->state == TRANS_ABORT)
    {
      continue;
    }
    if (!FullTransactionIdIsValid(s->fullTransactionId))
    {
      continue;                                          
    }
    if (TransactionIdEquals(xid, XidFromFullTransactionId(s->fullTransactionId)))
    {
      return true;
    }
                                                                     
    low = 0;
    high = s->nChildXids - 1;
    while (low <= high)
    {
      int middle;
      TransactionId probe;

      middle = low + (high - low) / 2;
      probe = s->childXids[middle];
      if (TransactionIdEquals(probe, xid))
      {
        return true;
      }
      else if (TransactionIdPrecedes(probe, xid))
      {
        low = middle + 1;
      }
      else
      {
        high = middle - 1;
      }
    }
  }

  return false;
}

   
                                    
   
                                                                            
                                                                              
                         
   
bool
TransactionStartedDuringRecovery(void)
{
  return CurrentTransactionState->startedInRecovery;
}

   
                     
   
void
EnterParallelMode(void)
{
  TransactionState s = CurrentTransactionState;

  Assert(s->parallelModeLevel >= 0);

  ++s->parallelModeLevel;
}

   
                    
   
void
ExitParallelMode(void)
{
  TransactionState s = CurrentTransactionState;

  Assert(s->parallelModeLevel > 0);
  Assert(s->parallelModeLevel > 1 || !ParallelContextActive());

  --s->parallelModeLevel;
}

   
                    
   
                                                                            
                                                                           
                                                                       
                                                                          
                                         
   
bool
IsInParallelMode(void)
{
  return CurrentTransactionState->parallelModeLevel != 0;
}

   
                           
   
void
CommandCounterIncrement(void)
{
     
                                                                            
                                                                            
                                                                           
                                                                         
     
  if (currentCommandIdUsed)
  {
       
                                                                      
                                                                           
              
       
    if (IsInParallelMode() || IsParallelWorker())
    {
      elog(ERROR, "cannot start commands during a parallel operation");
    }

    currentCommandId += 1;
    if (currentCommandId == InvalidCommandId)
    {
      currentCommandId -= 1;
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("cannot have more than 2^32-2 commands in a transaction")));
    }
    currentCommandIdUsed = false;

                                                        
    SnapshotSetCommandId(currentCommandId);

       
                                                                           
                                                                          
                                                                          
                                                                        
       
    AtCCI_LocalCache();
  }
}

   
                   
   
                                                                            
                                 
   
void
ForceSyncCommit(void)
{
  forceSyncCommit = true;
}

                                                                    
                               
                                                                    
   

   
                 
   
static void
AtStart_Cache(void)
{
  AcceptInvalidationMessages();
}

   
                  
   
static void
AtStart_Memory(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                     
                                                                       
                                                                   
                                                                            
                                                       
     
  if (TransactionAbortContext == NULL)
  {
    TransactionAbortContext = AllocSetContextCreate(TopMemoryContext, "TransactionAbortContext", 32 * 1024, 32 * 1024, 32 * 1024);
  }

     
                                                      
     
  Assert(TopTransactionContext == NULL);

     
                                                    
     
  TopTransactionContext = AllocSetContextCreate(TopMemoryContext, "TopTransactionContext", ALLOCSET_DEFAULT_SIZES);

     
                                                                      
                            
     
  CurTransactionContext = TopTransactionContext;
  s->curTransactionContext = CurTransactionContext;

                                              
  MemoryContextSwitchTo(CurTransactionContext);
}

   
                         
   
static void
AtStart_ResourceOwner(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                             
     
  Assert(TopTransactionResourceOwner == NULL);

     
                                                           
     
  s->curTransactionOwner = ResourceOwnerCreate(NULL, "TopTransaction");

  TopTransactionResourceOwner = s->curTransactionOwner;
  CurTransactionResourceOwner = s->curTransactionOwner;
  CurrentResourceOwner = s->curTransactionOwner;
}

                                                                    
                                  
                                                                    
   

   
                     
   
static void
AtSubStart_Memory(void)
{
  TransactionState s = CurrentTransactionState;

  Assert(CurTransactionContext != NULL);

     
                                                                          
                                                                            
                                                                         
     
  CurTransactionContext = AllocSetContextCreate(CurTransactionContext, "CurTransactionContext", ALLOCSET_DEFAULT_SIZES);
  s->curTransactionContext = CurTransactionContext;

                                              
  MemoryContextSwitchTo(CurTransactionContext);
}

   
                            
   
static void
AtSubStart_ResourceOwner(void)
{
  TransactionState s = CurrentTransactionState;

  Assert(s->parent != NULL);

     
                                                                            
                                            
     
  s->curTransactionOwner = ResourceOwnerCreate(s->parent->curTransactionOwner, "SubTransaction");

  CurTransactionResourceOwner = s->curTransactionOwner;
  CurrentResourceOwner = s->curTransactionOwner;
}

                                                                    
                                
                                                                    
   

   
                           
   
                                                                           
                                                                             
   
                                                                          
   
static TransactionId
RecordTransactionCommit(void)
{
  TransactionId xid = GetTopTransactionIdIfAny();
  bool markXidCommitted = TransactionIdIsValid(xid);
  TransactionId latestXid = InvalidTransactionId;
  int nrels;
  RelFileNode *rels;
  int nchildren;
  TransactionId *children;
  int nmsgs = 0;
  SharedInvalidationMessage *invalMessages = NULL;
  bool RelcacheInitFileInval = false;
  bool wrote_xlog;

                                         
  nrels = smgrGetPendingDeletes(true, &rels);
  nchildren = xactGetCommittedChildren(&children);
  if (XLogStandbyInfoActive())
  {
    nmsgs = xactGetCommittedInvalidationMessages(&invalMessages, &RelcacheInitFileInval);
  }
  wrote_xlog = (XactLastRecEnd != 0);

     
                                                                            
                               
     
  if (!markXidCommitted)
  {
       
                                                                        
                                                                           
                                                                           
                                 
       
    if (nrels != 0)
    {
      elog(ERROR, "cannot commit a transaction that deleted files but has no xid");
    }

                                                                         
    Assert(nchildren == 0);

       
                                                                     
                                                                  
                                                                           
                                                                          
                                                                      
                                                                         
                                                                          
                                                                           
                     
       
    if (nmsgs != 0)
    {
      LogStandbyInvalidations(nmsgs, invalMessages, RelcacheInitFileInval);
      wrote_xlog = true;                             
    }

       
                                                                       
                                                                         
                                                                           
                                                     
       
    if (!wrote_xlog)
    {
      goto cleanup;
    }
  }
  else
  {
    bool replorigin;

       
                                                                          
                                        
       
    replorigin = (replorigin_session_origin != InvalidRepOriginId && replorigin_session_origin != DoNotReplicateId);

       
                                                                        
       
                                                    
    BufmgrCommit();

       
                                                                     
                                                                    
                                                                        
                                                                          
                                                                     
                               
       
                                                             
                                                                           
                                                                         
       
                                                                          
                                                                         
                                                                           
                                           
       
    Assert(!MyPgXact->delayChkpt);
    START_CRIT_SECTION();
    MyPgXact->delayChkpt = true;

    SetCurrentTransactionStopTimestamp();

    XactLogCommitRecord(xactStopTimestamp, nchildren, children, nrels, rels, nmsgs, invalMessages, RelcacheInitFileInval, forceSyncCommit, MyXactFlags, InvalidTransactionId, NULL                   );

    if (replorigin)
    {
                                                         
      replorigin_session_advance(replorigin_session_origin_lsn, XactLastRecEnd);
    }

       
                                                                   
                                                                  
                                                                           
                    
       
                                                                    
                                                
       

    if (!replorigin || replorigin_session_origin_timestamp == 0)
    {
      replorigin_session_origin_timestamp = xactStopTimestamp;
    }

    TransactionTreeSetCommitTsData(xid, nchildren, children, replorigin_session_origin_timestamp, replorigin_session_origin, false);
  }

     
                                                                             
                                                                           
                                                                             
                                                                             
                                                                           
                                                                         
                                                                         
                                                                            
                                                                           
                                                                          
                                                                           
                                                            
                                                                             
                                                                          
             
     
                                                                            
                                                                       
                                                                           
                                                                             
                                                                            
                                                                           
                          
     
  if ((wrote_xlog && markXidCommitted && synchronous_commit > SYNCHRONOUS_COMMIT_OFF) || forceSyncCommit || nrels > 0)
  {
    XLogFlush(XactLastRecEnd);

       
                                                                     
       
    if (markXidCommitted)
    {
      TransactionIdCommitTree(xid, nchildren, children);
    }
  }
  else
  {
       
                                 
       
                                                                         
                                                                           
                                                             
                                                           
       
                                                                           
                          
       
    XLogSetAsyncXactLSN(XactLastRecEnd);

       
                                                                          
                                                                    
                                               
       
    if (markXidCommitted)
    {
      TransactionIdAsyncCommitTree(xid, nchildren, children, XactLastRecEnd);
    }
  }

     
                                                                    
                          
     
  if (markXidCommitted)
  {
    MyPgXact->delayChkpt = false;
    END_CRIT_SECTION();
  }

                                                            
  latestXid = TransactionIdLatest(xid, nchildren, children);

     
                                                                            
                                                                         
                                                                            
                                                                          
     
                                                                            
                                                  
     
  if (wrote_xlog && markXidCommitted)
  {
    SyncRepWaitForLSN(XactLastRecEnd, true);
  }

                                          
  XactLastCommitEnd = XactLastRecEnd;

                                                                        
  XactLastRecEnd = 0;
cleanup:
                           
  if (rels)
  {
    pfree(rels);
  }

  return latestXid;
}

   
                    
   
static void
AtCCI_LocalCache(void)
{
     
                                                                            
                                                                        
                                                                     
     
  AtCCI_RelationMap();

     
                                                              
     
  CommandEndInvalidationMessages();
}

   
                   
   
static void
AtCommit_Memory(void)
{
     
                                                                            
                                                                    
     
  MemoryContextSwitchTo(TopMemoryContext);

     
                                           
     
  Assert(TopTransactionContext != NULL);
  MemoryContextDelete(TopTransactionContext);
  TopTransactionContext = NULL;
  CurTransactionContext = NULL;
  CurrentTransactionState->curTransactionContext = NULL;
}

                                                                    
                                   
                                                                    
   

   
                      
   
static void
AtSubCommit_Memory(void)
{
  TransactionState s = CurrentTransactionState;

  Assert(s->parent != NULL);

                                                            
  CurTransactionContext = s->parent->curTransactionContext;
  MemoryContextSwitchTo(CurTransactionContext);

     
                                                                        
                                                                             
                                                                             
                                                                   
     
  if (MemoryContextIsEmpty(s->curTransactionContext))
  {
    MemoryContextDelete(s->curTransactionContext);
    s->curTransactionContext = NULL;
  }
}

   
                         
   
                                                                            
   
static void
AtSubCommit_childXids(void)
{
  TransactionState s = CurrentTransactionState;
  int new_nChildXids;

  Assert(s->parent != NULL);

     
                                                                    
                                                       
     
  new_nChildXids = s->parent->nChildXids + s->nChildXids + 1;

                                                         
  if (s->parent->maxChildXids < new_nChildXids)
  {
    int new_maxChildXids;
    TransactionId *new_childXids;

       
                                                                         
                                                                          
                                                                          
                                                      
       
    new_maxChildXids = Min(new_nChildXids * 2, (int)(MaxAllocSize / sizeof(TransactionId)));

    if (new_maxChildXids < new_nChildXids)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("maximum number of committed subtransactions (%d) exceeded", (int)(MaxAllocSize / sizeof(TransactionId)))));
    }

       
                                                                          
                                                                          
                                 
       
    if (s->parent->childXids == NULL)
    {
      new_childXids = MemoryContextAlloc(TopTransactionContext, new_maxChildXids * sizeof(TransactionId));
    }
    else
    {
      new_childXids = repalloc(s->parent->childXids, new_maxChildXids * sizeof(TransactionId));
    }

    s->parent->childXids = new_childXids;
    s->parent->maxChildXids = new_maxChildXids;
  }

     
                                         
     
                                                                           
                                                                          
                                                                             
                                                                         
                                                              
     
  s->parent->childXids[s->parent->nChildXids] = XidFromFullTransactionId(s->fullTransactionId);

  if (s->nChildXids > 0)
  {
    memcpy(&s->parent->childXids[s->parent->nChildXids + 1], s->childXids, s->nChildXids * sizeof(TransactionId));
  }

  s->parent->nChildXids = new_nChildXids;

                                              
  if (s->childXids != NULL)
  {
    pfree(s->childXids);
  }
                                                                        
  s->childXids = NULL;
  s->nChildXids = 0;
  s->maxChildXids = 0;
}

                                                                    
                               
                                                                    
   

   
                          
   
                                                                           
                                                                             
   
static TransactionId
RecordTransactionAbort(bool isSubXact)
{
  TransactionId xid = GetCurrentTransactionIdIfAny();
  TransactionId latestXid;
  int nrels;
  RelFileNode *rels;
  int nchildren;
  TransactionId *children;
  TimestampTz xact_time;

     
                                                                             
                                                                             
                                                                            
                                                            
     
  if (!TransactionIdIsValid(xid))
  {
                                                                          
    if (!isSubXact)
    {
      XactLastRecEnd = 0;
    }
    return InvalidTransactionId;
  }

     
                                                                     
     
                                                                             
                                                                            
                                                                
     

     
                                                                            
     
  if (TransactionIdDidCommit(xid))
  {
    elog(PANIC, "cannot abort transaction %u, it was already committed", xid);
  }

                                                   
  nrels = smgrGetPendingDeletes(false, &rels);
  nchildren = xactGetCommittedChildren(&children);

                                                      
  START_CRIT_SECTION();

                              
  if (isSubXact)
  {
    xact_time = GetCurrentTimestamp();
  }
  else
  {
    SetCurrentTransactionStopTimestamp();
    xact_time = xactStopTimestamp;
  }

  XactLogAbortRecord(xact_time, nchildren, children, nrels, rels, MyXactFlags, InvalidTransactionId, NULL);

     
                                                                        
                                                                            
                                                                       
                                                                           
                                                                          
                                                                      
                                   
     
  if (!isSubXact)
  {
    XLogSetAsyncXactLSN(XactLastRecEnd);
  }

     
                                                                             
                                                                           
                                                                      
                                                                             
                                                                          
                                             
     
  TransactionIdAbortTree(xid, nchildren, children);

  END_CRIT_SECTION();

                                                            
  latestXid = TransactionIdLatest(xid, nchildren, children);

     
                                                                          
                                                                          
                                                                         
                                                                          
     
  if (isSubXact)
  {
    XidCacheRemoveRunningXids(xid, nchildren, children, latestXid);
  }

                                                                        
  if (!isSubXact)
  {
    XactLastRecEnd = 0;
  }

                               
  if (rels)
  {
    pfree(rels);
  }

  return latestXid;
}

   
                  
   
static void
AtAbort_Memory(void)
{
     
                                                                            
                                                                        
                           
     
                                                                          
                                                              
     
  if (TransactionAbortContext != NULL)
  {
    MemoryContextSwitchTo(TransactionAbortContext);
  }
  else
  {
    MemoryContextSwitchTo(TopMemoryContext);
  }
}

   
                     
   
static void
AtSubAbort_Memory(void)
{
  Assert(TransactionAbortContext != NULL);

  MemoryContextSwitchTo(TransactionAbortContext);
}

   
                         
   
static void
AtAbort_ResourceOwner(void)
{
     
                                                                           
                        
     
  CurrentResourceOwner = TopTransactionResourceOwner;
}

   
                            
   
static void
AtSubAbort_ResourceOwner(void)
{
  TransactionState s = CurrentTransactionState;

                                               
  CurrentResourceOwner = s->curTransactionOwner;
}

   
                        
   
static void
AtSubAbort_childXids(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                
                                                                    
                                           
     
  if (s->childXids != NULL)
  {
    pfree(s->childXids);
  }
  s->childXids = NULL;
  s->nChildXids = 0;
  s->maxChildXids = 0;

     
                                                                             
                                                                            
                                                                            
                            
     
}

                                                                    
                                 
                                                                    
   

   
                    
   
static void
AtCleanup_Memory(void)
{
  Assert(CurrentTransactionState->parent == NULL);

     
                                                                            
                                                                    
     
  MemoryContextSwitchTo(TopMemoryContext);

     
                                                    
     
  if (TransactionAbortContext != NULL)
  {
    MemoryContextResetAndDeleteChildren(TransactionAbortContext);
  }

     
                                           
     
  if (TopTransactionContext != NULL)
  {
    MemoryContextDelete(TopTransactionContext);
  }
  TopTransactionContext = NULL;
  CurTransactionContext = NULL;
  CurrentTransactionState->curTransactionContext = NULL;
}

                                                                    
                                    
                                                                    
   

   
                       
   
static void
AtSubCleanup_Memory(void)
{
  TransactionState s = CurrentTransactionState;

  Assert(s->parent != NULL);

                                                             
  MemoryContextSwitchTo(s->parent->curTransactionContext);
  CurTransactionContext = s->parent->curTransactionContext;

     
                                                    
     
  if (TransactionAbortContext != NULL)
  {
    MemoryContextResetAndDeleteChildren(TransactionAbortContext);
  }

     
                                                                             
                                                                           
                      
     
  if (s->curTransactionContext)
  {
    MemoryContextDelete(s->curTransactionContext);
  }
  s->curTransactionContext = NULL;
}

                                                                    
                           
                                                                    
   

   
                    
   
static void
StartTransaction(void)
{
  TransactionState s;
  VirtualTransactionId vxid;

     
                                                   
     
  s = &TopTransactionStateData;
  CurrentTransactionState = s;

  Assert(!FullTransactionIdIsValid(XactTopFullTransactionId));

                                           
  Assert(s->state == TRANS_DEFAULT);

     
                                                                        
                                                                          
                                                                         
                              
     
  s->state = TRANS_START;
  s->fullTransactionId = InvalidFullTransactionId;                     

                                                              
  xact_is_sampled = log_xact_sample_rate != 0 && (log_xact_sample_rate == 1 || random() <= log_xact_sample_rate * MAX_RANDOM_VALUE);

     
                                                 
     
                                                               
     
  s->nestingLevel = 1;
  s->gucNestLevel = 1;
  s->childXids = NULL;
  s->nChildXids = 0;
  s->maxChildXids = 0;

     
                                                                          
                                                                    
     
  GetUserIdAndSecContext(&s->prevUser, &s->prevSecContext);

                                                                            
  Assert(s->prevSecContext == 0);

     
                                                
     
                                                                           
                                                                         
                                                                    
                                                               
     
  if (RecoveryInProgress())
  {
    s->startedInRecovery = true;
    XactReadOnly = true;
  }
  else
  {
    s->startedInRecovery = false;
    XactReadOnly = DefaultXactReadOnly;
  }
  XactDeferrable = DefaultXactDeferrable;
  XactIsoLevel = DefaultXactIsoLevel;
  forceSyncCommit = false;
  MyXactFlags = 0;

     
                                              
     
  s->subTransactionId = TopSubTransactionId;
  currentSubTransactionId = TopSubTransactionId;
  currentCommandId = FirstCommandId;
  currentCommandIdUsed = false;

     
                                        
     
  nUnreportedXids = 0;
  s->didLogXid = false;

     
                                                     
     
  AtStart_Memory();
  AtStart_ResourceOwner();

     
                                                                           
                                    
     
  vxid.backendId = MyBackendId;
  vxid.localTransactionId = GetNextLocalTransactionId();

     
                                                                             
     
  VirtualXactLockTableInsert(vxid);

     
                                                              
                                                                            
     
  Assert(MyProc->backendId == vxid.backendId);
  MyProc->lxid = vxid.localTransactionId;

  TRACE_POSTGRESQL_TRANSACTION_START(vxid.localTransactionId);

     
                                                                           
                                                                             
                                                                          
                                                                     
                                                                          
                                                                          
                                   
     
  if (!IsParallelWorker())
  {
    if (!SPI_inside_nonatomic_context())
    {
      xactStartTimestamp = stmtStartTimestamp;
    }
    else
    {
      xactStartTimestamp = GetCurrentTimestamp();
    }
  }
  else
  {
    Assert(xactStartTimestamp != 0);
  }
  pgstat_report_xact_timestamp(xactStartTimestamp);
                                        
  xactStopTimestamp = 0;

     
                                                     
     
  AtStart_GUC();
  AtStart_Cache();
  AfterTriggerBeginXact();

     
                                                                      
               
     
  s->state = TRANS_INPROGRESS;

  ShowTransactionState("StartTransaction");
}

   
                     
   
                                                                          
   
static void
CommitTransaction(void)
{
  TransactionState s = CurrentTransactionState;
  TransactionId latestXid;
  bool is_parallel_worker;

  is_parallel_worker = (s->blockState == TBLOCK_PARALLEL_INPROGRESS);

                                                                         
  if (is_parallel_worker)
  {
    EnterParallelMode();
  }

  ShowTransactionState("CommitTransaction");

     
                                         
     
  if (s->state != TRANS_INPROGRESS)
  {
    elog(WARNING, "CommitTransaction while in %s state", TransStateAsString(s->state));
  }
  Assert(s->parent == NULL);

     
                                                                            
                                                                            
                                                                        
                                                                            
                                                                             
     
  for (;;)
  {
       
                                                     
       
    AfterTriggerFireDeferred();

       
                                                                          
                                                                          
                                                                 
       
    if (!PreCommit_Portals(false))
    {
      break;
    }
  }

     
                                                                           
                                                                             
                                                                          
                                 
     

  CallXactCallbacks(is_parallel_worker ? XACT_EVENT_PARALLEL_PRE_COMMIT : XACT_EVENT_PRE_COMMIT);

                                                             
  if (IsInParallelMode())
  {
    AtEOXact_Parallel(true);
  }

                                              
  AfterTriggerEndXact(true);

     
                                                                      
                                                    
     
  PreCommit_on_commit_actions();

                                                      
  AtEOXact_LargeObject(true);

     
                                                                        
                                                                      
                                                                       
                                                                        
     
  PreCommit_Notify();

     
                                                                     
                                                                             
                                                                            
                                                                            
                                                                       
     
  if (!is_parallel_worker)
  {
    PreCommit_CheckForSerializationFailure();
  }

                                                      
  HOLD_INTERRUPTS();

                                                                          
  AtEOXact_RelationMap(true, is_parallel_worker);

     
                                                                        
                       
     
  s->state = TRANS_COMMIT;
  s->parallelModeLevel = 0;

  if (!is_parallel_worker)
  {
       
                                                                           
                       
       
    latestXid = RecordTransactionCommit();
  }
  else
  {
       
                                                                  
                             
       
    latestXid = InvalidTransactionId;

       
                                                                       
                
       
    ParallelWorkerReportLastRecEnd(XactLastRecEnd);
  }

  TRACE_POSTGRESQL_TRANSACTION_COMMIT(MyProc->lxid);

     
                                                                            
                                                               
                              
     
  ProcArrayEndTransaction(MyProc, latestXid);

     
                                                                             
                                                                  
                                     
     
                                                                      
                                                                           
                                                                          
                                                                          
                                                
     
                                                                             
                                                                             
            
     

  CallXactCallbacks(is_parallel_worker ? XACT_EVENT_PARALLEL_COMMIT : XACT_EVENT_COMMIT);

  ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_BEFORE_LOCKS, true, true);

                                            
  AtEOXact_Buffers(true);

                                   
  AtEOXact_RelationCache(true);

     
                                                                             
                                                       
                                                                          
                                                                         
                                                                     
     
  AtEOXact_Inval(true);

  AtEOXact_MultiXact();

  ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_LOCKS, true, true);
  ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_AFTER_LOCKS, true, true);

     
                                                                             
                                                                      
                                                                        
                                                                             
                                                                         
                                                                       
                                       
     
  smgrDoPendingDeletes(true);

  AtCommit_Notify();
  AtEOXact_GUC(true, 1);
  AtEOXact_SPI(true);
  AtEOXact_Enum();
  AtEOXact_on_commit_actions(true);
  AtEOXact_Namespace(true, is_parallel_worker);
  AtEOXact_SMgr();
  AtEOXact_Files(true);
  AtEOXact_ComboCid();
  AtEOXact_HashTables(true);
  AtEOXact_PgStat(true, is_parallel_worker);
  AtEOXact_Snapshot(true, false);
  AtEOXact_ApplyLauncher(true);
  pgstat_report_xact_timestamp(0);

  CurrentResourceOwner = NULL;
  ResourceOwnerDelete(TopTransactionResourceOwner);
  s->curTransactionOwner = NULL;
  CurTransactionResourceOwner = NULL;
  TopTransactionResourceOwner = NULL;

  AtCommit_Memory();

  s->fullTransactionId = InvalidFullTransactionId;
  s->subTransactionId = InvalidSubTransactionId;
  s->nestingLevel = 0;
  s->gucNestLevel = 0;
  s->childXids = NULL;
  s->nChildXids = 0;
  s->maxChildXids = 0;

  XactTopFullTransactionId = InvalidFullTransactionId;
  nParallelCurrentXids = 0;

     
                                                                        
             
     
  s->state = TRANS_DEFAULT;

  RESUME_INTERRUPTS();
}

   
                      
   
                                                                         
   
static void
PrepareTransaction(void)
{
  TransactionState s = CurrentTransactionState;
  TransactionId xid = GetCurrentTransactionId();
  GlobalTransaction gxact;
  TimestampTz prepared_at;

  Assert(!IsInParallelMode());

  ShowTransactionState("PrepareTransaction");

     
                                         
     
  if (s->state != TRANS_INPROGRESS)
  {
    elog(WARNING, "PrepareTransaction while in %s state", TransStateAsString(s->state));
  }
  Assert(s->parent == NULL);

     
                                                                            
                                                                      
                                                                             
                         
     
  for (;;)
  {
       
                                                     
       
    AfterTriggerFireDeferred();

       
                                                                          
                                                                          
                                                                 
       
    if (!PreCommit_Portals(true))
    {
      break;
    }
  }

  CallXactCallbacks(XACT_EVENT_PRE_PREPARE);

     
                                                                           
                                                                             
                                                                          
                                 
     

                                              
  AfterTriggerEndXact(true);

     
                                                                      
                                                    
     
  PreCommit_on_commit_actions();

                                                      
  AtEOXact_LargeObject(true);

                                             

     
                                                                     
                                                                             
                                                               
     
  PreCommit_CheckForSerializationFailure();

     
                                                                            
                                                                       
                                                                             
                                                                             
                                                                            
                                                    
     
                                                                             
                                                                          
                                                                           
                                          
     
                                                                            
                                         
     
                                                                         
                                                                    
                                                                       
     
  if ((MyXactFlags & XACT_FLAGS_ACCESSEDTEMPNAMESPACE))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE a transaction that has operated on temporary objects")));
  }

     
                                                                            
                                                                       
                                     
     
  if (XactHasExportedSnapshots())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE a transaction that has exported snapshots")));
  }

     
                                                                         
                          
     
  if (XactManipulatesLogicalReplicationWorkers())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot PREPARE a transaction that has manipulated logical replication workers")));
  }

                                                      
  HOLD_INTERRUPTS();

     
                                                                        
                        
     
  s->state = TRANS_PREPARE;

  prepared_at = GetCurrentTimestamp();

                                                  
  BufmgrCommit();

     
                                                                            
                                       
     
  gxact = MarkAsPreparing(xid, prepareGID, prepared_at, GetUserId(), MyDatabaseId);
  prepareGID = NULL;

     
                                                                           
                                                                        
                                                                          
                                                                         
                                                                     
                                                                       
                                                                             
                                                            
     
                                                                           
                                                                             
                                                                       
                                                                            
                                                        
     
  StartPrepare(gxact);

  AtPrepare_Notify();
  AtPrepare_Locks();
  AtPrepare_PredicateLocks();
  AtPrepare_PgStat();
  AtPrepare_MultiXact();
  AtPrepare_RelationMap();

     
                                            
     
                                                                       
                                                                            
                           
     
  EndPrepare(gxact);

     
                                                                            
     

                                                                        
  XactLastRecEnd = 0;

     
                                                                       
                                                                         
                                                                 
     
  PostPrepare_Locks(xid);

     
                                                                             
                                                                       
                                                      
     
  ProcArrayClearTransaction(MyProc);

     
                                                                            
                                                                         
                                                                           
                                                                           
                                                                             
                                                                          
                                                
     

  CallXactCallbacks(XACT_EVENT_PREPARE);

  ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_BEFORE_LOCKS, true, true);

                                            
  AtEOXact_Buffers(true);

                                   
  AtEOXact_RelationCache(true);

                                              

  PostPrepare_PgStat();

  PostPrepare_Inval();

  PostPrepare_smgr();

  PostPrepare_MultiXact(xid);

  PostPrepare_PredicateLocks(xid);

  ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_LOCKS, true, true);
  ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_AFTER_LOCKS, true, true);

     
                                                             
                                                                             
                                                                             
     
  PostPrepare_Twophase();

                                                                  
  AtEOXact_GUC(true, 1);
  AtEOXact_SPI(true);
  AtEOXact_Enum();
  AtEOXact_on_commit_actions(true);
  AtEOXact_Namespace(true, false);
  AtEOXact_SMgr();
  AtEOXact_Files(true);
  AtEOXact_ComboCid();
  AtEOXact_HashTables(true);
                                                                    
  AtEOXact_Snapshot(true, true);
  pgstat_report_xact_timestamp(0);

  CurrentResourceOwner = NULL;
  ResourceOwnerDelete(TopTransactionResourceOwner);
  s->curTransactionOwner = NULL;
  CurTransactionResourceOwner = NULL;
  TopTransactionResourceOwner = NULL;

  AtCommit_Memory();

  s->fullTransactionId = InvalidFullTransactionId;
  s->subTransactionId = InvalidSubTransactionId;
  s->nestingLevel = 0;
  s->gucNestLevel = 0;
  s->childXids = NULL;
  s->nChildXids = 0;
  s->maxChildXids = 0;

  XactTopFullTransactionId = InvalidFullTransactionId;
  nParallelCurrentXids = 0;

     
                                                                          
                     
     
  s->state = TRANS_DEFAULT;

  RESUME_INTERRUPTS();
}

   
                    
   
static void
AbortTransaction(void)
{
  TransactionState s = CurrentTransactionState;
  TransactionId latestXid;
  bool is_parallel_worker;

                                                      
  HOLD_INTERRUPTS();

                                                                   
  AtAbort_Memory();
  AtAbort_ResourceOwner();

     
                                                                      
                                                                     
                                                                          
                        
     
  LWLockReleaseAll();

                                                             
  pgstat_report_wait_end();
  pgstat_progress_end_command();

                                                         
  AbortBufferIO();
  UnlockBuffers();

                                           
  XLogResetInsertion();

                                       
  ConditionVariableCancelSleep();

     
                                                                             
                                                           
     
  LockErrorCleanup();

     
                                                                             
                                                                            
                                                                            
                                                                          
                                                 
     
  reschedule_timeouts();

     
                                                                           
                                                                           
                                                                 
     
  PG_SETMASK(&UnBlockSig);

     
                                         
     
  is_parallel_worker = (s->blockState == TBLOCK_PARALLEL_INPROGRESS);
  if (s->state != TRANS_INPROGRESS && s->state != TRANS_PREPARE)
  {
    elog(WARNING, "AbortTransaction while in %s state", TransStateAsString(s->state));
  }
  Assert(s->parent == NULL);

     
                                                                            
                      
     
  s->state = TRANS_ABORT;

     
                                                                            
                                                                            
                                                                           
                                                           
     
                                                                         
                                                                           
                                                 
     
  SetUserIdAndSecContext(s->prevUser, s->prevSecContext);

                                        
  ResetReindexState(s->nestingLevel);

                                    
  SnapBuildResetExportedSnapshotState();

                                                                     
  if (IsInParallelMode())
  {
    AtEOXact_Parallel(false);
    s->parallelModeLevel = 0;
  }

     
                         
     
  AfterTriggerEndXact(false);                               
  AtAbort_Portals();
  AtEOXact_LargeObject(false);
  AtAbort_Notify();
  AtEOXact_RelationMap(false, is_parallel_worker);
  AtAbort_Twophase();

     
                                                                            
                                                                            
                                                                            
             
     
  if (!is_parallel_worker)
  {
    latestXid = RecordTransactionAbort(false);
  }
  else
  {
    latestXid = InvalidTransactionId;

       
                                                                          
                                                                           
                                                                
       
    XLogSetAsyncXactLSN(XactLastRecEnd);
  }

  TRACE_POSTGRESQL_TRANSACTION_ABORT(MyProc->lxid);

     
                                                                            
                                                               
                             
     
  ProcArrayEndTransaction(MyProc, latestXid);

     
                                                                      
                                                                       
                                
     
  if (TopTransactionResourceOwner != NULL)
  {
    if (is_parallel_worker)
    {
      CallXactCallbacks(XACT_EVENT_PARALLEL_ABORT);
    }
    else
    {
      CallXactCallbacks(XACT_EVENT_ABORT);
    }

    ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_BEFORE_LOCKS, false, true);
    AtEOXact_Buffers(false);
    AtEOXact_RelationCache(false);
    AtEOXact_Inval(false);
    AtEOXact_MultiXact();
    ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_LOCKS, false, true);
    ResourceOwnerRelease(TopTransactionResourceOwner, RESOURCE_RELEASE_AFTER_LOCKS, false, true);
    smgrDoPendingDeletes(false);

    AtEOXact_GUC(false, 1);
    AtEOXact_SPI(false);
    AtEOXact_Enum();
    AtEOXact_on_commit_actions(false);
    AtEOXact_Namespace(false, is_parallel_worker);
    AtEOXact_SMgr();
    AtEOXact_Files(false);
    AtEOXact_ComboCid();
    AtEOXact_HashTables(false);
    AtEOXact_PgStat(false, is_parallel_worker);
    AtEOXact_ApplyLauncher(false);
    pgstat_report_xact_timestamp(0);
  }

     
                                                           
     
  RESUME_INTERRUPTS();
}

   
                      
   
static void
CleanupTransaction(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                
     
  if (s->state != TRANS_ABORT)
  {
    elog(FATAL, "CleanupTransaction: unexpected state %s", TransStateAsString(s->state));
  }

     
                                 
     
  AtCleanup_Portals();                                                   
  AtEOXact_Snapshot(false, true);                                              

  CurrentResourceOwner = NULL;                         
  if (TopTransactionResourceOwner)
  {
    ResourceOwnerDelete(TopTransactionResourceOwner);
  }
  s->curTransactionOwner = NULL;
  CurTransactionResourceOwner = NULL;
  TopTransactionResourceOwner = NULL;

  AtCleanup_Memory();                             

  s->fullTransactionId = InvalidFullTransactionId;
  s->subTransactionId = InvalidSubTransactionId;
  s->nestingLevel = 0;
  s->gucNestLevel = 0;
  s->childXids = NULL;
  s->nChildXids = 0;
  s->maxChildXids = 0;
  s->parallelModeLevel = 0;

  XactTopFullTransactionId = InvalidFullTransactionId;
  nParallelCurrentXids = 0;

     
                                                                       
             
     
  s->state = TRANS_DEFAULT;
}

   
                           
   
void
StartTransactionCommand(void)
{
  TransactionState s = CurrentTransactionState;

  switch (s->blockState)
  {
       
                                                                       
                    
       
  case TBLOCK_DEFAULT:
    StartTransaction();
    s->blockState = TBLOCK_STARTED;
    break;

       
                                                                     
                                                                 
                                                                      
                                                               
                                           
       
  case TBLOCK_INPROGRESS:
  case TBLOCK_IMPLICIT_INPROGRESS:
  case TBLOCK_SUBINPROGRESS:
    break;

       
                                                                      
                                                                 
                                                                    
                                                                    
                                                                       
                
       
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
    break;

                                  
  case TBLOCK_STARTED:
  case TBLOCK_BEGIN:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(ERROR, "StartTransactionCommand: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }

     
                                                                       
                                                                
     
  Assert(CurTransactionContext != NULL);
  MemoryContextSwitchTo(CurTransactionContext);
}

   
                                                                      
                                                                           
                                                                              
                                                                              
                                                                            
                                                              
   
static int save_XactIsoLevel;
static bool save_XactReadOnly;
static bool save_XactDeferrable;

void
SaveTransactionCharacteristics(void)
{
  save_XactIsoLevel = XactIsoLevel;
  save_XactReadOnly = XactReadOnly;
  save_XactDeferrable = XactDeferrable;
}

void
RestoreTransactionCharacteristics(void)
{
  XactIsoLevel = save_XactIsoLevel;
  XactReadOnly = save_XactReadOnly;
  XactDeferrable = save_XactDeferrable;
}

   
                            
   
void
CommitTransactionCommand(void)
{
  TransactionState s = CurrentTransactionState;

  if (s->chain)
  {
    SaveTransactionCharacteristics();
  }

  switch (s->blockState)
  {
       
                                                                  
                                                            
                                                                       
                                                             
       
  case TBLOCK_DEFAULT:
  case TBLOCK_PARALLEL_INPROGRESS:
    elog(FATAL, "CommitTransactionCommand: unexpected state %s", BlockStateAsString(s->blockState));
    break;

       
                                                              
                                                         
       
  case TBLOCK_STARTED:
    CommitTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                                     
                                                                     
                                                                   
                                 
       
  case TBLOCK_BEGIN:
    s->blockState = TBLOCK_INPROGRESS;
    break;

       
                                                                  
                                                                       
                           
       
  case TBLOCK_INPROGRESS:
  case TBLOCK_IMPLICIT_INPROGRESS:
  case TBLOCK_SUBINPROGRESS:
    CommandCounterIncrement();
    break;

       
                                                                      
                   
       
  case TBLOCK_END:
    CommitTransaction();
    s->blockState = TBLOCK_DEFAULT;
    if (s->chain)
    {
      StartTransaction();
      s->blockState = TBLOCK_INPROGRESS;
      s->chain = false;
      RestoreTransactionCharacteristics();
    }
    break;

       
                                                                       
                                                                   
                                                                
       
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
    break;

       
                                                                    
                                                           
                                                                 
       
  case TBLOCK_ABORT_END:
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    if (s->chain)
    {
      StartTransaction();
      s->blockState = TBLOCK_INPROGRESS;
      s->chain = false;
      RestoreTransactionCharacteristics();
    }
    break;

       
                                                                       
                                                                     
                          
       
  case TBLOCK_ABORT_PENDING:
    AbortTransaction();
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    if (s->chain)
    {
      StartTransaction();
      s->blockState = TBLOCK_INPROGRESS;
      s->chain = false;
      RestoreTransactionCharacteristics();
    }
    break;

       
                                                                     
                                 
       
  case TBLOCK_PREPARE:
    PrepareTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                                   
                                                             
                                                                    
               
       
  case TBLOCK_SUBBEGIN:
    StartSubTransaction();
    s->blockState = TBLOCK_SUBINPROGRESS;
    break;

       
                                                               
                                                                       
                                                                
                                      
       
  case TBLOCK_SUBRELEASE:
    do
    {
      CommitSubTransaction();
      s = CurrentTransactionState;                     
    } while (s->blockState == TBLOCK_SUBRELEASE);

    Assert(s->blockState == TBLOCK_INPROGRESS || s->blockState == TBLOCK_SUBINPROGRESS);
    break;

       
                                                                     
                                                                    
                                                                    
                                                                    
                                                              
                                                                  
                        
       
  case TBLOCK_SUBCOMMIT:
    do
    {
      CommitSubTransaction();
      s = CurrentTransactionState;                     
    } while (s->blockState == TBLOCK_SUBCOMMIT);
                                                                  
    if (s->blockState == TBLOCK_END)
    {
      Assert(s->parent == NULL);
      CommitTransaction();
      s->blockState = TBLOCK_DEFAULT;
      if (s->chain)
      {
        StartTransaction();
        s->blockState = TBLOCK_INPROGRESS;
        s->chain = false;
        RestoreTransactionCharacteristics();
      }
    }
    else if (s->blockState == TBLOCK_PREPARE)
    {
      Assert(s->parent == NULL);
      PrepareTransaction();
      s->blockState = TBLOCK_DEFAULT;
    }
    else
    {
      elog(ERROR, "CommitTransactionCommand: unexpected state %s", BlockStateAsString(s->blockState));
    }
    break;

       
                                                                    
                                                                  
                                                                     
       
  case TBLOCK_SUBABORT_END:
    CleanupSubTransaction();
    CommitTransactionCommand();
    break;

       
                                                        
       
  case TBLOCK_SUBABORT_PENDING:
    AbortSubTransaction();
    CleanupSubTransaction();
    CommitTransactionCommand();
    break;

       
                                                                 
                                                                   
                           
       
  case TBLOCK_SUBRESTART:
  {
    char *name;
    int savepointLevel;

                                                    
    name = s->name;
    s->name = NULL;
    savepointLevel = s->savepointLevel;

    AbortSubTransaction();
    CleanupSubTransaction();

    DefineSavepoint(NULL);
    s = CurrentTransactionState;                      
    s->name = name;
    s->savepointLevel = savepointLevel;

                                                  
    AssertState(s->blockState == TBLOCK_SUBBEGIN);
    StartSubTransaction();
    s->blockState = TBLOCK_SUBINPROGRESS;
  }
  break;

       
                                                                       
                                       
       
  case TBLOCK_SUBABORT_RESTART:
  {
    char *name;
    int savepointLevel;

                                                    
    name = s->name;
    s->name = NULL;
    savepointLevel = s->savepointLevel;

    CleanupSubTransaction();

    DefineSavepoint(NULL);
    s = CurrentTransactionState;                      
    s->name = name;
    s->savepointLevel = savepointLevel;

                                                  
    AssertState(s->blockState == TBLOCK_SUBBEGIN);
    StartSubTransaction();
    s->blockState = TBLOCK_SUBINPROGRESS;
  }
  break;
  }
}

   
                           
   
void
AbortCurrentTransaction(void)
{
  TransactionState s = CurrentTransactionState;

  switch (s->blockState)
  {
  case TBLOCK_DEFAULT:
    if (s->state == TRANS_DEFAULT)
    {
                                         
    }
    else
    {
         
                                                                 
                                                            
                                                              
                                                          
                           
         
      if (s->state == TRANS_START)
      {
        s->state = TRANS_INPROGRESS;
      }
      AbortTransaction();
      CleanupTransaction();
    }
    break;

       
                                                                       
                                                                      
                                                           
       
  case TBLOCK_STARTED:
  case TBLOCK_IMPLICIT_INPROGRESS:
    AbortTransaction();
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                                     
                                                                   
                                                                       
                                                                       
              
       
  case TBLOCK_BEGIN:
    AbortTransaction();
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                                  
                                                                      
                                                                    
       
  case TBLOCK_INPROGRESS:
  case TBLOCK_PARALLEL_INPROGRESS:
    AbortTransaction();
    s->blockState = TBLOCK_ABORT;
                                                                  
    break;

       
                                                             
                                                                       
                         
       
  case TBLOCK_END:
    AbortTransaction();
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                                    
                                                                       
                                          
       
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
    break;

       
                                                                       
                                                                       
              
       
  case TBLOCK_ABORT_END:
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                                   
                                         
       
  case TBLOCK_ABORT_PENDING:
    AbortTransaction();
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                              
                                                                       
                         
       
  case TBLOCK_PREPARE:
    AbortTransaction();
    CleanupTransaction();
    s->blockState = TBLOCK_DEFAULT;
    break;

       
                                                                
                                                                     
                        
       
  case TBLOCK_SUBINPROGRESS:
    AbortSubTransaction();
    s->blockState = TBLOCK_SUBABORT;
    break;

       
                                                                      
                                                                 
                                                                  
       
  case TBLOCK_SUBBEGIN:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
    AbortSubTransaction();
    CleanupSubTransaction();
    AbortCurrentTransaction();
    break;

       
                                                           
       
  case TBLOCK_SUBABORT_END:
  case TBLOCK_SUBABORT_RESTART:
    CleanupSubTransaction();
    AbortCurrentTransaction();
    break;
  }
}

   
                             
   
                                                                       
                                                                      
                                        
   
                                                                         
                                                                      
                          
   
                                                                              
                                                                             
                                                                              
                                                  
   
                                                                             
                                                                              
   
                                                                           
                                                                       
                                                                             
                                                      
   
void
PreventInTransactionBlock(bool isTopLevel, const char *stmtType)
{
     
                                 
     
  if (IsTransactionBlock())
  {
    ereport(ERROR, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
                                                                            
                       errmsg("%s cannot run inside a transaction block", stmtType)));
  }

     
                     
     
  if (IsSubTransaction())
  {
    ereport(ERROR, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
                                                                            
                       errmsg("%s cannot run inside a subtransaction", stmtType)));
  }

     
                                                                 
     
  if (MyXactFlags & XACT_FLAGS_PIPELINING)
  {
    ereport(ERROR, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
                                                                            
                       errmsg("%s cannot be executed within a pipeline", stmtType)));
  }

     
                             
     
  if (!isTopLevel)
  {
    ereport(ERROR, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION),
                                                                            
                       errmsg("%s cannot be executed from a function", stmtType)));
  }

                                                                          
  if (CurrentTransactionState->blockState != TBLOCK_DEFAULT && CurrentTransactionState->blockState != TBLOCK_STARTED)
  {
    elog(FATAL, "cannot prevent transaction chain");
  }

                                                                           
  MyXactFlags |= XACT_FLAGS_NEEDIMMEDIATECOMMIT;
}

   
                          
                           
   
                                                                             
                                                                             
                                                                            
                                                                             
                                                                             
                                                                             
           
   
                                                                        
                                                                          
                                                                      
                                                          
   
                                                                           
                      
                                                                 
   
void
WarnNoTransactionBlock(bool isTopLevel, const char *stmtType)
{
  CheckTransactionBlock(isTopLevel, false, stmtType);
}

void
RequireTransactionBlock(bool isTopLevel, const char *stmtType)
{
  CheckTransactionBlock(isTopLevel, true, stmtType);
}

   
                                                
   
static void
CheckTransactionBlock(bool isTopLevel, bool throwError, const char *stmtType)
{
     
                                 
     
  if (IsTransactionBlock())
  {
    return;
  }

     
                     
     
  if (IsSubTransaction())
  {
    return;
  }

     
                             
     
  if (!isTopLevel)
  {
    return;
  }

  ereport(throwError ? ERROR : WARNING, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
                                                                                                 
                                            errmsg("%s can only be used in transaction blocks", stmtType)));
  return;
}

   
                        
   
                                                                         
                                                                         
                               
   
                                                                          
                                                                           
                                                                            
                                                                         
                                                     
   
                                                                           
                      
   
bool
IsInTransactionBlock(bool isTopLevel)
{
     
                                                    
                                         
     
  if (IsTransactionBlock())
  {
    return true;
  }

  if (IsSubTransaction())
  {
    return true;
  }

  if (MyXactFlags & XACT_FLAGS_PIPELINING)
  {
    return true;
  }

  if (!isTopLevel)
  {
    return true;
  }

  if (CurrentTransactionState->blockState != TBLOCK_DEFAULT && CurrentTransactionState->blockState != TBLOCK_STARTED)
  {
    return true;
  }

  return false;
}

   
                                                                        
               
   
                                                                       
                                                                         
                                                                             
   
                                                                             
                                                       
   
void
RegisterXactCallback(XactCallback callback, void *arg)
{
  XactCallbackItem *item;

  item = (XactCallbackItem *)MemoryContextAlloc(TopMemoryContext, sizeof(XactCallbackItem));
  item->callback = callback;
  item->arg = arg;
  item->next = Xact_callbacks;
  Xact_callbacks = item;
}

void
UnregisterXactCallback(XactCallback callback, void *arg)
{
  XactCallbackItem *item;
  XactCallbackItem *prev;

  prev = NULL;
  for (item = Xact_callbacks; item; prev = item, item = item->next)
  {
    if (item->callback == callback && item->arg == arg)
    {
      if (prev)
      {
        prev->next = item->next;
      }
      else
      {
        Xact_callbacks = item->next;
      }
      pfree(item);
      break;
    }
  }
}

static void
CallXactCallbacks(XactEvent event)
{
  XactCallbackItem *item;

  for (item = Xact_callbacks; item; item = item->next)
  {
    item->callback(event, item->arg);
  }
}

   
                                                                           
               
   
                                                             
   
                                                                               
                                                                  
                                                                            
                          
   
void
RegisterSubXactCallback(SubXactCallback callback, void *arg)
{
  SubXactCallbackItem *item;

  item = (SubXactCallbackItem *)MemoryContextAlloc(TopMemoryContext, sizeof(SubXactCallbackItem));
  item->callback = callback;
  item->arg = arg;
  item->next = SubXact_callbacks;
  SubXact_callbacks = item;
}

void
UnregisterSubXactCallback(SubXactCallback callback, void *arg)
{
  SubXactCallbackItem *item;
  SubXactCallbackItem *prev;

  prev = NULL;
  for (item = SubXact_callbacks; item; prev = item, item = item->next)
  {
    if (item->callback == callback && item->arg == arg)
    {
      if (prev)
      {
        prev->next = item->next;
      }
      else
      {
        SubXact_callbacks = item->next;
      }
      pfree(item);
      break;
    }
  }
}

static void
CallSubXactCallbacks(SubXactEvent event, SubTransactionId mySubid, SubTransactionId parentSubid)
{
  SubXactCallbackItem *item;

  for (item = SubXact_callbacks; item; item = item->next)
  {
    item->callback(event, mySubid, parentSubid, item->arg);
  }
}

                                                                    
                                    
                                                                    
   

   
                         
                                   
   
void
BeginTransactionBlock(void)
{
  TransactionState s = CurrentTransactionState;

  switch (s->blockState)
  {
       
                                                                     
       
  case TBLOCK_STARTED:
    s->blockState = TBLOCK_BEGIN;
    break;

       
                                                                      
                                                                
                                                                      
       
  case TBLOCK_IMPLICIT_INPROGRESS:
    s->blockState = TBLOCK_BEGIN;
    break;

       
                                                
       
  case TBLOCK_INPROGRESS:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBINPROGRESS:
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
    ereport(WARNING, (errcode(ERRCODE_ACTIVE_SQL_TRANSACTION), errmsg("there is already a transaction in progress")));
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_BEGIN:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(FATAL, "BeginTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }
}

   
                           
                                     
   
                                                                       
                                                   
   
                                                                          
                                                                    
                                                                           
                                                        
   
bool
PrepareTransactionBlock(const char *gid)
{
  TransactionState s;
  bool result;

                                                
  result = EndTransactionBlock(false);

                                                           
  if (result)
  {
    s = CurrentTransactionState;

    while (s->parent != NULL)
    {
      s = s->parent;
    }

    if (s->blockState == TBLOCK_END)
    {
                                                               
      prepareGID = MemoryContextStrdup(TopTransactionContext, gid);

      s->blockState = TBLOCK_PREPARE;
    }
    else
    {
         
                                                        
                                                       
         
      Assert(s->blockState == TBLOCK_STARTED || s->blockState == TBLOCK_IMPLICIT_INPROGRESS);
                                                   
      result = false;
    }
  }

  return result;
}

   
                       
                                    
   
                                                                      
                                                  
   
                                                                          
                                                                          
                                                                           
                                                        
   
bool
EndTransactionBlock(bool chain)
{
  TransactionState s = CurrentTransactionState;
  bool result = false;

  switch (s->blockState)
  {
       
                                                                       
                  
       
  case TBLOCK_INPROGRESS:
    s->blockState = TBLOCK_END;
    result = true;
    break;

       
                                                                  
                                                                
                                                        
       
  case TBLOCK_IMPLICIT_INPROGRESS:
    if (chain)
    {
      ereport(ERROR, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
                                                                              
                         errmsg("%s can only be used in transaction blocks", "COMMIT AND CHAIN")));
    }
    else
    {
      ereport(WARNING, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION), errmsg("there is no transaction in progress")));
    }
    s->blockState = TBLOCK_END;
    result = true;
    break;

       
                                                   
                                                             
       
  case TBLOCK_ABORT:
    s->blockState = TBLOCK_ABORT_END;
    break;

       
                                                                       
                                                                  
       
  case TBLOCK_SUBINPROGRESS:
    while (s->parent != NULL)
    {
      if (s->blockState == TBLOCK_SUBINPROGRESS)
      {
        s->blockState = TBLOCK_SUBCOMMIT;
      }
      else
      {
        elog(FATAL, "EndTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
      }
      s = s->parent;
    }
    if (s->blockState == TBLOCK_INPROGRESS)
    {
      s->blockState = TBLOCK_END;
    }
    else
    {
      elog(FATAL, "EndTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
    }
    result = true;
    break;

       
                                                                       
                                                                 
                    
       
  case TBLOCK_SUBABORT:
    while (s->parent != NULL)
    {
      if (s->blockState == TBLOCK_SUBINPROGRESS)
      {
        s->blockState = TBLOCK_SUBABORT_PENDING;
      }
      else if (s->blockState == TBLOCK_SUBABORT)
      {
        s->blockState = TBLOCK_SUBABORT_END;
      }
      else
      {
        elog(FATAL, "EndTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
      }
      s = s->parent;
    }
    if (s->blockState == TBLOCK_INPROGRESS)
    {
      s->blockState = TBLOCK_ABORT_PENDING;
    }
    else if (s->blockState == TBLOCK_ABORT)
    {
      s->blockState = TBLOCK_ABORT_END;
    }
    else
    {
      elog(FATAL, "EndTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
    }
    break;

       
                                                                  
                                                         
                                                   
                                                                      
                                                                  
              
       
  case TBLOCK_STARTED:
    if (chain)
    {
      ereport(ERROR, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
                                                                              
                         errmsg("%s can only be used in transaction blocks", "COMMIT AND CHAIN")));
    }
    else
    {
      ereport(WARNING, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION), errmsg("there is no transaction in progress")));
    }
    result = true;
    break;

       
                                                                   
                                         
       
  case TBLOCK_PARALLEL_INPROGRESS:
    ereport(FATAL, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot commit during a parallel operation")));
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_BEGIN:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(FATAL, "EndTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }

  Assert(s->blockState == TBLOCK_STARTED || s->blockState == TBLOCK_END || s->blockState == TBLOCK_ABORT_END || s->blockState == TBLOCK_ABORT_PENDING);

  s->chain = chain;

  return result;
}

   
                             
                                      
   
                                                                          
   
void
UserAbortTransactionBlock(bool chain)
{
  TransactionState s = CurrentTransactionState;

  switch (s->blockState)
  {
       
                                                                       
                                                                    
                                   
       
  case TBLOCK_INPROGRESS:
    s->blockState = TBLOCK_ABORT_PENDING;
    break;

       
                                                                      
                                                                    
                                                                   
                   
       
  case TBLOCK_ABORT:
    s->blockState = TBLOCK_ABORT_END;
    break;

       
                                                                  
                          
       
  case TBLOCK_SUBINPROGRESS:
  case TBLOCK_SUBABORT:
    while (s->parent != NULL)
    {
      if (s->blockState == TBLOCK_SUBINPROGRESS)
      {
        s->blockState = TBLOCK_SUBABORT_PENDING;
      }
      else if (s->blockState == TBLOCK_SUBABORT)
      {
        s->blockState = TBLOCK_SUBABORT_END;
      }
      else
      {
        elog(FATAL, "UserAbortTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
      }
      s = s->parent;
    }
    if (s->blockState == TBLOCK_INPROGRESS)
    {
      s->blockState = TBLOCK_ABORT_PENDING;
    }
    else if (s->blockState == TBLOCK_ABORT)
    {
      s->blockState = TBLOCK_ABORT_END;
    }
    else
    {
      elog(FATAL, "UserAbortTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
    }
    break;

       
                                                                 
                                                                      
                                                                     
                                                                       
       
                                                                       
                                                                      
                                                                 
                                                                
       
  case TBLOCK_STARTED:
  case TBLOCK_IMPLICIT_INPROGRESS:
    if (chain)
    {
      ereport(ERROR, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
                                                                              
                         errmsg("%s can only be used in transaction blocks", "ROLLBACK AND CHAIN")));
    }
    else
    {
      ereport(WARNING, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION), errmsg("there is no transaction in progress")));
    }
    s->blockState = TBLOCK_ABORT_PENDING;
    break;

       
                                                                   
                                         
       
  case TBLOCK_PARALLEL_INPROGRESS:
    ereport(FATAL, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot abort during a parallel operation")));
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_BEGIN:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(FATAL, "UserAbortTransactionBlock: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }

  Assert(s->blockState == TBLOCK_ABORT_END || s->blockState == TBLOCK_ABORT_PENDING);

  s->chain = chain;
}

   
                                 
                                                                     
   
                                                                            
                                                                         
                                                             
                                                     
   
void
BeginImplicitTransactionBlock(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                         
                                                                           
            
     
                                                                         
                                                                            
                             
     
  if (s->blockState == TBLOCK_STARTED)
  {
    s->blockState = TBLOCK_IMPLICIT_INPROGRESS;
  }
}

   
                               
                                                        
   
                                                                             
                                                                          
   
void
EndImplicitTransactionBlock(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                           
                                                                          
                                                                          
     
                                                                         
                                                                            
                             
     
  if (s->blockState == TBLOCK_IMPLICIT_INPROGRESS)
  {
    s->blockState = TBLOCK_STARTED;
  }
}

   
                   
                                       
   
void
DefineSavepoint(const char *name)
{
  TransactionState s = CurrentTransactionState;

     
                                                                             
                                                                       
                                                                             
                                                                            
             
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot define savepoints during a parallel operation")));
  }

  switch (s->blockState)
  {
  case TBLOCK_INPROGRESS:
  case TBLOCK_SUBINPROGRESS:
                                     
    PushTransaction();
    s = CurrentTransactionState;                      

       
                                                                     
                                 
       
    if (name)
    {
      s->name = MemoryContextStrdup(TopTransactionContext, name);
    }
    break;

       
                                                                      
                                                                     
                                                                    
                                                                       
                                                                      
                                                                     
                                                                     
                  
       
                                                                   
                                                                
                                  
       
  case TBLOCK_IMPLICIT_INPROGRESS:
    ereport(ERROR, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
                                                                            
                       errmsg("%s can only be used in transaction blocks", "SAVEPOINT")));
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_STARTED:
  case TBLOCK_BEGIN:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(FATAL, "DefineSavepoint: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }
}

   
                    
                                     
   
                                                                          
   
void
ReleaseSavepoint(const char *name)
{
  TransactionState s = CurrentTransactionState;
  TransactionState target, xact;

     
                                                                             
                                                                            
                                                                             
                                                                            
             
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot release savepoints during a parallel operation")));
  }

  switch (s->blockState)
  {
       
                                                                      
       
  case TBLOCK_INPROGRESS:
    ereport(ERROR, (errcode(ERRCODE_S_E_INVALID_SPECIFICATION), errmsg("savepoint \"%s\" does not exist", name)));
    break;

  case TBLOCK_IMPLICIT_INPROGRESS:
                                                                    
    ereport(ERROR, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
                                                                            
                       errmsg("%s can only be used in transaction blocks", "RELEASE SAVEPOINT")));
    break;

       
                                                                       
             
       
  case TBLOCK_SUBINPROGRESS:
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_STARTED:
  case TBLOCK_BEGIN:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(FATAL, "ReleaseSavepoint: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }

  for (target = s; PointerIsValid(target); target = target->parent)
  {
    if (PointerIsValid(target->name) && strcmp(target->name, name) == 0)
    {
      break;
    }
  }

  if (!PointerIsValid(target))
  {
    ereport(ERROR, (errcode(ERRCODE_S_E_INVALID_SPECIFICATION), errmsg("savepoint \"%s\" does not exist", name)));
  }

                                                    
  if (target->savepointLevel != s->savepointLevel)
  {
    ereport(ERROR, (errcode(ERRCODE_S_E_INVALID_SPECIFICATION), errmsg("savepoint \"%s\" does not exist within current savepoint level", name)));
  }

     
                                                                
                                                                          
                               
     
  xact = CurrentTransactionState;
  for (;;)
  {
    Assert(xact->blockState == TBLOCK_SUBINPROGRESS);
    xact->blockState = TBLOCK_SUBRELEASE;
    if (xact == target)
    {
      break;
    }
    xact = xact->parent;
    Assert(PointerIsValid(xact));
  }
}

   
                       
                                                     
   
                                                                          
   
void
RollbackToSavepoint(const char *name)
{
  TransactionState s = CurrentTransactionState;
  TransactionState target, xact;

     
                                                                             
                                                                            
                                                                             
                                                                            
             
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot rollback to savepoints during a parallel operation")));
  }

  switch (s->blockState)
  {
       
                                                                 
                
       
  case TBLOCK_INPROGRESS:
  case TBLOCK_ABORT:
    ereport(ERROR, (errcode(ERRCODE_S_E_INVALID_SPECIFICATION), errmsg("savepoint \"%s\" does not exist", name)));
    break;

  case TBLOCK_IMPLICIT_INPROGRESS:
                                                                    
    ereport(ERROR, (errcode(ERRCODE_NO_ACTIVE_SQL_TRANSACTION),
                                                                            
                       errmsg("%s can only be used in transaction blocks", "ROLLBACK TO SAVEPOINT")));
    break;

       
                                                    
       
  case TBLOCK_SUBINPROGRESS:
  case TBLOCK_SUBABORT:
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_STARTED:
  case TBLOCK_BEGIN:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(FATAL, "RollbackToSavepoint: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }

  for (target = s; PointerIsValid(target); target = target->parent)
  {
    if (PointerIsValid(target->name) && strcmp(target->name, name) == 0)
    {
      break;
    }
  }

  if (!PointerIsValid(target))
  {
    ereport(ERROR, (errcode(ERRCODE_S_E_INVALID_SPECIFICATION), errmsg("savepoint \"%s\" does not exist", name)));
  }

                                                    
  if (target->savepointLevel != s->savepointLevel)
  {
    ereport(ERROR, (errcode(ERRCODE_S_E_INVALID_SPECIFICATION), errmsg("savepoint \"%s\" does not exist within current savepoint level", name)));
  }

     
                                                               
                                                                         
                               
     
  xact = CurrentTransactionState;
  for (;;)
  {
    if (xact == target)
    {
      break;
    }
    if (xact->blockState == TBLOCK_SUBINPROGRESS)
    {
      xact->blockState = TBLOCK_SUBABORT_PENDING;
    }
    else if (xact->blockState == TBLOCK_SUBABORT)
    {
      xact->blockState = TBLOCK_SUBABORT_END;
    }
    else
    {
      elog(FATAL, "RollbackToSavepoint: unexpected state %s", BlockStateAsString(xact->blockState));
    }
    xact = xact->parent;
    Assert(PointerIsValid(xact));
  }

                                                
  if (xact->blockState == TBLOCK_SUBINPROGRESS)
  {
    xact->blockState = TBLOCK_SUBRESTART;
  }
  else if (xact->blockState == TBLOCK_SUBABORT)
  {
    xact->blockState = TBLOCK_SUBABORT_RESTART;
  }
  else
  {
    elog(FATAL, "RollbackToSavepoint: unexpected state %s", BlockStateAsString(xact->blockState));
  }
}

   
                               
                                                                         
                                                                       
                                                                          
                                                                       
                                                      
                                                                          
                         
   
void
BeginInternalSubTransaction(const char *name)
{
  TransactionState s = CurrentTransactionState;

     
                                                                             
                                                                       
                                                                  
                                                                             
                                                                           
                                                                  
                                                                             
                          
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot start subtransactions during a parallel operation")));
  }

  switch (s->blockState)
  {
  case TBLOCK_STARTED:
  case TBLOCK_INPROGRESS:
  case TBLOCK_IMPLICIT_INPROGRESS:
  case TBLOCK_END:
  case TBLOCK_PREPARE:
  case TBLOCK_SUBINPROGRESS:
                                     
    PushTransaction();
    s = CurrentTransactionState;                      

       
                                                                     
                                 
       
    if (name)
    {
      s->name = MemoryContextStrdup(TopTransactionContext, name);
    }
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_BEGIN:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
    elog(FATAL, "BeginInternalSubTransaction: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }

  CommitTransactionCommand();
  StartTransactionCommand();
}

   
                                
   
                                                                        
                            
                                                                              
   
void
ReleaseCurrentSubTransaction(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                             
                                                                             
                                                                     
                                                                        
            
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot commit subtransactions during a parallel operation")));
  }

  if (s->blockState != TBLOCK_SUBINPROGRESS)
  {
    elog(ERROR, "ReleaseCurrentSubTransaction: unexpected state %s", BlockStateAsString(s->blockState));
  }
  Assert(s->state == TRANS_INPROGRESS);
  MemoryContextSwitchTo(CurTransactionContext);
  CommitSubTransaction();
  s = CurrentTransactionState;                     
  Assert(s->state == TRANS_INPROGRESS);
}

   
                                           
   
                                                                             
                                   
                                                                              
   
void
RollbackAndReleaseCurrentSubTransaction(void)
{
  TransactionState s = CurrentTransactionState;

     
                                                                        
                                                                          
                                                                         
                                                                         
                       
     

  switch (s->blockState)
  {
                                     
  case TBLOCK_SUBINPROGRESS:
  case TBLOCK_SUBABORT:
    break;

                                  
  case TBLOCK_DEFAULT:
  case TBLOCK_STARTED:
  case TBLOCK_BEGIN:
  case TBLOCK_IMPLICIT_INPROGRESS:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_INPROGRESS:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_ABORT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
  case TBLOCK_PREPARE:
    elog(FATAL, "RollbackAndReleaseCurrentSubTransaction: unexpected state %s", BlockStateAsString(s->blockState));
    break;
  }

     
                                                  
     
  if (s->blockState == TBLOCK_SUBINPROGRESS)
  {
    AbortSubTransaction();
  }

                            
  CleanupSubTransaction();

  s = CurrentTransactionState;                     
  AssertState(s->blockState == TBLOCK_SUBINPROGRESS || s->blockState == TBLOCK_INPROGRESS || s->blockState == TBLOCK_IMPLICIT_INPROGRESS || s->blockState == TBLOCK_STARTED);
}

   
                            
   
                                                                        
                                                                          
               
   
void
AbortOutOfAnyTransaction(void)
{
  TransactionState s = CurrentTransactionState;

                                                           
  AtAbort_Memory();

     
                                                      
     
  do
  {
    switch (s->blockState)
    {
    case TBLOCK_DEFAULT:
      if (s->state == TRANS_DEFAULT)
      {
                                              
      }
      else
      {
           
                                                                   
                                                              
                                                                
                                                            
                             
           
        if (s->state == TRANS_START)
        {
          s->state = TRANS_INPROGRESS;
        }
        AbortTransaction();
        CleanupTransaction();
      }
      break;
    case TBLOCK_STARTED:
    case TBLOCK_BEGIN:
    case TBLOCK_INPROGRESS:
    case TBLOCK_IMPLICIT_INPROGRESS:
    case TBLOCK_PARALLEL_INPROGRESS:
    case TBLOCK_END:
    case TBLOCK_ABORT_PENDING:
    case TBLOCK_PREPARE:
                                         
      AbortTransaction();
      CleanupTransaction();
      s->blockState = TBLOCK_DEFAULT;
      break;
    case TBLOCK_ABORT:
    case TBLOCK_ABORT_END:

         
                                                               
                                                                 
                                                                    
                                                               
         
      AtAbort_Portals();
      CleanupTransaction();
      s->blockState = TBLOCK_DEFAULT;
      break;

         
                                                                  
         
    case TBLOCK_SUBBEGIN:
    case TBLOCK_SUBINPROGRESS:
    case TBLOCK_SUBRELEASE:
    case TBLOCK_SUBCOMMIT:
    case TBLOCK_SUBABORT_PENDING:
    case TBLOCK_SUBRESTART:
      AbortSubTransaction();
      CleanupSubTransaction();
      s = CurrentTransactionState;                     
      break;

    case TBLOCK_SUBABORT:
    case TBLOCK_SUBABORT_END:
    case TBLOCK_SUBABORT_RESTART:
                                                          
      if (s->curTransactionOwner)
      {
                                                                 
        AtSubAbort_Portals(s->subTransactionId, s->parent->subTransactionId, s->curTransactionOwner, s->parent->curTransactionOwner);
      }
      CleanupSubTransaction();
      s = CurrentTransactionState;                     
      break;
    }
  } while (s->blockState != TBLOCK_DEFAULT);

                                         
  Assert(s->parent == NULL);

                                                                             
  AtCleanup_Memory();
}

   
                                                             
   
bool
IsTransactionBlock(void)
{
  TransactionState s = CurrentTransactionState;

  if (s->blockState == TBLOCK_DEFAULT || s->blockState == TBLOCK_STARTED)
  {
    return false;
  }

  return true;
}

   
                                                                          
                                                                        
                   
   
                                                                        
   
bool
IsTransactionOrTransactionBlock(void)
{
  TransactionState s = CurrentTransactionState;

  if (s->blockState == TBLOCK_DEFAULT)
  {
    return false;
  }

  return true;
}

   
                                                                            
   
char
TransactionBlockStatusCode(void)
{
  TransactionState s = CurrentTransactionState;

  switch (s->blockState)
  {
  case TBLOCK_DEFAULT:
  case TBLOCK_STARTED:
    return 'I';                                  
  case TBLOCK_BEGIN:
  case TBLOCK_SUBBEGIN:
  case TBLOCK_INPROGRESS:
  case TBLOCK_IMPLICIT_INPROGRESS:
  case TBLOCK_PARALLEL_INPROGRESS:
  case TBLOCK_SUBINPROGRESS:
  case TBLOCK_END:
  case TBLOCK_SUBRELEASE:
  case TBLOCK_SUBCOMMIT:
  case TBLOCK_PREPARE:
    return 'T';                     
  case TBLOCK_ABORT:
  case TBLOCK_SUBABORT:
  case TBLOCK_ABORT_END:
  case TBLOCK_SUBABORT_END:
  case TBLOCK_ABORT_PENDING:
  case TBLOCK_SUBABORT_PENDING:
  case TBLOCK_SUBRESTART:
  case TBLOCK_SUBABORT_RESTART:
    return 'E';                            
  }

                             
  elog(FATAL, "invalid transaction block state: %s", BlockStateAsString(s->blockState));
  return 0;                          
}

   
                    
   
bool
IsSubTransaction(void)
{
  TransactionState s = CurrentTransactionState;

  if (s->nestingLevel >= 2)
  {
    return true;
  }

  return false;
}

   
                       
   
                                                                               
                                                                          
                                                                         
                                                                         
                                                                         
                                                                        
                                                                           
                                          
   
static void
StartSubTransaction(void)
{
  TransactionState s = CurrentTransactionState;

  if (s->state != TRANS_DEFAULT)
  {
    elog(WARNING, "StartSubTransaction while in %s state", TransStateAsString(s->state));
  }

  s->state = TRANS_START;

     
                                                  
     
                                                     
     
  AtSubStart_Memory();
  AtSubStart_ResourceOwner();
  AtSubStart_Notify();
  AfterTriggerBeginSubXact();

  s->state = TRANS_INPROGRESS;

     
                                     
     
  CallSubXactCallbacks(SUBXACT_EVENT_START_SUB, s->subTransactionId, s->parent->subTransactionId);

  ShowTransactionState("StartSubTransaction");
}

   
                        
   
                                                                          
                                                                
   
static void
CommitSubTransaction(void)
{
  TransactionState s = CurrentTransactionState;

  ShowTransactionState("CommitSubTransaction");

  if (s->state != TRANS_INPROGRESS)
  {
    elog(WARNING, "CommitSubTransaction while in %s state", TransStateAsString(s->state));
  }

                                       

  CallSubXactCallbacks(SUBXACT_EVENT_PRE_COMMIT_SUB, s->subTransactionId, s->parent->subTransactionId);

                                                                     
  if (IsInParallelMode())
  {
    AtEOSubXact_Parallel(true, s->subTransactionId);
    s->parallelModeLevel = 0;
  }

                                             
  s->state = TRANS_COMMIT;

                                                                      
  CommandCounterIncrement();

     
                                                                          
                                                                         
                                                          
     

                           
  if (FullTransactionIdIsValid(s->fullTransactionId))
  {
    AtSubCommit_childXids();
  }
  AfterTriggerEndSubXact(true);
  AtSubCommit_Portals(s->subTransactionId, s->parent->subTransactionId, s->parent->nestingLevel, s->parent->curTransactionOwner);
  AtEOSubXact_LargeObject(true, s->subTransactionId, s->parent->subTransactionId);
  AtSubCommit_Notify();

  CallSubXactCallbacks(SUBXACT_EVENT_COMMIT_SUB, s->subTransactionId, s->parent->subTransactionId);

  ResourceOwnerRelease(s->curTransactionOwner, RESOURCE_RELEASE_BEFORE_LOCKS, true, false);
  AtEOSubXact_RelationCache(true, s->subTransactionId, s->parent->subTransactionId);
  AtEOSubXact_Inval(true);
  AtSubCommit_smgr();

     
                                                                            
     
  CurrentResourceOwner = s->curTransactionOwner;
  if (FullTransactionIdIsValid(s->fullTransactionId))
  {
    XactLockTableDelete(XidFromFullTransactionId(s->fullTransactionId));
  }

     
                                                                        
     
  ResourceOwnerRelease(s->curTransactionOwner, RESOURCE_RELEASE_LOCKS, true, false);
  ResourceOwnerRelease(s->curTransactionOwner, RESOURCE_RELEASE_AFTER_LOCKS, true, false);

  AtEOXact_GUC(true, s->gucNestLevel);
  AtEOSubXact_SPI(true, s->subTransactionId);
  AtEOSubXact_on_commit_actions(true, s->subTransactionId, s->parent->subTransactionId);
  AtEOSubXact_Namespace(true, s->subTransactionId, s->parent->subTransactionId);
  AtEOSubXact_Files(true, s->subTransactionId, s->parent->subTransactionId);
  AtEOSubXact_HashTables(true, s->nestingLevel);
  AtEOSubXact_PgStat(true, s->nestingLevel);
  AtSubCommit_Snapshot(s->nestingLevel);
  AtEOSubXact_ApplyLauncher(true, s->nestingLevel);

     
                                                                             
                                                                            
                                                     
     
  XactReadOnly = s->prevXactReadOnly;

  CurrentResourceOwner = s->parent->curTransactionOwner;
  CurTransactionResourceOwner = s->parent->curTransactionOwner;
  ResourceOwnerDelete(s->curTransactionOwner);
  s->curTransactionOwner = NULL;

  AtSubCommit_Memory();

  s->state = TRANS_DEFAULT;

  PopTransaction();
}

   
                       
   
static void
AbortSubTransaction(void)
{
  TransactionState s = CurrentTransactionState;

                                                      
  HOLD_INTERRUPTS();

                                                                   
  AtSubAbort_Memory();
  AtSubAbort_ResourceOwner();

     
                                                                      
                                                                     
                                                                          
                        
     
                                                                          
                                                                    
     
  LWLockReleaseAll();

  pgstat_report_wait_end();
  pgstat_progress_end_command();
  AbortBufferIO();
  UnlockBuffers();

                                           
  XLogResetInsertion();

                                       
  ConditionVariableCancelSleep();

     
                                                                             
                                                           
     
  LockErrorCleanup();

     
                                                                             
                                                                            
                                                                            
                                                                          
                                                 
     
  reschedule_timeouts();

     
                                                                           
                                                                           
                                                                 
     
  PG_SETMASK(&UnBlockSig);

     
                                         
     
  ShowTransactionState("AbortSubTransaction");

  if (s->state != TRANS_INPROGRESS)
  {
    elog(WARNING, "AbortSubTransaction while in %s state", TransStateAsString(s->state));
  }

  s->state = TRANS_ABORT;

     
                                                                             
                        
     
  SetUserIdAndSecContext(s->prevUser, s->prevSecContext);

                                        
  ResetReindexState(s->nestingLevel);

     
                                                                      
                                                   
     

                                              
  if (IsInParallelMode())
  {
    AtEOSubXact_Parallel(false, s->subTransactionId);
    s->parallelModeLevel = 0;
  }

     
                                                                        
                      
     
  if (s->curTransactionOwner)
  {
    AfterTriggerEndSubXact(false);
    AtSubAbort_Portals(s->subTransactionId, s->parent->subTransactionId, s->curTransactionOwner, s->parent->curTransactionOwner);
    AtEOSubXact_LargeObject(false, s->subTransactionId, s->parent->subTransactionId);
    AtSubAbort_Notify();

                                                        
    (void)RecordTransactionAbort(true);

                            
    if (FullTransactionIdIsValid(s->fullTransactionId))
    {
      AtSubAbort_childXids();
    }

    CallSubXactCallbacks(SUBXACT_EVENT_ABORT_SUB, s->subTransactionId, s->parent->subTransactionId);

    ResourceOwnerRelease(s->curTransactionOwner, RESOURCE_RELEASE_BEFORE_LOCKS, false, false);
    AtEOSubXact_RelationCache(false, s->subTransactionId, s->parent->subTransactionId);
    AtEOSubXact_Inval(false);
    ResourceOwnerRelease(s->curTransactionOwner, RESOURCE_RELEASE_LOCKS, false, false);
    ResourceOwnerRelease(s->curTransactionOwner, RESOURCE_RELEASE_AFTER_LOCKS, false, false);
    AtSubAbort_smgr();

    AtEOXact_GUC(false, s->gucNestLevel);
    AtEOSubXact_SPI(false, s->subTransactionId);
    AtEOSubXact_on_commit_actions(false, s->subTransactionId, s->parent->subTransactionId);
    AtEOSubXact_Namespace(false, s->subTransactionId, s->parent->subTransactionId);
    AtEOSubXact_Files(false, s->subTransactionId, s->parent->subTransactionId);
    AtEOSubXact_HashTables(false, s->nestingLevel);
    AtEOSubXact_PgStat(false, s->nestingLevel);
    AtSubAbort_Snapshot(s->nestingLevel);
    AtEOSubXact_ApplyLauncher(false, s->nestingLevel);
  }

     
                                                                           
                                                                           
                           
     
  XactReadOnly = s->prevXactReadOnly;

  RESUME_INTERRUPTS();
}

   
                         
   
                                                                          
                                                                
   
static void
CleanupSubTransaction(void)
{
  TransactionState s = CurrentTransactionState;

  ShowTransactionState("CleanupSubTransaction");

  if (s->state != TRANS_ABORT)
  {
    elog(WARNING, "CleanupSubTransaction while in %s state", TransStateAsString(s->state));
  }

  AtSubCleanup_Portals(s->subTransactionId);

  CurrentResourceOwner = s->parent->curTransactionOwner;
  CurTransactionResourceOwner = s->parent->curTransactionOwner;
  if (s->curTransactionOwner)
  {
    ResourceOwnerDelete(s->curTransactionOwner);
  }
  s->curTransactionOwner = NULL;

  AtSubCleanup_Memory();

  s->state = TRANS_DEFAULT;

  PopTransaction();
}

   
                   
                                                              
   
                                                                          
                                                                
   
static void
PushTransaction(void)
{
  TransactionState p = CurrentTransactionState;
  TransactionState s;

     
                                                                  
     
  s = (TransactionState)MemoryContextAllocZero(TopTransactionContext, sizeof(TransactionStateData));

     
                                                                      
     
  currentSubTransactionId += 1;
  if (currentSubTransactionId == InvalidSubTransactionId)
  {
    currentSubTransactionId -= 1;
    pfree(s);
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("cannot have more than 2^32-1 subtransactions in a transaction")));
  }

     
                                                                       
              
     
  s->fullTransactionId = InvalidFullTransactionId;                     
  s->subTransactionId = currentSubTransactionId;
  s->parent = p;
  s->nestingLevel = p->nestingLevel + 1;
  s->gucNestLevel = NewGUCNestLevel();
  s->savepointLevel = p->savepointLevel;
  s->state = TRANS_DEFAULT;
  s->blockState = TBLOCK_SUBBEGIN;
  GetUserIdAndSecContext(&s->prevUser, &s->prevSecContext);
  s->prevXactReadOnly = XactReadOnly;
  s->parallelModeLevel = 0;

  CurrentTransactionState = s;

     
                                                                           
                                                                             
                                                                           
             
     
}

   
                  
                                         
   
                                                                          
                                                                
   
static void
PopTransaction(void)
{
  TransactionState s = CurrentTransactionState;

  if (s->state != TRANS_DEFAULT)
  {
    elog(WARNING, "PopTransaction while in %s state", TransStateAsString(s->state));
  }

  if (s->parent == NULL)
  {
    elog(FATAL, "PopTransaction with no parent");
  }

  CurrentTransactionState = s->parent;

                                                          
  CurTransactionContext = s->parent->curTransactionContext;
  MemoryContextSwitchTo(CurTransactionContext);

                                     
  CurTransactionResourceOwner = s->parent->curTransactionOwner;
  CurrentResourceOwner = s->parent->curTransactionOwner;

                                    
  if (s->name)
  {
    pfree(s->name);
  }
  pfree(s);
}

   
                                 
                                                        
                                                                         
                                                                    
   
Size
EstimateTransactionStateSpace(void)
{
  TransactionState s;
  Size nxids = 0;
  Size size = SerializedTransactionStateHeaderSize;

  for (s = CurrentTransactionState; s != NULL; s = s->parent)
  {
    if (FullTransactionIdIsValid(s->fullTransactionId))
    {
      nxids = add_size(nxids, 1);
    }
    nxids = add_size(nxids, s->nChildXids);
  }

  return add_size(size, mul_size(sizeof(TransactionId), nxids));
}

   
                             
                                                                     
                                 
   
                                                                          
                                                                  
                                                                             
                                                                              
                                         
   
void
SerializeTransactionState(Size maxsize, char *start_address)
{
  TransactionState s;
  Size nxids = 0;
  Size i = 0;
  TransactionId *workspace;
  SerializedTransactionState *result;

  result = (SerializedTransactionState *)start_address;

  result->xactIsoLevel = XactIsoLevel;
  result->xactDeferrable = XactDeferrable;
  result->topFullTransactionId = XactTopFullTransactionId;
  result->currentFullTransactionId = CurrentTransactionState->fullTransactionId;
  result->currentCommandId = currentCommandId;

     
                                                                           
                                                                           
         
     
  if (nParallelCurrentXids > 0)
  {
    result->nParallelCurrentXids = nParallelCurrentXids;
    memcpy(&result->parallelCurrentXids[0], ParallelCurrentXids, nParallelCurrentXids * sizeof(TransactionId));
    return;
  }

     
                                                                           
                                                             
     
  for (s = CurrentTransactionState; s != NULL; s = s->parent)
  {
    if (FullTransactionIdIsValid(s->fullTransactionId))
    {
      nxids = add_size(nxids, 1);
    }
    nxids = add_size(nxids, s->nChildXids);
  }
  Assert(SerializedTransactionStateHeaderSize + nxids * sizeof(TransactionId) <= maxsize);

                                       
  workspace = palloc(nxids * sizeof(TransactionId));
  for (s = CurrentTransactionState; s != NULL; s = s->parent)
  {
    if (FullTransactionIdIsValid(s->fullTransactionId))
    {
      workspace[i++] = XidFromFullTransactionId(s->fullTransactionId);
    }
    if (s->nChildXids > 0)
    {
      memcpy(&workspace[i], s->childXids, s->nChildXids * sizeof(TransactionId));
    }
    i += s->nChildXids;
  }
  Assert(i == nxids);

                  
  qsort(workspace, nxids, sizeof(TransactionId), xidComparator);

                                   
  result->nParallelCurrentXids = nxids;
  memcpy(&result->parallelCurrentXids[0], workspace, nxids * sizeof(TransactionId));
}

   
                                  
                                                                
                                                               
   
void
StartParallelWorkerTransaction(char *tstatespace)
{
  SerializedTransactionState *tstate;

  Assert(CurrentTransactionState->blockState == TBLOCK_DEFAULT);
  StartTransaction();

  tstate = (SerializedTransactionState *)tstatespace;
  XactIsoLevel = tstate->xactIsoLevel;
  XactDeferrable = tstate->xactDeferrable;
  XactTopFullTransactionId = tstate->topFullTransactionId;
  CurrentTransactionState->fullTransactionId = tstate->currentFullTransactionId;
  currentCommandId = tstate->currentCommandId;
  nParallelCurrentXids = tstate->nParallelCurrentXids;
  ParallelCurrentXids = &tstate->parallelCurrentXids[0];

  CurrentTransactionState->blockState = TBLOCK_PARALLEL_INPROGRESS;
}

   
                                
                                       
   
void
EndParallelWorkerTransaction(void)
{
  Assert(CurrentTransactionState->blockState == TBLOCK_PARALLEL_INPROGRESS);
  CommitTransaction();
  CurrentTransactionState->blockState = TBLOCK_DEFAULT;
}

   
                        
                  
   
static void
ShowTransactionState(const char *str)
{
                                                           
  if (log_min_messages <= DEBUG5 || client_min_messages <= DEBUG5)
  {
    ShowTransactionStateRec(str, CurrentTransactionState);
  }
}

   
                           
                                                  
   
static void
ShowTransactionStateRec(const char *str, TransactionState s)
{
  StringInfoData buf;

  initStringInfo(&buf);

  if (s->nChildXids > 0)
  {
    int i;

    appendStringInfo(&buf, ", children: %u", s->childXids[0]);
    for (i = 1; i < s->nChildXids; i++)
    {
      appendStringInfo(&buf, " %u", s->childXids[i]);
    }
  }

  if (s->parent)
  {
    ShowTransactionStateRec(str, s->parent);
  }

                                                                      
  ereport(DEBUG5, (errmsg_internal("%s(%d) name: %s; blockState: %s; state: %s, xid/subid/cid: %u/%u/%u%s%s", str, s->nestingLevel, PointerIsValid(s->name) ? s->name : "unnamed", BlockStateAsString(s->blockState), TransStateAsString(s->state), (unsigned int)XidFromFullTransactionId(s->fullTransactionId), (unsigned int)s->subTransactionId, (unsigned int)currentCommandId, currentCommandIdUsed ? " (used)" : "", buf.data)));

  pfree(buf.data);
}

   
                      
                  
   
static const char *
BlockStateAsString(TBlockState blockState)
{
  switch (blockState)
  {
  case TBLOCK_DEFAULT:
    return "DEFAULT";
  case TBLOCK_STARTED:
    return "STARTED";
  case TBLOCK_BEGIN:
    return "BEGIN";
  case TBLOCK_INPROGRESS:
    return "INPROGRESS";
  case TBLOCK_IMPLICIT_INPROGRESS:
    return "IMPLICIT_INPROGRESS";
  case TBLOCK_PARALLEL_INPROGRESS:
    return "PARALLEL_INPROGRESS";
  case TBLOCK_END:
    return "END";
  case TBLOCK_ABORT:
    return "ABORT";
  case TBLOCK_ABORT_END:
    return "ABORT_END";
  case TBLOCK_ABORT_PENDING:
    return "ABORT_PENDING";
  case TBLOCK_PREPARE:
    return "PREPARE";
  case TBLOCK_SUBBEGIN:
    return "SUBBEGIN";
  case TBLOCK_SUBINPROGRESS:
    return "SUBINPROGRESS";
  case TBLOCK_SUBRELEASE:
    return "SUBRELEASE";
  case TBLOCK_SUBCOMMIT:
    return "SUBCOMMIT";
  case TBLOCK_SUBABORT:
    return "SUBABORT";
  case TBLOCK_SUBABORT_END:
    return "SUBABORT_END";
  case TBLOCK_SUBABORT_PENDING:
    return "SUBABORT_PENDING";
  case TBLOCK_SUBRESTART:
    return "SUBRESTART";
  case TBLOCK_SUBABORT_RESTART:
    return "SUBABORT_RESTART";
  }
  return "UNRECOGNIZED";
}

   
                      
                  
   
static const char *
TransStateAsString(TransState state)
{
  switch (state)
  {
  case TRANS_DEFAULT:
    return "DEFAULT";
  case TRANS_START:
    return "START";
  case TRANS_INPROGRESS:
    return "INPROGRESS";
  case TRANS_COMMIT:
    return "COMMIT";
  case TRANS_ABORT:
    return "ABORT";
  case TRANS_PREPARE:
    return "PREPARE";
  }
  return "UNRECOGNIZED";
}

   
                            
   
                                                                               
                                                                          
                                                                              
                                                                             
                                                  
   
int
xactGetCommittedChildren(TransactionId **ptr)
{
  TransactionState s = CurrentTransactionState;

  if (s->nChildXids == 0)
  {
    *ptr = NULL;
  }
  else
  {
    *ptr = s->childXids;
  }

  return s->nChildXids;
}

   
                         
   

   
                                                                     
   
                                                                        
              
   
XLogRecPtr
XactLogCommitRecord(TimestampTz commit_time, int nsubxacts, TransactionId *subxacts, int nrels, RelFileNode *rels, int nmsgs, SharedInvalidationMessage *msgs, bool relcacheInval, bool forceSync, int xactflags, TransactionId twophase_xid, const char *twophase_gid)
{
  xl_xact_commit xlrec;
  xl_xact_xinfo xl_xinfo;
  xl_xact_dbinfo xl_dbinfo;
  xl_xact_subxacts xl_subxacts;
  xl_xact_relfilenodes xl_relfilenodes;
  xl_xact_invals xl_invals;
  xl_xact_twophase xl_twophase;
  xl_xact_origin xl_origin;
  uint8 info;

  Assert(CritSectionCount > 0);

  xl_xinfo.xinfo = 0;

                                             
  if (!TransactionIdIsValid(twophase_xid))
  {
    info = XLOG_XACT_COMMIT;
  }
  else
  {
    info = XLOG_XACT_COMMIT_PREPARED;
  }

                                                               

  xlrec.xact_time = commit_time;

  if (relcacheInval)
  {
    xl_xinfo.xinfo |= XACT_COMPLETION_UPDATE_RELCACHE_FILE;
  }
  if (forceSyncCommit)
  {
    xl_xinfo.xinfo |= XACT_COMPLETION_FORCE_SYNC_COMMIT;
  }
  if ((xactflags & XACT_FLAGS_ACQUIREDACCESSEXCLUSIVELOCK))
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_AE_LOCKS;
  }

     
                                                                           
                                  
     
  if (synchronous_commit >= SYNCHRONOUS_COMMIT_REMOTE_APPLY)
  {
    xl_xinfo.xinfo |= XACT_COMPLETION_APPLY_FEEDBACK;
  }

     
                                                                            
                                   
     
  if (nmsgs > 0 || XLogLogicalInfoActive())
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_DBINFO;
    xl_dbinfo.dbId = MyDatabaseId;
    xl_dbinfo.tsId = MyDatabaseTableSpace;
  }

  if (nsubxacts > 0)
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_SUBXACTS;
    xl_subxacts.nsubxacts = nsubxacts;
  }

  if (nrels > 0)
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_RELFILENODES;
    xl_relfilenodes.nrels = nrels;
  }

  if (nmsgs > 0)
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_INVALS;
    xl_invals.nmsgs = nmsgs;
  }

  if (TransactionIdIsValid(twophase_xid))
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_TWOPHASE;
    xl_twophase.xid = twophase_xid;
    Assert(twophase_gid != NULL);

    if (XLogLogicalInfoActive())
    {
      xl_xinfo.xinfo |= XACT_XINFO_HAS_GID;
    }
  }

                                           
  if (replorigin_session_origin != InvalidRepOriginId)
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_ORIGIN;

    xl_origin.origin_lsn = replorigin_session_origin_lsn;
    xl_origin.origin_timestamp = replorigin_session_origin_timestamp;
  }

  if (xl_xinfo.xinfo != 0)
  {
    info |= XLOG_XACT_HAS_INFO;
  }

                                                                   

  XLogBeginInsert();

  XLogRegisterData((char *)(&xlrec), sizeof(xl_xact_commit));

  if (xl_xinfo.xinfo != 0)
  {
    XLogRegisterData((char *)(&xl_xinfo.xinfo), sizeof(xl_xinfo.xinfo));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_DBINFO)
  {
    XLogRegisterData((char *)(&xl_dbinfo), sizeof(xl_dbinfo));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_SUBXACTS)
  {
    XLogRegisterData((char *)(&xl_subxacts), MinSizeOfXactSubxacts);
    XLogRegisterData((char *)subxacts, nsubxacts * sizeof(TransactionId));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_RELFILENODES)
  {
    XLogRegisterData((char *)(&xl_relfilenodes), MinSizeOfXactRelfilenodes);
    XLogRegisterData((char *)rels, nrels * sizeof(RelFileNode));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_INVALS)
  {
    XLogRegisterData((char *)(&xl_invals), MinSizeOfXactInvals);
    XLogRegisterData((char *)msgs, nmsgs * sizeof(SharedInvalidationMessage));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_TWOPHASE)
  {
    XLogRegisterData((char *)(&xl_twophase), sizeof(xl_xact_twophase));
    if (xl_xinfo.xinfo & XACT_XINFO_HAS_GID)
    {
      XLogRegisterData(unconstify(char *, twophase_gid), strlen(twophase_gid) + 1);
    }
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_ORIGIN)
  {
    XLogRegisterData((char *)(&xl_origin), sizeof(xl_xact_origin));
  }

                                   
  XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

  return XLogInsert(RM_XACT_ID, info);
}

   
                                                                    
   
                                                                       
              
   
XLogRecPtr
XactLogAbortRecord(TimestampTz abort_time, int nsubxacts, TransactionId *subxacts, int nrels, RelFileNode *rels, int xactflags, TransactionId twophase_xid, const char *twophase_gid)
{
  xl_xact_abort xlrec;
  xl_xact_xinfo xl_xinfo;
  xl_xact_subxacts xl_subxacts;
  xl_xact_relfilenodes xl_relfilenodes;
  xl_xact_twophase xl_twophase;
  xl_xact_dbinfo xl_dbinfo;
  xl_xact_origin xl_origin;

  uint8 info;

  Assert(CritSectionCount > 0);

  xl_xinfo.xinfo = 0;

                                            
  if (!TransactionIdIsValid(twophase_xid))
  {
    info = XLOG_XACT_ABORT;
  }
  else
  {
    info = XLOG_XACT_ABORT_PREPARED;
  }

                                                               

  xlrec.xact_time = abort_time;

  if ((xactflags & XACT_FLAGS_ACQUIREDACCESSEXCLUSIVELOCK))
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_AE_LOCKS;
  }

  if (nsubxacts > 0)
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_SUBXACTS;
    xl_subxacts.nsubxacts = nsubxacts;
  }

  if (nrels > 0)
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_RELFILENODES;
    xl_relfilenodes.nrels = nrels;
  }

  if (TransactionIdIsValid(twophase_xid))
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_TWOPHASE;
    xl_twophase.xid = twophase_xid;
    Assert(twophase_gid != NULL);

    if (XLogLogicalInfoActive())
    {
      xl_xinfo.xinfo |= XACT_XINFO_HAS_GID;
    }
  }

  if (TransactionIdIsValid(twophase_xid) && XLogLogicalInfoActive())
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_DBINFO;
    xl_dbinfo.dbId = MyDatabaseId;
    xl_dbinfo.tsId = MyDatabaseTableSpace;
  }

                                                                   
  if ((replorigin_session_origin != InvalidRepOriginId) && TransactionIdIsValid(twophase_xid) && XLogLogicalInfoActive())
  {
    xl_xinfo.xinfo |= XACT_XINFO_HAS_ORIGIN;

    xl_origin.origin_lsn = replorigin_session_origin_lsn;
    xl_origin.origin_timestamp = replorigin_session_origin_timestamp;
  }

  if (xl_xinfo.xinfo != 0)
  {
    info |= XLOG_XACT_HAS_INFO;
  }

                                                                  

  XLogBeginInsert();

  XLogRegisterData((char *)(&xlrec), MinSizeOfXactAbort);

  if (xl_xinfo.xinfo != 0)
  {
    XLogRegisterData((char *)(&xl_xinfo), sizeof(xl_xinfo));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_DBINFO)
  {
    XLogRegisterData((char *)(&xl_dbinfo), sizeof(xl_dbinfo));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_SUBXACTS)
  {
    XLogRegisterData((char *)(&xl_subxacts), MinSizeOfXactSubxacts);
    XLogRegisterData((char *)subxacts, nsubxacts * sizeof(TransactionId));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_RELFILENODES)
  {
    XLogRegisterData((char *)(&xl_relfilenodes), MinSizeOfXactRelfilenodes);
    XLogRegisterData((char *)rels, nrels * sizeof(RelFileNode));
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_TWOPHASE)
  {
    XLogRegisterData((char *)(&xl_twophase), sizeof(xl_xact_twophase));
    if (xl_xinfo.xinfo & XACT_XINFO_HAS_GID)
    {
      XLogRegisterData(unconstify(char *, twophase_gid), strlen(twophase_gid) + 1);
    }
  }

  if (xl_xinfo.xinfo & XACT_XINFO_HAS_ORIGIN)
  {
    XLogRegisterData((char *)(&xl_origin), sizeof(xl_xact_origin));
  }

  if (TransactionIdIsValid(twophase_xid))
  {
    XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);
  }

  return XLogInsert(RM_XACT_ID, info);
}

   
                                                                         
                                                         
   
static void
xact_redo_commit(xl_xact_parsed_commit *parsed, TransactionId xid, XLogRecPtr lsn, RepOriginId origin_id)
{
  TransactionId max_xid;
  TimestampTz commit_time;

  Assert(TransactionIdIsValid(xid));

  max_xid = TransactionIdLatest(xid, parsed->nsubxacts, parsed->subxacts);

                                                                        
  AdvanceNextFullTransactionIdPastXid(max_xid);

  Assert(((parsed->xinfo & XACT_XINFO_HAS_ORIGIN) == 0) == (origin_id == InvalidRepOriginId));

  if (parsed->xinfo & XACT_XINFO_HAS_ORIGIN)
  {
    commit_time = parsed->origin_timestamp;
  }
  else
  {
    commit_time = parsed->xact_time;
  }

                                                         
  TransactionTreeSetCommitTsData(xid, parsed->nsubxacts, parsed->subxacts, commit_time, origin_id, false);

  if (standbyState == STANDBY_DISABLED)
  {
       
                                                  
       
    TransactionIdCommitTree(xid, parsed->nsubxacts, parsed->subxacts);
  }
  else
  {
       
                                                                  
                                                                     
                                                                         
                                                                           
                                                                       
                                                                         
                             
       
    RecordKnownAssignedTransactionIds(max_xid);

       
                                                                      
                                                                   
                                                                        
                                                                       
                                                                           
                                                                 
                                                          
       
    TransactionIdAsyncCommitTree(xid, parsed->nsubxacts, parsed->subxacts, lsn);

       
                                                         
       
    ExpireTreeKnownAssignedTransactionIds(xid, parsed->nsubxacts, parsed->subxacts, max_xid);

       
                                                                    
                                                                     
                                      
       
    ProcessCommittedInvalidationMessages(parsed->msgs, parsed->nmsgs, XactCompletionRelcacheInitFileInval(parsed->xinfo), parsed->dbId, parsed->tsId);

       
                                                                           
                                                                           
                                            
       
    if (parsed->xinfo & XACT_XINFO_HAS_AE_LOCKS)
    {
      StandbyReleaseLockTree(xid, parsed->nsubxacts, parsed->subxacts);
    }
  }

  if (parsed->xinfo & XACT_XINFO_HAS_ORIGIN)
  {
                                
    replorigin_advance(origin_id, parsed->origin_lsn, lsn, false               , false /* WAL */);
  }

                                                          
  if (parsed->nrels > 0)
  {
       
                                                                          
                                                                        
                                                                         
                                                                       
                                                                        
                                                                          
       
                                                                           
                                                                           
                                                                        
                                                                          
                                                                     
                                         
       
    XLogFlush(lsn);

                                                            
    DropRelationFiles(parsed->xnodes, parsed->nrels, true);
  }

     
                                                                           
                                                                             
                                                                        
                                                                           
                                                                          
                                                                        
                                                                         
                                                                
                                                                            
                                                    
     
  if (XactCompletionForceSyncCommit(parsed->xinfo))
  {
    XLogFlush(lsn);
  }

     
                                                                           
                                                                             
                  
     
  if (XactCompletionApplyFeedback(parsed->xinfo))
  {
    XLogRequestWalReceiverReply();
  }
}

   
                                                                       
                                                           
   
                                                                         
                                                                  
                                                                       
                                                      
   
static void
xact_redo_abort(xl_xact_parsed_abort *parsed, TransactionId xid, XLogRecPtr lsn)
{
  TransactionId max_xid;

  Assert(TransactionIdIsValid(xid));

                                                                        
  max_xid = TransactionIdLatest(xid, parsed->nsubxacts, parsed->subxacts);
  AdvanceNextFullTransactionIdPastXid(max_xid);

  if (standbyState == STANDBY_DISABLED)
  {
                                                                          
    TransactionIdAbortTree(xid, parsed->nsubxacts, parsed->subxacts);
  }
  else
  {
       
                                                                  
                                                                     
                                                                         
                                                                           
                                                                       
                                                                         
                             
       
    RecordKnownAssignedTransactionIds(max_xid);

                                                                          
    TransactionIdAbortTree(xid, parsed->nsubxacts, parsed->subxacts);

       
                                                               
       
    ExpireTreeKnownAssignedTransactionIds(xid, parsed->nsubxacts, parsed->subxacts, max_xid);

       
                                                           
       

       
                                                                  
       
    if (parsed->xinfo & XACT_XINFO_HAS_AE_LOCKS)
    {
      StandbyReleaseLockTree(xid, parsed->nsubxacts, parsed->subxacts);
    }
  }

                                                          
  if (parsed->nrels > 0)
  {
       
                                                                          
                              
       
    XLogFlush(lsn);

    DropRelationFiles(parsed->xnodes, parsed->nrels, true);
  }
}

void
xact_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & XLOG_XACT_OPMASK;

                                                  
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == XLOG_XACT_COMMIT)
  {
    xl_xact_commit *xlrec = (xl_xact_commit *)XLogRecGetData(record);
    xl_xact_parsed_commit parsed;

    ParseCommitRecord(XLogRecGetInfo(record), xlrec, &parsed);
    xact_redo_commit(&parsed, XLogRecGetXid(record), record->EndRecPtr, XLogRecGetOrigin(record));
  }
  else if (info == XLOG_XACT_COMMIT_PREPARED)
  {
    xl_xact_commit *xlrec = (xl_xact_commit *)XLogRecGetData(record);
    xl_xact_parsed_commit parsed;

    ParseCommitRecord(XLogRecGetInfo(record), xlrec, &parsed);
    xact_redo_commit(&parsed, parsed.twophase_xid, record->EndRecPtr, XLogRecGetOrigin(record));

                                                           
    LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
    PrepareRedoRemove(parsed.twophase_xid, false);
    LWLockRelease(TwoPhaseStateLock);
  }
  else if (info == XLOG_XACT_ABORT)
  {
    xl_xact_abort *xlrec = (xl_xact_abort *)XLogRecGetData(record);
    xl_xact_parsed_abort parsed;

    ParseAbortRecord(XLogRecGetInfo(record), xlrec, &parsed);
    xact_redo_abort(&parsed, XLogRecGetXid(record), record->EndRecPtr);
  }
  else if (info == XLOG_XACT_ABORT_PREPARED)
  {
    xl_xact_abort *xlrec = (xl_xact_abort *)XLogRecGetData(record);
    xl_xact_parsed_abort parsed;

    ParseAbortRecord(XLogRecGetInfo(record), xlrec, &parsed);
    xact_redo_abort(&parsed, parsed.twophase_xid, record->EndRecPtr);

                                                           
    LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
    PrepareRedoRemove(parsed.twophase_xid, false);
    LWLockRelease(TwoPhaseStateLock);
  }
  else if (info == XLOG_XACT_PREPARE)
  {
       
                                                                           
                    
       
    LWLockAcquire(TwoPhaseStateLock, LW_EXCLUSIVE);
    PrepareRedoAdd(XLogRecGetData(record), record->ReadRecPtr, record->EndRecPtr, XLogRecGetOrigin(record));
    LWLockRelease(TwoPhaseStateLock);
  }
  else if (info == XLOG_XACT_ASSIGNMENT)
  {
    xl_xact_assignment *xlrec = (xl_xact_assignment *)XLogRecGetData(record);

    if (standbyState >= STANDBY_INITIALIZED)
    {
      ProcArrayApplyXidAssignment(xlrec->xtop, xlrec->nsubxacts, xlrec->xsub);
    }
  }
  else
  {
    elog(PANIC, "xact_redo: unknown op code %u", info);
  }
}
