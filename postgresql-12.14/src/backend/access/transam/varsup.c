                                                                            
   
            
                                                   
   
                                                                
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "access/clog.h"
#include "access/commit_ts.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "commands/dbcommands.h"
#include "miscadmin.h"
#include "postmaster/autovacuum.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "utils/syscache.h"

                                                             
#define VAR_OID_PREFETCH 8192

                                                                      
VariableCache ShmemVariableCache = NULL;

   
                                                                
                   
   
                                                              
   
                                                                     
                                                                       
                                                                        
                                   
   
FullTransactionId
GetNewTransactionId(bool isSubXact)
{
  FullTransactionId full_xid;
  TransactionId xid;

     
                                                                             
                                                                   
     
  if (IsInParallelMode())
  {
    elog(ERROR, "cannot assign TransactionIds during a parallel operation");
  }

     
                                                                      
                     
     
  if (IsBootstrapProcessingMode())
  {
    Assert(!isSubXact);
    MyPgXact->xid = BootstrapTransactionId;
    return FullTransactionIdFromEpochAndXid(0, BootstrapTransactionId);
  }

                                                                  
  if (RecoveryInProgress())
  {
    elog(ERROR, "cannot assign TransactionIds during recovery");
  }

  LWLockAcquire(XidGenLock, LW_EXCLUSIVE);

  full_xid = ShmemVariableCache->nextFullXid;
  xid = XidFromFullTransactionId(full_xid);

               
                                                                             
                                                                         
     
                                                                         
                                                         
                                                                        
                                                                     
                                                            
     
                                                              
               
     
  if (TransactionIdFollowsOrEquals(xid, ShmemVariableCache->xidVacLimit))
  {
       
                                                                       
                                                                 
                                                                 
                                                                       
                                                           
       
    TransactionId xidWarnLimit = ShmemVariableCache->xidWarnLimit;
    TransactionId xidStopLimit = ShmemVariableCache->xidStopLimit;
    TransactionId xidWrapLimit = ShmemVariableCache->xidWrapLimit;
    Oid oldest_datoid = ShmemVariableCache->oldestXidDB;

    LWLockRelease(XidGenLock);

       
                                                                           
                                                                       
                                                          
       
    if (IsUnderPostmaster && (xid % 65536) == 0)
    {
      SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);
    }

    if (IsUnderPostmaster && TransactionIdFollowsOrEquals(xid, xidStopLimit))
    {
      char *oldest_datname = get_database_name(oldest_datoid);

                                                    
      if (oldest_datname)
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("database is not accepting commands to avoid wraparound data loss in database \"%s\"", oldest_datname),
                           errhint("Stop the postmaster and vacuum that database in single-user mode.\n"
                                   "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("database is not accepting commands to avoid wraparound data loss in database with OID %u", oldest_datoid),
                           errhint("Stop the postmaster and vacuum that database in single-user mode.\n"
                                   "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
    }
    else if (TransactionIdFollowsOrEquals(xid, xidWarnLimit))
    {
      char *oldest_datname = get_database_name(oldest_datoid);

                                                    
      if (oldest_datname)
      {
        ereport(WARNING, (errmsg("database \"%s\" must be vacuumed within %u transactions", oldest_datname, xidWrapLimit - xid), errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
                                                                                                                                         "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
      else
      {
        ereport(WARNING, (errmsg("database with OID %u must be vacuumed within %u transactions", oldest_datoid, xidWrapLimit - xid), errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
                                                                                                                                             "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
    }

                                        
    LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
    full_xid = ShmemVariableCache->nextFullXid;
    xid = XidFromFullTransactionId(full_xid);
  }

     
                                                                         
                                                                           
                                                                            
                                                                         
                                                                             
     
                                              
     
  ExtendCLOG(xid);
  ExtendCommitTs(xid);
  ExtendSUBTRANS(xid);

     
                                                                            
                                                                            
                                                                       
                                                          
     
  FullTransactionIdAdvance(&ShmemVariableCache->nextFullXid);

     
                                                                          
                                                                
                                                                            
                                                                         
     
                                                                           
                                                                        
                                                                         
                                                   
     
                                                                           
     
                                                                          
                                                                           
                                                                 
                                                                             
                
     
                                                                      
                                                                            
                                                     
                                              
     
                                                                         
                                                                    
                                                                             
                                                                           
                                                                           
                                                                             
                                                                          
                                                                            
                                                                  
     
  if (!isSubXact)
  {
    MyPgXact->xid = xid;                                    
  }
  else
  {
    int nxids = MyPgXact->nxids;

    if (nxids < PGPROC_MAX_CACHED_SUBXIDS)
    {
      MyProc->subxids.xids[nxids] = xid;
      pg_write_barrier();
      MyPgXact->nxids = nxids + 1;
    }
    else
    {
      MyPgXact->overflowed = true;
    }
  }

  LWLockRelease(XidGenLock);

  return full_xid;
}

   
                                           
   
FullTransactionId
ReadNextFullTransactionId(void)
{
  FullTransactionId fullXid;

  LWLockAcquire(XidGenLock, LW_SHARED);
  fullXid = ShmemVariableCache->nextFullXid;
  LWLockRelease(XidGenLock);

  return fullXid;
}

   
                                                                               
                                                                             
   
void
AdvanceNextFullTransactionIdPastXid(TransactionId xid)
{
  FullTransactionId newNextFullXid;
  TransactionId next_xid;
  uint32 epoch;

     
                                                                         
                                                                             
                                  
     
  Assert(AmStartupProcess() || !IsUnderPostmaster);

                                                                        
  next_xid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  if (!TransactionIdFollowsOrEquals(xid, next_xid))
  {
    return;
  }

     
                                                                          
                                                                         
                                                                            
                                                                           
                                                                          
                              
     
  TransactionIdAdvance(xid);
  epoch = EpochFromFullTransactionId(ShmemVariableCache->nextFullXid);
  if (unlikely(xid < next_xid))
  {
    ++epoch;
  }
  newNextFullXid = FullTransactionIdFromEpochAndXid(epoch, xid);

     
                                                                     
                         
     
  LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
  ShmemVariableCache->nextFullXid = newNextFullXid;
  LWLockRelease(XidGenLock);
}

   
                                                                   
   
                                                                             
                                                                              
                                                                            
                                                                          
                              
   
void
AdvanceOldestClogXid(TransactionId oldest_datfrozenxid)
{
  LWLockAcquire(CLogTruncationLock, LW_EXCLUSIVE);
  if (TransactionIdPrecedes(ShmemVariableCache->oldestClogXid, oldest_datfrozenxid))
  {
    ShmemVariableCache->oldestClogXid = oldest_datfrozenxid;
  }
  LWLockRelease(CLogTruncationLock);
}

   
                                                                      
                                                                     
                                                                        
   
void
SetTransactionIdLimit(TransactionId oldest_datfrozenxid, Oid oldest_datoid)
{
  TransactionId xidVacLimit;
  TransactionId xidWarnLimit;
  TransactionId xidStopLimit;
  TransactionId xidWrapLimit;
  TransactionId curXid;

  Assert(TransactionIdIsNormal(oldest_datfrozenxid));

     
                                                                         
                                                                     
                                                                            
                                                                           
                            
     
  xidWrapLimit = oldest_datfrozenxid + (MaxTransactionId >> 1);
  if (xidWrapLimit < FirstNormalTransactionId)
  {
    xidWrapLimit += FirstNormalTransactionId;
  }

     
                                                                             
                                                                            
                                                                         
                                                                     
                                                                            
                                        
     
  xidStopLimit = xidWrapLimit - 1000000;
  if (xidStopLimit < FirstNormalTransactionId)
  {
    xidStopLimit -= FirstNormalTransactionId;
  }

     
                                                                           
                                                                         
                                                                         
                                                                            
                                                                         
                                                                             
                                                                             
                                                             
     
  xidWarnLimit = xidStopLimit - 10000000;
  if (xidWarnLimit < FirstNormalTransactionId)
  {
    xidWarnLimit -= FirstNormalTransactionId;
  }

     
                                                                           
                                                                 
     
                                                                            
                                                           
     
                                                                           
                                                                         
                                                                         
                                                                    
                                                                            
                                                                         
                          
     
  xidVacLimit = oldest_datfrozenxid + autovacuum_freeze_max_age;
  if (xidVacLimit < FirstNormalTransactionId)
  {
    xidVacLimit += FirstNormalTransactionId;
  }

                                                                  
  LWLockAcquire(XidGenLock, LW_EXCLUSIVE);
  ShmemVariableCache->oldestXid = oldest_datfrozenxid;
  ShmemVariableCache->xidVacLimit = xidVacLimit;
  ShmemVariableCache->xidWarnLimit = xidWarnLimit;
  ShmemVariableCache->xidStopLimit = xidStopLimit;
  ShmemVariableCache->xidWrapLimit = xidWrapLimit;
  ShmemVariableCache->oldestXidDB = oldest_datoid;
  curXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  LWLockRelease(XidGenLock);

                    
  ereport(DEBUG1, (errmsg("transaction ID wrap limit is %u, limited by database with OID %u", xidWrapLimit, oldest_datoid)));

     
                                                                       
                                                                      
                                                                         
                                                                         
                                                                         
     
  if (TransactionIdFollowsOrEquals(curXid, xidVacLimit) && IsUnderPostmaster && !InRecovery)
  {
    SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);
  }

                                                             
  if (TransactionIdFollowsOrEquals(curXid, xidWarnLimit) && !InRecovery)
  {
    char *oldest_datname;

       
                                                                          
                                                                          
                                             
       
                                                                         
                                                                       
                                                                     
       
    if (IsTransactionState())
    {
      oldest_datname = get_database_name(oldest_datoid);
    }
    else
    {
      oldest_datname = NULL;
    }

    if (oldest_datname)
    {
      ereport(WARNING, (errmsg("database \"%s\" must be vacuumed within %u transactions", oldest_datname, xidWrapLimit - curXid), errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
                                                                                                                                          "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
    }
    else
    {
      ereport(WARNING, (errmsg("database with OID %u must be vacuumed within %u transactions", oldest_datoid, xidWrapLimit - curXid), errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
                                                                                                                                              "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
    }
  }
}

   
                                                                                
   
                                                                          
                                                                           
                                                                        
                                                                    
                                                                          
                                                      
   
bool
ForceTransactionIdLimitUpdate(void)
{
  TransactionId nextXid;
  TransactionId xidVacLimit;
  TransactionId oldestXid;
  Oid oldestXidDB;

                                                                      
  LWLockAcquire(XidGenLock, LW_SHARED);
  nextXid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  xidVacLimit = ShmemVariableCache->xidVacLimit;
  oldestXid = ShmemVariableCache->oldestXid;
  oldestXidDB = ShmemVariableCache->oldestXidDB;
  LWLockRelease(XidGenLock);

  if (!TransactionIdIsNormal(oldestXid))
  {
    return true;                                         
  }
  if (!TransactionIdIsValid(xidVacLimit))
  {
    return true;                                           
  }
  if (TransactionIdFollowsOrEquals(nextXid, xidVacLimit))
  {
    return true;                                          
  }
  if (!SearchSysCacheExists1(DATABASEOID, ObjectIdGetDatum(oldestXidDB)))
  {
    return true;                                       
  }
  return false;
}

   
                                        
   
                                                                              
                                                                              
                                                                           
                                                                               
                                                                     
                      
   
Oid
GetNewObjectId(void)
{
  Oid result;

                                                                  
  if (RecoveryInProgress())
  {
    elog(ERROR, "cannot assign OIDs during recovery");
  }

  LWLockAcquire(OidGenLock, LW_EXCLUSIVE);

     
                                                                      
                                                                            
                                                                      
                                                                           
     
                                                                             
                                                                             
                                                                            
                                                                       
                                                                            
                                                                           
                                                  
     
  if (ShmemVariableCache->nextOid < ((Oid)FirstNormalObjectId))
  {
    if (IsPostmasterEnvironment)
    {
                                                                       
      ShmemVariableCache->nextOid = FirstNormalObjectId;
      ShmemVariableCache->oidCount = 0;
    }
    else
    {
                                                                    
      if (ShmemVariableCache->nextOid < ((Oid)FirstBootstrapObjectId))
      {
                                                                   
        ShmemVariableCache->nextOid = FirstNormalObjectId;
        ShmemVariableCache->oidCount = 0;
      }
    }
  }

                                                                  
  if (ShmemVariableCache->oidCount == 0)
  {
    XLogPutNextOid(ShmemVariableCache->nextOid + VAR_OID_PREFETCH);
    ShmemVariableCache->oidCount = VAR_OID_PREFETCH;
  }

  result = ShmemVariableCache->nextOid;

  (ShmemVariableCache->nextOid)++;
  (ShmemVariableCache->oidCount)--;

  LWLockRelease(OidGenLock);

  return result;
}
