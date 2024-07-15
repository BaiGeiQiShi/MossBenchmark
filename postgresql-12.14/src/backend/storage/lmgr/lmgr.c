                                                                            
   
          
                                
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "commands/progress.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/lmgr.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "utils/inval.h"

   
                                                                    
   
                                                                       
                                                                              
                                                                             
                                                                               
                                                                            
                                                                         
                                                                           
                                                                         
                                                                           
                                                                             
                              
   
static uint32 speculativeInsertionToken = 0;

   
                                                           
   
                                                                               
                                                                 
   
typedef struct XactLockTableWaitInfo
{
  XLTW_Oper oper;
  Relation rel;
  ItemPointer ctid;
} XactLockTableWaitInfo;

static void
XactLockTableWaitErrorCb(void *arg);

   
                        
                                                               
   
                                                              
   
void
RelationInitLockInfo(Relation relation)
{
  Assert(RelationIsValid(relation));
  Assert(OidIsValid(RelationGetRelid(relation)));

  relation->rd_lockInfo.lockRelId.relId = RelationGetRelid(relation);

  if (relation->rd_rel->relisshared)
  {
    relation->rd_lockInfo.lockRelId.dbId = InvalidOid;
  }
  else
  {
    relation->rd_lockInfo.lockRelId.dbId = MyDatabaseId;
  }
}

   
                         
                                                             
   
static inline void
SetLocktagRelationOid(LOCKTAG *tag, Oid relid)
{
  Oid dbid;

  if (IsSharedRelation(relid))
  {
    dbid = InvalidOid;
  }
  else
  {
    dbid = MyDatabaseId;
  }

  SET_LOCKTAG_RELATION(*tag, dbid, relid);
}

   
                    
   
                                                                      
                                                            
   
void
LockRelationOid(Oid relid, LOCKMODE lockmode)
{
  LOCKTAG tag;
  LOCALLOCK *locallock;
  LockAcquireResult res;

  SetLocktagRelationOid(&tag, relid);

  res = LockAcquireExtended(&tag, lockmode, false, false, true, &locallock);

     
                                                                            
                                                                            
                                                                         
                                                                             
                                                                     
                                                                            
                                                       
                                         
     
                                                                           
                                                                        
                                                                          
                                                                      
                                        
     
  if (res != LOCKACQUIRE_ALREADY_CLEAR)
  {
    AcceptInvalidationMessages();
    MarkLockClear(locallock);
  }
}

   
                               
   
                                                                    
                                           
   
                                                                  
                                                                            
   
bool
ConditionalLockRelationOid(Oid relid, LOCKMODE lockmode)
{
  LOCKTAG tag;
  LOCALLOCK *locallock;
  LockAcquireResult res;

  SetLocktagRelationOid(&tag, relid);

  res = LockAcquireExtended(&tag, lockmode, false, true, true, &locallock);

  if (res == LOCKACQUIRE_NOT_AVAIL)
  {
    return false;
  }

     
                                                                           
                         
     
  if (res != LOCKACQUIRE_ALREADY_CLEAR)
  {
    AcceptInvalidationMessages();
    MarkLockClear(locallock);
  }

  return true;
}

   
                     
   
                                                                        
                      
   
void
UnlockRelationId(LockRelId *relid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION(tag, relid->dbId, relid->relId);

  LockRelease(&tag, lockmode, false);
}

   
                      
   
                                                                        
   
void
UnlockRelationOid(Oid relid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SetLocktagRelationOid(&tag, relid);

  LockRelease(&tag, lockmode, false);
}

   
                 
   
                                                                        
                                                                        
                            
   
void
LockRelation(Relation relation, LOCKMODE lockmode)
{
  LOCKTAG tag;
  LOCALLOCK *locallock;
  LockAcquireResult res;

  SET_LOCKTAG_RELATION(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  res = LockAcquireExtended(&tag, lockmode, false, false, true, &locallock);

     
                                                                           
                         
     
  if (res != LOCKACQUIRE_ALREADY_CLEAR)
  {
    AcceptInvalidationMessages();
    MarkLockClear(locallock);
  }
}

   
                            
   
                                                                        
                                                                        
                            
   
bool
ConditionalLockRelation(Relation relation, LOCKMODE lockmode)
{
  LOCKTAG tag;
  LOCALLOCK *locallock;
  LockAcquireResult res;

  SET_LOCKTAG_RELATION(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  res = LockAcquireExtended(&tag, lockmode, false, true, true, &locallock);

  if (res == LOCKACQUIRE_NOT_AVAIL)
  {
    return false;
  }

     
                                                                           
                         
     
  if (res != LOCKACQUIRE_ALREADY_CLEAR)
  {
    AcceptInvalidationMessages();
    MarkLockClear(locallock);
  }

  return true;
}

   
                   
   
                                                                       
               
   
void
UnlockRelation(Relation relation, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  LockRelease(&tag, lockmode, false);
}

   
                            
   
                                                                          
                                                                         
                                                                  
                                                                     
   
bool
CheckRelationLockedByMe(Relation relation, LOCKMODE lockmode, bool orstronger)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  if (LockHeldByMe(&tag, lockmode))
  {
    return true;
  }

  if (orstronger)
  {
    LOCKMODE slockmode;

    for (slockmode = lockmode + 1; slockmode <= MaxLockMode; slockmode++)
    {
      if (LockHeldByMe(&tag, slockmode))
      {
#ifdef NOT_USED
                                                                   
        elog(WARNING, "lock mode %s substituted for %s on relation %s", GetLockmodeName(tag.locktag_lockmethodid, slockmode), GetLockmodeName(tag.locktag_lockmethodid, lockmode), RelationGetRelationName(relation));
#endif
        return true;
      }
    }
  }

  return false;
}

   
                           
   
                                                                     
                                        
   
bool
LockHasWaitersRelation(Relation relation, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  return LockHasWaiters(&tag, lockmode, false);
}

   
                             
   
                                                                        
                                                                            
                                                                                
                            
   
                                                                      
                                                                     
                                 
   
void
LockRelationIdForSession(LockRelId *relid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION(tag, relid->dbId, relid->relId);

  (void)LockAcquire(&tag, lockmode, true, false);
}

   
                               
   
void
UnlockRelationIdForSession(LockRelId *relid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION(tag, relid->dbId, relid->relId);

  LockRelease(&tag, lockmode, true);
}

   
                             
   
                                                                      
                                                                       
                         
   
                                                                        
                                                                       
   
void
LockRelationForExtension(Relation relation, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION_EXTEND(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  (void)LockAcquire(&tag, lockmode, false, false);
}

   
                                        
   
                                                                    
                                           
   
bool
ConditionalLockRelationForExtension(Relation relation, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION_EXTEND(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  return (LockAcquire(&tag, lockmode, false, true) != LOCKACQUIRE_NOT_AVAIL);
}

   
                                     
   
                                                                                
   
int
RelationExtensionLockWaiterCount(Relation relation)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION_EXTEND(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  return LockWaiterCount(&tag);
}

   
                               
   
void
UnlockRelationForExtension(Relation relation, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_RELATION_EXTEND(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId);

  LockRelease(&tag, lockmode, false);
}

   
                          
   
                                                                              
   
void
LockDatabaseFrozenIds(LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_DATABASE_FROZEN_IDS(tag, MyDatabaseId);

  (void)LockAcquire(&tag, lockmode, false, false);
}

   
             
   
                                                                          
                                           
   
void
LockPage(Relation relation, BlockNumber blkno, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_PAGE(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId, blkno);

  (void)LockAcquire(&tag, lockmode, false, false);
}

   
                        
   
                                                                    
                                           
   
bool
ConditionalLockPage(Relation relation, BlockNumber blkno, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_PAGE(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId, blkno);

  return (LockAcquire(&tag, lockmode, false, true) != LOCKACQUIRE_NOT_AVAIL);
}

   
               
   
void
UnlockPage(Relation relation, BlockNumber blkno, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_PAGE(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId, blkno);

  LockRelease(&tag, lockmode, false);
}

   
              
   
                                                                             
                                                                              
                                                  
   
void
LockTuple(Relation relation, ItemPointer tid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_TUPLE(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId, ItemPointerGetBlockNumber(tid), ItemPointerGetOffsetNumber(tid));

  (void)LockAcquire(&tag, lockmode, false, false);
}

   
                         
   
                                                                    
                                           
   
bool
ConditionalLockTuple(Relation relation, ItemPointer tid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_TUPLE(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId, ItemPointerGetBlockNumber(tid), ItemPointerGetOffsetNumber(tid));

  return (LockAcquire(&tag, lockmode, false, true) != LOCKACQUIRE_NOT_AVAIL);
}

   
                
   
void
UnlockTuple(Relation relation, ItemPointer tid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_TUPLE(tag, relation->rd_lockInfo.lockRelId.dbId, relation->rd_lockInfo.lockRelId.relId, ItemPointerGetBlockNumber(tid), ItemPointerGetOffsetNumber(tid));

  LockRelease(&tag, lockmode, false);
}

   
                        
   
                                                                      
                                                                            
                                                                    
   
void
XactLockTableInsert(TransactionId xid)
{
  LOCKTAG tag;

  SET_LOCKTAG_TRANSACTION(tag, xid);

  (void)LockAcquire(&tag, ExclusiveLock, false, false);
}

   
                        
   
                                                                     
                                                                      
                                                                                
   
void
XactLockTableDelete(TransactionId xid)
{
  LOCKTAG tag;

  SET_LOCKTAG_TRANSACTION(tag, xid);

  LockRelease(&tag, ExclusiveLock, false);
}

   
                      
   
                                                                           
                                                                              
                                              
   
                                                                            
                                                                                
                                                                             
                                                                      
                                                                                
                                  
   
void
XactLockTableWait(TransactionId xid, Relation rel, ItemPointer ctid, XLTW_Oper oper)
{
  LOCKTAG tag;
  XactLockTableWaitInfo info;
  ErrorContextCallback callback;
  bool first = true;

     
                                                                    
               
     
  if (oper != XLTW_None)
  {
    Assert(RelationIsValid(rel));
    Assert(ItemPointerIsValid(ctid));

    info.rel = rel;
    info.ctid = ctid;
    info.oper = oper;

    callback.callback = XactLockTableWaitErrorCb;
    callback.arg = &info;
    callback.previous = error_context_stack;
    error_context_stack = &callback;
  }

  for (;;)
  {
    Assert(TransactionIdIsValid(xid));
    Assert(!TransactionIdEquals(xid, GetTopTransactionIdIfAny()));

    SET_LOCKTAG_TRANSACTION(tag, xid);

    (void)LockAcquire(&tag, ShareLock, false, false);

    LockRelease(&tag, ShareLock, false);

    if (!TransactionIdIsInProgress(xid))
    {
      break;
    }

       
                                                                         
                                                                           
                                                                         
                                                                           
                                                                          
                                      
       
                                                                         
                                                                           
                                                                        
                                                                         
                                                                         
                                                        
       
    if (!first)
    {
      pg_usleep(1000L);
    }
    first = false;
    xid = SubTransGetTopmostTransaction(xid);
  }

  if (oper != XLTW_None)
  {
    error_context_stack = callback.previous;
  }
}

   
                                 
   
                                                                    
                                          
   
bool
ConditionalXactLockTableWait(TransactionId xid)
{
  LOCKTAG tag;
  bool first = true;

  for (;;)
  {
    Assert(TransactionIdIsValid(xid));
    Assert(!TransactionIdEquals(xid, GetTopTransactionIdIfAny()));

    SET_LOCKTAG_TRANSACTION(tag, xid);

    if (LockAcquire(&tag, ShareLock, false, true) == LOCKACQUIRE_NOT_AVAIL)
    {
      return false;
    }

    LockRelease(&tag, ShareLock, false);

    if (!TransactionIdIsInProgress(xid))
    {
      break;
    }

                                               
    if (!first)
    {
      pg_usleep(1000L);
    }
    first = false;
    xid = SubTransGetTopmostTransaction(xid);
  }

  return true;
}

   
                                    
   
                                                                             
                                                                               
                                                                             
       
   
                                                                    
                                           
   
uint32
SpeculativeInsertionLockAcquire(TransactionId xid)
{
  LOCKTAG tag;

  speculativeInsertionToken++;

     
                                                                            
     
  if (speculativeInsertionToken == 0)
  {
    speculativeInsertionToken = 1;
  }

  SET_LOCKTAG_SPECULATIVE_INSERTION(tag, xid, speculativeInsertionToken);

  (void)LockAcquire(&tag, ExclusiveLock, false, false);

  return speculativeInsertionToken;
}

   
                                    
   
                                                                       
                      
   
void
SpeculativeInsertionLockRelease(TransactionId xid)
{
  LOCKTAG tag;

  SET_LOCKTAG_SPECULATIVE_INSERTION(tag, xid, speculativeInsertionToken);

  LockRelease(&tag, ExclusiveLock, false);
}

   
                             
   
                                                                            
          
   
void
SpeculativeInsertionWait(TransactionId xid, uint32 token)
{
  LOCKTAG tag;

  SET_LOCKTAG_SPECULATIVE_INSERTION(tag, xid, token);

  Assert(TransactionIdIsValid(xid));
  Assert(token != 0);

  (void)LockAcquire(&tag, ShareLock, false, false);
  LockRelease(&tag, ShareLock, false);
}

   
                                   
                                                       
   
static void
XactLockTableWaitErrorCb(void *arg)
{
  XactLockTableWaitInfo *info = (XactLockTableWaitInfo *)arg;

     
                                                                      
                      
     
  if (info->oper != XLTW_None && ItemPointerIsValid(info->ctid) && RelationIsValid(info->rel))
  {
    const char *cxt;

    switch (info->oper)
    {
    case XLTW_Update:
      cxt = gettext_noop("while updating tuple (%u,%u) in relation \"%s\"");
      break;
    case XLTW_Delete:
      cxt = gettext_noop("while deleting tuple (%u,%u) in relation \"%s\"");
      break;
    case XLTW_Lock:
      cxt = gettext_noop("while locking tuple (%u,%u) in relation \"%s\"");
      break;
    case XLTW_LockUpdated:
      cxt = gettext_noop("while locking updated version (%u,%u) of tuple in relation \"%s\"");
      break;
    case XLTW_InsertIndex:
      cxt = gettext_noop("while inserting index tuple (%u,%u) in relation \"%s\"");
      break;
    case XLTW_InsertIndexUnique:
      cxt = gettext_noop("while checking uniqueness of tuple (%u,%u) in relation \"%s\"");
      break;
    case XLTW_FetchUpdated:
      cxt = gettext_noop("while rechecking updated tuple (%u,%u) in relation \"%s\"");
      break;
    case XLTW_RecheckExclusionConstr:
      cxt = gettext_noop("while checking exclusion constraint on tuple (%u,%u) in relation \"%s\"");
      break;

    default:
      return;
    }

    errcontext(cxt, ItemPointerGetBlockNumber(info->ctid), ItemPointerGetOffsetNumber(info->ctid), RelationGetRelationName(info->rel));
  }
}

   
                          
                                                                       
                                    
   
                                                                           
                            
   
                                                                          
                                                                              
                                                                             
                  
   
void
WaitForLockersMultiple(List *locktags, LOCKMODE lockmode, bool progress)
{
  List *holders = NIL;
  ListCell *lc;
  int total = 0;
  int done = 0;

                                    
  if (list_length(locktags) == 0)
  {
    return;
  }

                                                   
  foreach (lc, locktags)
  {
    LOCKTAG *locktag = lfirst(lc);
    int count;

    holders = lappend(holders, GetLockConflicts(locktag, lockmode, progress ? &count : NULL));
    if (progress)
    {
      total += count;
    }
  }

  if (progress)
  {
    pgstat_progress_update_param(PROGRESS_WAITFOR_TOTAL, total);
  }

     
                                                                           
                                                                     
     

                                                          
  foreach (lc, holders)
  {
    VirtualTransactionId *lockholders = lfirst(lc);

    while (VirtualTransactionIdIsValid(*lockholders))
    {
                                                              
      if (progress)
      {
        PGPROC *holder = BackendIdGetProc(lockholders->backendId);

        if (holder)
        {
          pgstat_progress_update_param(PROGRESS_WAITFOR_CURRENT_PID, holder->pid);
        }
      }
      VirtualXactLock(*lockholders, true);
      lockholders++;

      if (progress)
      {
        pgstat_progress_update_param(PROGRESS_WAITFOR_DONE, ++done);
      }
    }
  }
  if (progress)
  {
    const int index[] = {PROGRESS_WAITFOR_TOTAL, PROGRESS_WAITFOR_DONE, PROGRESS_WAITFOR_CURRENT_PID};
    const int64 values[] = {0, 0, 0};

    pgstat_progress_update_multi_param(3, index, values);
  }

  list_free_deep(holders);
}

   
                  
   
                                                          
   
void
WaitForLockers(LOCKTAG heaplocktag, LOCKMODE lockmode, bool progress)
{
  List *l;

  l = list_make1(&heaplocktag);
  WaitForLockersMultiple(l, lockmode, progress);
  list_free(l);
}

   
                       
   
                                                                         
                                                                           
                                                                          
                                             
   
void
LockDatabaseObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_OBJECT(tag, MyDatabaseId, classid, objid, objsubid);

  (void)LockAcquire(&tag, lockmode, false, false);

                                                                         
  AcceptInvalidationMessages();
}

   
                         
   
void
UnlockDatabaseObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_OBJECT(tag, MyDatabaseId, classid, objid, objsubid);

  LockRelease(&tag, lockmode, false);
}

   
                     
   
                                                      
   
void
LockSharedObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_OBJECT(tag, InvalidOid, classid, objid, objsubid);

  (void)LockAcquire(&tag, lockmode, false, false);

                                                                         
  AcceptInvalidationMessages();
}

   
                       
   
void
UnlockSharedObject(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_OBJECT(tag, InvalidOid, classid, objid, objsubid);

  LockRelease(&tag, lockmode, false);
}

   
                               
   
                                                                    
                                                                     
   
void
LockSharedObjectForSession(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_OBJECT(tag, InvalidOid, classid, objid, objsubid);

  (void)LockAcquire(&tag, lockmode, true, false);
}

   
                                 
   
void
UnlockSharedObjectForSession(Oid classid, Oid objid, uint16 objsubid, LOCKMODE lockmode)
{
  LOCKTAG tag;

  SET_LOCKTAG_OBJECT(tag, InvalidOid, classid, objid, objsubid);

  LockRelease(&tag, lockmode, true);
}

   
                                                     
   
                                                                          
                                                                            
                                                 
   
void
DescribeLockTag(StringInfo buf, const LOCKTAG *tag)
{
  switch ((LockTagType)tag->locktag_type)
  {
  case LOCKTAG_RELATION:
    appendStringInfo(buf, _("relation %u of database %u"), tag->locktag_field2, tag->locktag_field1);
    break;
  case LOCKTAG_RELATION_EXTEND:
    appendStringInfo(buf, _("extension of relation %u of database %u"), tag->locktag_field2, tag->locktag_field1);
    break;
  case LOCKTAG_DATABASE_FROZEN_IDS:
    appendStringInfo(buf, _("pg_database.datfrozenxid of database %u"), tag->locktag_field1);
    break;
  case LOCKTAG_PAGE:
    appendStringInfo(buf, _("page %u of relation %u of database %u"), tag->locktag_field3, tag->locktag_field2, tag->locktag_field1);
    break;
  case LOCKTAG_TUPLE:
    appendStringInfo(buf, _("tuple (%u,%u) of relation %u of database %u"), tag->locktag_field3, tag->locktag_field4, tag->locktag_field2, tag->locktag_field1);
    break;
  case LOCKTAG_TRANSACTION:
    appendStringInfo(buf, _("transaction %u"), tag->locktag_field1);
    break;
  case LOCKTAG_VIRTUALTRANSACTION:
    appendStringInfo(buf, _("virtual transaction %d/%u"), tag->locktag_field1, tag->locktag_field2);
    break;
  case LOCKTAG_SPECULATIVE_TOKEN:
    appendStringInfo(buf, _("speculative token %u of transaction %u"), tag->locktag_field2, tag->locktag_field1);
    break;
  case LOCKTAG_OBJECT:
    appendStringInfo(buf, _("object %u of class %u of database %u"), tag->locktag_field3, tag->locktag_field2, tag->locktag_field1);
    break;
  case LOCKTAG_USERLOCK:
                                                         
    appendStringInfo(buf, _("user lock [%u,%u,%u]"), tag->locktag_field1, tag->locktag_field2, tag->locktag_field3);
    break;
  case LOCKTAG_ADVISORY:
    appendStringInfo(buf, _("advisory lock [%u,%u,%u,%u]"), tag->locktag_field1, tag->locktag_field2, tag->locktag_field3, tag->locktag_field4);
    break;
  default:
    appendStringInfo(buf, _("unrecognized locktag type %d"), (int)tag->locktag_type);
    break;
  }
}

   
                          
   
                                                           
   
const char *
GetLockNameFromTagType(uint16 locktag_type)
{
  if (locktag_type > LOCKTAG_LAST_TYPE)
  {
    return "???";
  }
  return LockTagTypeNames[locktag_type];
}
