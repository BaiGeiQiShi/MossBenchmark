                                                                            
   
             
                                                          
   
                                                                         
                                                                        
   
   
                  
                                          
   
         
                                                                      
                         
   
                                                                            
   

#include "postgres.h"

#include "access/clog.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "utils/snapmgr.h"

   
                                                                            
                                                                             
                                                                         
                      
   
static TransactionId cachedFetchXid = InvalidTransactionId;
static XidStatus cachedFetchXidStatus;
static XLogRecPtr cachedCommitLSN;

                     
static XidStatus
TransactionLogFetch(TransactionId transactionId);

                                                                    
                                         
   
                        
                                                                    
   

   
                                                                           
   
static XidStatus
TransactionLogFetch(TransactionId transactionId)
{
  XidStatus xidstatus;
  XLogRecPtr xidlsn;

     
                                                                            
                                                                      
     
  if (TransactionIdEquals(transactionId, cachedFetchXid))
  {
    return cachedFetchXidStatus;
  }

     
                                                                  
     
  if (!TransactionIdIsNormal(transactionId))
  {
    if (TransactionIdEquals(transactionId, BootstrapTransactionId))
    {
      return TRANSACTION_STATUS_COMMITTED;
    }
    if (TransactionIdEquals(transactionId, FrozenTransactionId))
    {
      return TRANSACTION_STATUS_COMMITTED;
    }
    return TRANSACTION_STATUS_ABORTED;
  }

     
                                 
     
  xidstatus = TransactionIdGetStatus(transactionId, &xidlsn);

     
                                                                       
                                                                           
     
  if (xidstatus != TRANSACTION_STATUS_IN_PROGRESS && xidstatus != TRANSACTION_STATUS_SUB_COMMITTED)
  {
    cachedFetchXid = transactionId;
    cachedFetchXidStatus = xidstatus;
    cachedCommitLSN = xidlsn;
  }

  return xidstatus;
}

                                                                    
                            
   
                           
                          
             
                                                      
                                   
   
                            
                                 
                           
             
                                                                   
                         
   
                                                                     
                                 
                                                                    
   

   
                          
                                                                    
   
         
                                                                
   
bool                                          
TransactionIdDidCommit(TransactionId transactionId)
{
  XidStatus xidstatus;

  xidstatus = TransactionLogFetch(transactionId);

     
                                               
     
  if (xidstatus == TRANSACTION_STATUS_COMMITTED)
  {
    return true;
  }

     
                                                                           
                                                                   
                                                                             
                   
     
                                                                          
                                                                             
                                                                           
                                                                         
                                                                        
                                                                            
                                                
     
  if (xidstatus == TRANSACTION_STATUS_SUB_COMMITTED)
  {
    TransactionId parentXid;

    if (TransactionIdPrecedes(transactionId, TransactionXmin))
    {
      return false;
    }
    parentXid = SubTransGetParent(transactionId);
    if (!TransactionIdIsValid(parentXid))
    {
      elog(WARNING, "no pg_subtrans entry for subcommitted XID %u", transactionId);
      return false;
    }
    return TransactionIdDidCommit(parentXid);
  }

     
                         
     
  return false;
}

   
                         
                                                                   
   
         
                                                                
   
bool                                        
TransactionIdDidAbort(TransactionId transactionId)
{
  XidStatus xidstatus;

  xidstatus = TransactionLogFetch(transactionId);

     
                                           
     
  if (xidstatus == TRANSACTION_STATUS_ABORTED)
  {
    return true;
  }

     
                                                                           
                                                                   
                                                                             
                   
     
  if (xidstatus == TRANSACTION_STATUS_SUB_COMMITTED)
  {
    TransactionId parentXid;

    if (TransactionIdPrecedes(transactionId, TransactionXmin))
    {
      return true;
    }
    parentXid = SubTransGetParent(transactionId);
    if (!TransactionIdIsValid(parentXid))
    {
                                               
      elog(WARNING, "no pg_subtrans entry for subcommitted XID %u", transactionId);
      return true;
    }
    return TransactionIdDidAbort(parentXid);
  }

     
                       
     
  return false;
}

   
                                 
                                                                     
                                               
   
                                                                     
                                                                       
                                                          
   
                                                                    
                                                                     
                                                                        
                                                                         
                                                                   
                                                                          
                               
   
         
                                             
   
bool
TransactionIdIsKnownCompleted(TransactionId transactionId)
{
  if (TransactionIdEquals(transactionId, cachedFetchXid))
  {
                                                            
    return true;
  }

  return false;
}

   
                           
                                                          
   
                                                                           
                              
   
                                                                             
                                         
   
void
TransactionIdCommitTree(TransactionId xid, int nxids, TransactionId *xids)
{
  TransactionIdSetTreeStatus(xid, nxids, xids, TRANSACTION_STATUS_COMMITTED, InvalidXLogRecPtr);
}

   
                                
                                                                            
   
void
TransactionIdAsyncCommitTree(TransactionId xid, int nxids, TransactionId *xids, XLogRecPtr lsn)
{
  TransactionIdSetTreeStatus(xid, nxids, xids, TRANSACTION_STATUS_COMMITTED, lsn);
}

   
                          
                                                         
   
                                                                           
                              
   
                                                                             
                                                            
   
void
TransactionIdAbortTree(TransactionId xid, int nxids, TransactionId *xids)
{
  TransactionIdSetTreeStatus(xid, nxids, xids, TRANSACTION_STATUS_ABORTED, InvalidXLogRecPtr);
}

   
                                                     
   
bool
TransactionIdPrecedes(TransactionId id1, TransactionId id2)
{
     
                                                                  
                                                                   
     
  int32 diff;

  if (!TransactionIdIsNormal(id1) || !TransactionIdIsNormal(id2))
  {
    return (id1 < id2);
  }

  diff = (int32)(id1 - id2);
  return (diff < 0);
}

   
                                                              
   
bool
TransactionIdPrecedesOrEquals(TransactionId id1, TransactionId id2)
{
  int32 diff;

  if (!TransactionIdIsNormal(id1) || !TransactionIdIsNormal(id2))
  {
    return (id1 <= id2);
  }

  diff = (int32)(id1 - id2);
  return (diff <= 0);
}

   
                                                    
   
bool
TransactionIdFollows(TransactionId id1, TransactionId id2)
{
  int32 diff;

  if (!TransactionIdIsNormal(id1) || !TransactionIdIsNormal(id2))
  {
    return (id1 > id2);
  }

  diff = (int32)(id1 - id2);
  return (diff > 0);
}

   
                                                             
   
bool
TransactionIdFollowsOrEquals(TransactionId id1, TransactionId id2)
{
  int32 diff;

  if (!TransactionIdIsNormal(id1) || !TransactionIdIsNormal(id2))
  {
    return (id1 >= id2);
  }

  diff = (int32)(id1 - id2);
  return (diff >= 0);
}

   
                                                                             
   
TransactionId
TransactionIdLatest(TransactionId mainxid, int nxids, const TransactionId *xids)
{
  TransactionId result;

     
                                                                             
                                                                           
                                                                         
                                                                           
                                                       
     
  result = mainxid;
  while (--nxids >= 0)
  {
    if (TransactionIdPrecedes(result, xids[nxids]))
    {
      result = xids[nxids];
    }
  }
  return result;
}

   
                             
   
                                                               
                                                                
                                                              
   
                                                                    
                                                                        
                                                                         
                                                                         
                                                                           
                   
   
XLogRecPtr
TransactionIdGetCommitLSN(TransactionId xid)
{
  XLogRecPtr result;

     
                                                                      
                                                                        
                                                                            
                                  
     
  if (TransactionIdEquals(xid, cachedFetchXid))
  {
    return cachedCommitLSN;
  }

                                               
  if (!TransactionIdIsNormal(xid))
  {
    return InvalidXLogRecPtr;
  }

     
                                 
     
  (void)TransactionIdGetStatus(xid, &result);

  return result;
}
