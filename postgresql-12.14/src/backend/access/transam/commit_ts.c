                                                                            
   
               
                                        
   
                                                                         
                         
   
                                                                          
                                                                     
                                                                            
                                                                          
                                                                         
                                                                          
                                                                            
               
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/commit_ts.h"
#include "access/htup_details.h"
#include "access/slru.h"
#include "access/transam.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "storage/shmem.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

   
                                                                          
                                
   
                                                                           
                                                
                                                                          
                                                                                
                                                                              
                                                                    
   

   
                                                                           
                                                                 
                      
   
typedef struct CommitTimestampEntry
{
  TimestampTz time;
  RepOriginId nodeid;
} CommitTimestampEntry;

#define SizeOfCommitTimestampEntry (offsetof(CommitTimestampEntry, nodeid) + sizeof(RepOriginId))

#define COMMIT_TS_XACTS_PER_PAGE (BLCKSZ / SizeOfCommitTimestampEntry)

#define TransactionIdToCTsPage(xid) ((xid) / (TransactionId)COMMIT_TS_XACTS_PER_PAGE)
#define TransactionIdToCTsEntry(xid) ((xid) % (TransactionId)COMMIT_TS_XACTS_PER_PAGE)

   
                                                              
   
static SlruCtlData CommitTsCtlData;

#define CommitTsCtl (&CommitTsCtlData)

   
                                                           
   
                                                                        
                                                                            
                                                                
   
                                                                             
                                                                          
                     
   
typedef struct CommitTimestampShared
{
  TransactionId xidLastCommit;
  CommitTimestampEntry dataLastCommit;
  bool commitTsActive;
} CommitTimestampShared;

CommitTimestampShared *commitTsShared;

                  
bool track_commit_timestamp;

static void
SetXidCommitTsInPage(TransactionId xid, int nsubxids, TransactionId *subxids, TimestampTz ts, RepOriginId nodeid, int pageno);
static void
TransactionIdSetCommitTs(TransactionId xid, TimestampTz ts, RepOriginId nodeid, int slotno);
static void
error_commit_ts_disabled(void);
static int
ZeroCommitTsPage(int pageno, bool writeXlog);
static bool
CommitTsPagePrecedes(int page1, int page2);
static void
ActivateCommitTs(void);
static void
DeactivateCommitTs(void);
static void
WriteZeroPageXlogRec(int pageno);
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXid);
static void
WriteSetTimestampXlogRec(TransactionId mainxid, int nsubxids, TransactionId *subxids, TimestampTz timestamp, RepOriginId nodeid);

   
                                  
   
                                                                              
                                                                              
   
                                        
   
                                                                                
                                                              
                                                                              
                                                                          
                                                                         
                                                                               
                                                       
   
                                                                               
                                                                            
                                                                              
                                                                                
                                                                              
                                                              
   
void
TransactionTreeSetCommitTsData(TransactionId xid, int nsubxids, TransactionId *subxids, TimestampTz timestamp, RepOriginId nodeid, bool write_xlog)
{
  int i;
  TransactionId headxid;
  TransactionId newestXact;

     
                                        
     
                                                                         
                                                                             
                                                                           
           
     
  if (!commitTsShared->commitTsActive)
  {
    return;
  }

     
                                                                             
                                                                  
     
  if (write_xlog)
  {
    WriteSetTimestampXlogRec(xid, nsubxids, subxids, timestamp, nodeid);
  }

     
                                                                        
                                            
     
  if (nsubxids > 0)
  {
    newestXact = subxids[nsubxids - 1];
  }
  else
  {
    newestXact = xid;
  }

     
                                                                          
                                                                          
                                                                             
                                                                         
                                      
     
  headxid = xid;
  i = 0;
  for (;;)
  {
    int pageno = TransactionIdToCTsPage(headxid);
    int j;

    for (j = i; j < nsubxids; j++)
    {
      if (TransactionIdToCTsPage(subxids[j]) != pageno)
      {
        break;
      }
    }
                                                        

    SetXidCommitTsInPage(headxid, j - i, subxids + i, timestamp, nodeid, pageno);

                                                  
    if (j >= nsubxids)
    {
      break;
    }

       
                                                                         
                   
       
    headxid = subxids[j];
    i = j + 1;
  }

                                                
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  commitTsShared->xidLastCommit = xid;
  commitTsShared->dataLastCommit.time = timestamp;
  commitTsShared->dataLastCommit.nodeid = nodeid;

                                                 
  if (TransactionIdPrecedes(ShmemVariableCache->newestCommitTsXid, newestXact))
  {
    ShmemVariableCache->newestCommitTsXid = newestXact;
  }
  LWLockRelease(CommitTsLock);
}

   
                                                                                
                                                        
   
static void
SetXidCommitTsInPage(TransactionId xid, int nsubxids, TransactionId *subxids, TimestampTz ts, RepOriginId nodeid, int pageno)
{
  int slotno;
  int i;

  LWLockAcquire(CommitTsControlLock, LW_EXCLUSIVE);

  slotno = SimpleLruReadPage(CommitTsCtl, pageno, true, xid);

  TransactionIdSetCommitTs(xid, ts, nodeid, slotno);
  for (i = 0; i < nsubxids; i++)
  {
    TransactionIdSetCommitTs(subxids[i], ts, nodeid, slotno);
  }

  CommitTsCtl->shared->page_dirty[slotno] = true;

  LWLockRelease(CommitTsControlLock);
}

   
                                                      
   
                                                
   
static void
TransactionIdSetCommitTs(TransactionId xid, TimestampTz ts, RepOriginId nodeid, int slotno)
{
  int entryno = TransactionIdToCTsEntry(xid);
  CommitTimestampEntry entry;

  Assert(TransactionIdIsNormal(xid));

  entry.time = ts;
  entry.nodeid = nodeid;

  memcpy(CommitTsCtl->shared->page_buffer[slotno] + SizeOfCommitTimestampEntry * entryno, &entry, SizeOfCommitTimestampEntry);
}

   
                                                      
   
                                                                              
                                                                            
                                                                              
         
   
bool
TransactionIdGetCommitTsData(TransactionId xid, TimestampTz *ts, RepOriginId *nodeid)
{
  int pageno = TransactionIdToCTsPage(xid);
  int entryno = TransactionIdToCTsEntry(xid);
  int slotno;
  CommitTimestampEntry entry;
  TransactionId oldestCommitTsXid;
  TransactionId newestCommitTsXid;

  if (!TransactionIdIsValid(xid))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("cannot retrieve commit timestamp for transaction %u", xid)));
  }
  else if (!TransactionIdIsNormal(xid))
  {
                                                                        
    *ts = 0;
    if (nodeid)
    {
      *nodeid = 0;
    }
    return false;
  }

  LWLockAcquire(CommitTsLock, LW_SHARED);

                                   
  if (!commitTsShared->commitTsActive)
  {
    error_commit_ts_disabled();
  }

     
                                                                        
                                
     
  if (commitTsShared->xidLastCommit == xid)
  {
    *ts = commitTsShared->dataLastCommit.time;
    if (nodeid)
    {
      *nodeid = commitTsShared->dataLastCommit.nodeid;
    }

    LWLockRelease(CommitTsLock);
    return *ts != 0;
  }

  oldestCommitTsXid = ShmemVariableCache->oldestCommitTsXid;
  newestCommitTsXid = ShmemVariableCache->newestCommitTsXid;
                                       
  Assert(TransactionIdIsValid(oldestCommitTsXid) == TransactionIdIsValid(newestCommitTsXid));
  LWLockRelease(CommitTsLock);

     
                                                                     
     
  if (!TransactionIdIsValid(oldestCommitTsXid) || TransactionIdPrecedes(xid, oldestCommitTsXid) || TransactionIdPrecedes(newestCommitTsXid, xid))
  {
    *ts = 0;
    if (nodeid)
    {
      *nodeid = InvalidRepOriginId;
    }
    return false;
  }

                                                      
  slotno = SimpleLruReadPage_ReadOnly(CommitTsCtl, pageno, xid);
  memcpy(&entry, CommitTsCtl->shared->page_buffer[slotno] + SizeOfCommitTimestampEntry * entryno, SizeOfCommitTimestampEntry);

  *ts = entry.time;
  if (nodeid)
  {
    *nodeid = entry.nodeid;
  }

  LWLockRelease(CommitTsControlLock);
  return *ts != 0;
}

   
                                                                               
                                                                             
                      
   
                                                                           
                          
   
TransactionId
GetLatestCommitTsData(TimestampTz *ts, RepOriginId *nodeid)
{
  TransactionId xid;

  LWLockAcquire(CommitTsLock, LW_SHARED);

                                   
  if (!commitTsShared->commitTsActive)
  {
    error_commit_ts_disabled();
  }

  xid = commitTsShared->xidLastCommit;
  if (ts)
  {
    *ts = commitTsShared->dataLastCommit.time;
  }
  if (nodeid)
  {
    *nodeid = commitTsShared->dataLastCommit.nodeid;
  }
  LWLockRelease(CommitTsLock);

  return xid;
}

static void
error_commit_ts_disabled(void)
{
  ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not get commit timestamp data"), RecoveryInProgress() ? errhint("Make sure the configuration parameter \"%s\" is set on the master server.", "track_commit_timestamp") : errhint("Make sure the configuration parameter \"%s\" is set.", "track_commit_timestamp")));
}

   
                                                               
   
Datum
pg_xact_commit_timestamp(PG_FUNCTION_ARGS)
{
  TransactionId xid = PG_GETARG_UINT32(0);
  TimestampTz ts;
  bool found;

  found = TransactionIdGetCommitTsData(xid, &ts, NULL);

  if (!found)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TIMESTAMPTZ(ts);
}

Datum
pg_last_committed_xact(PG_FUNCTION_ARGS)
{
  TransactionId xid;
  TimestampTz ts;
  Datum values[2];
  bool nulls[2];
  TupleDesc tupdesc;
  HeapTuple htup;

                                           
  xid = GetLatestCommitTsData(&ts, NULL);

     
                                                                            
                               
     
  tupdesc = CreateTemplateTupleDesc(2);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "xid", XIDOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)2, "timestamp", TIMESTAMPTZOID, -1, 0);
  tupdesc = BlessTupleDesc(tupdesc);

  if (!TransactionIdIsNormal(xid))
  {
    memset(nulls, true, sizeof(nulls));
  }
  else
  {
    values[0] = TransactionIdGetDatum(xid);
    nulls[0] = false;

    values[1] = TimestampTzGetDatum(ts);
    nulls[1] = false;
  }

  htup = heap_form_tuple(tupdesc, values, nulls);

  PG_RETURN_DATUM(HeapTupleGetDatum(htup));
}

   
                                      
   
                                                                               
                        
   
Size
CommitTsShmemBuffers(void)
{
  return Min(16, Max(4, NBuffers / 1024));
}

   
                                     
   
Size
CommitTsShmemSize(void)
{
  return SimpleLruShmemSize(CommitTsShmemBuffers(), 0) + sizeof(CommitTimestampShared);
}

   
                                                                         
            
   
void
CommitTsShmemInit(void)
{
  bool found;

  CommitTsCtl->PagePrecedes = CommitTsPagePrecedes;
  SimpleLruInit(CommitTsCtl, "commit_timestamp", CommitTsShmemBuffers(), 0, CommitTsControlLock, "pg_commit_ts", LWTRANCHE_COMMITTS_BUFFERS);
  SlruPagePrecedesUnitTests(CommitTsCtl, COMMIT_TS_XACTS_PER_PAGE);

  commitTsShared = ShmemInitStruct("CommitTs shared", sizeof(CommitTimestampShared), &found);

  if (!IsUnderPostmaster)
  {
    Assert(!found);

    commitTsShared->xidLastCommit = InvalidTransactionId;
    TIMESTAMP_NOBEGIN(commitTsShared->dataLastCommit.time);
    commitTsShared->dataLastCommit.nodeid = InvalidRepOriginId;
    commitTsShared->commitTsActive = false;
  }
  else
  {
    Assert(found);
  }
}

   
                                                        
   
                                                                          
                                                     
   
void
BootStrapCommitTs(void)
{
     
                                                                             
                                                                          
                       
     
}

   
                                                              
                                                                      
   
                                                                   
                                                
   
                                                                 
   
static int
ZeroCommitTsPage(int pageno, bool writeXlog)
{
  int slotno;

  slotno = SimpleLruZeroPage(CommitTsCtl, pageno);

  if (writeXlog)
  {
    WriteZeroPageXlogRec(pageno);
  }

  return slotno;
}

   
                                                                             
                                                                      
   
void
StartupCommitTs(void)
{
  ActivateCommitTs();
}

   
                                                                             
                                
   
void
CompleteCommitTsInitialization(void)
{
     
                                                                             
                        
     
                                                                            
                                                                        
                                                                  
                                        
     
  if (!track_commit_timestamp)
  {
    DeactivateCommitTs();
  }
  else
  {
    ActivateCommitTs();
  }
}

   
                                                                              
                                
   
void
CommitTsParameterChange(bool newvalue, bool oldvalue)
{
     
                                                                             
                                                                            
                                                                       
                                                                            
                        
     
                                                                          
                                    
     
                                                                         
           
     
  if (newvalue)
  {
    if (!commitTsShared->commitTsActive)
    {
      ActivateCommitTs();
    }
  }
  else if (commitTsShared->commitTsActive)
  {
    DeactivateCommitTs();
  }
}

   
                                            
                                                                      
                                                                       
                           
   
                                                                                
                                                                                
                                                                              
                                                                    
   
                                                                           
                                                                          
                                                                             
                              
   
static void
ActivateCommitTs(void)
{
  TransactionId xid;
  int pageno;

                                                         
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  if (commitTsShared->commitTsActive)
  {
    LWLockRelease(CommitTsLock);
    return;
  }
  LWLockRelease(CommitTsLock);

  xid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  pageno = TransactionIdToCTsPage(xid);

     
                                                       
     
  LWLockAcquire(CommitTsControlLock, LW_EXCLUSIVE);
  CommitTsCtl->shared->latest_page_number = pageno;
  LWLockRelease(CommitTsControlLock);

     
                                                                          
                                                                            
                                                             
     
                                                                       
                                                                          
                                                                             
                                                                      
                                                                       
                                                                             
                          
     
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  if (ShmemVariableCache->oldestCommitTsXid == InvalidTransactionId)
  {
    ShmemVariableCache->oldestCommitTsXid = ShmemVariableCache->newestCommitTsXid = ReadNewTransactionId();
  }
  LWLockRelease(CommitTsLock);

                                                     
  if (!SimpleLruDoesPhysicalPageExist(CommitTsCtl, pageno))
  {
    int slotno;

    LWLockAcquire(CommitTsControlLock, LW_EXCLUSIVE);
    slotno = ZeroCommitTsPage(pageno, false);
    SimpleLruWritePage(CommitTsCtl, slotno);
    Assert(!CommitTsCtl->shared->page_dirty[slotno]);
    LWLockRelease(CommitTsControlLock);
  }

                                                      
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  commitTsShared->commitTsActive = true;
  LWLockRelease(CommitTsLock);
}

   
                           
   
                                                                                
                                                                               
           
   
                                                                      
                                                             
   
static void
DeactivateCommitTs(void)
{
     
                                              
     
                                                                           
                                                                            
                                                              
     
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);

  commitTsShared->commitTsActive = false;
  commitTsShared->xidLastCommit = InvalidTransactionId;
  TIMESTAMP_NOBEGIN(commitTsShared->dataLastCommit.time);
  commitTsShared->dataLastCommit.nodeid = InvalidRepOriginId;

  ShmemVariableCache->oldestCommitTsXid = InvalidTransactionId;
  ShmemVariableCache->newestCommitTsXid = InvalidTransactionId;

  LWLockRelease(CommitTsLock);

     
                                                                          
                                                                          
                                                                             
                                                                           
                                                                          
            
     
  LWLockAcquire(CommitTsControlLock, LW_EXCLUSIVE);
  (void)SlruScanDirectory(CommitTsCtl, SlruScanDirCbDeleteAll, NULL);
  LWLockRelease(CommitTsControlLock);
}

   
                                                                             
   
void
ShutdownCommitTs(void)
{
                                          
  SimpleLruFlush(CommitTsCtl, false);

     
                                                                        
                      
     
  fsync_fname("pg_commit_ts", true);
}

   
                                                                  
   
void
CheckPointCommitTs(void)
{
                                          
  SimpleLruFlush(CommitTsCtl, true);
}

   
                                                               
   
                                                                            
                                                                           
                                                                               
                     
   
                                                                         
                   
   
void
ExtendCommitTs(TransactionId newestXact)
{
  int pageno;

     
                                                                          
                                                                           
                                                              
     
  Assert(!InRecovery);
  if (!commitTsShared->commitTsActive)
  {
    return;
  }

     
                                                                    
                                                                         
     
  if (TransactionIdToCTsEntry(newestXact) != 0 && !TransactionIdEquals(newestXact, FirstNormalTransactionId))
  {
    return;
  }

  pageno = TransactionIdToCTsPage(newestXact);

  LWLockAcquire(CommitTsControlLock, LW_EXCLUSIVE);

                                                     
  ZeroCommitTsPage(pageno, !InRecovery);

  LWLockRelease(CommitTsControlLock);
}

   
                                                                  
                   
   
                                               
   
void
TruncateCommitTs(TransactionId oldestXact)
{
  int cutoffPage;

     
                                                                            
                                                                 
     
  cutoffPage = TransactionIdToCTsPage(oldestXact);

                                                               
  if (!SlruScanDirectory(CommitTsCtl, SlruScanDirCbReportPresence, &cutoffPage))
  {
    return;                        
  }

                         
  WriteTruncateXlogRec(cutoffPage, oldestXact);

                                                     
  SimpleLruTruncate(CommitTsCtl, cutoffPage);
}

   
                                                                  
   
void
SetCommitTsLimit(TransactionId oldestXact, TransactionId newestXact)
{
     
                                                                         
                                             
     
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  if (ShmemVariableCache->oldestCommitTsXid != InvalidTransactionId)
  {
    if (TransactionIdPrecedes(ShmemVariableCache->oldestCommitTsXid, oldestXact))
    {
      ShmemVariableCache->oldestCommitTsXid = oldestXact;
    }
    if (TransactionIdPrecedes(newestXact, ShmemVariableCache->newestCommitTsXid))
    {
      ShmemVariableCache->newestCommitTsXid = newestXact;
    }
  }
  else
  {
    Assert(ShmemVariableCache->newestCommitTsXid == InvalidTransactionId);
    ShmemVariableCache->oldestCommitTsXid = oldestXact;
    ShmemVariableCache->newestCommitTsXid = newestXact;
  }
  LWLockRelease(CommitTsLock);
}

   
                                                                 
   
void
AdvanceOldestCommitTsXid(TransactionId oldestXact)
{
  LWLockAcquire(CommitTsLock, LW_EXCLUSIVE);
  if (ShmemVariableCache->oldestCommitTsXid != InvalidTransactionId && TransactionIdPrecedes(ShmemVariableCache->oldestCommitTsXid, oldestXact))
  {
    ShmemVariableCache->oldestCommitTsXid = oldestXact;
  }
  LWLockRelease(CommitTsLock);
}

   
                                                                             
                                    
   
                                                                         
                                                                            
                                                           
                                                                            
                                                                               
                                                                            
                                                                              
                                                                           
                                                                      
                                                                    
   
                                                                             
                                                                               
                                                                              
                                                                               
                                                                             
                                                                     
                                                               
   
static bool
CommitTsPagePrecedes(int page1, int page2)
{
  TransactionId xid1;
  TransactionId xid2;

  xid1 = ((TransactionId)page1) * COMMIT_TS_XACTS_PER_PAGE;
  xid1 += FirstNormalTransactionId + 1;
  xid2 = ((TransactionId)page2) * COMMIT_TS_XACTS_PER_PAGE;
  xid2 += FirstNormalTransactionId + 1;

  return (TransactionIdPrecedes(xid1, xid2) && TransactionIdPrecedes(xid1, xid2 + COMMIT_TS_XACTS_PER_PAGE - 1));
}

   
                                
   
static void
WriteZeroPageXlogRec(int pageno)
{
  XLogBeginInsert();
  XLogRegisterData((char *)(&pageno), sizeof(int));
  (void)XLogInsert(RM_COMMIT_TS_ID, COMMIT_TS_ZEROPAGE);
}

   
                                
   
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXid)
{
  xl_commit_ts_truncate xlrec;

  xlrec.pageno = pageno;
  xlrec.oldestXid = oldestXid;

  XLogBeginInsert();
  XLogRegisterData((char *)(&xlrec), SizeOfCommitTsTruncate);
  (void)XLogInsert(RM_COMMIT_TS_ID, COMMIT_TS_TRUNCATE);
}

   
                             
   
static void
WriteSetTimestampXlogRec(TransactionId mainxid, int nsubxids, TransactionId *subxids, TimestampTz timestamp, RepOriginId nodeid)
{
  xl_commit_ts_set record;

  record.timestamp = timestamp;
  record.nodeid = nodeid;
  record.mainxid = mainxid;

  XLogBeginInsert();
  XLogRegisterData((char *)&record, offsetof(xl_commit_ts_set, mainxid) + sizeof(TransactionId));
  XLogRegisterData((char *)subxids, nsubxids * sizeof(TransactionId));
  XLogInsert(RM_COMMIT_TS_ID, COMMIT_TS_SETTS);
}

   
                                        
   
void
commit_ts_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                       
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == COMMIT_TS_ZEROPAGE)
  {
    int pageno;
    int slotno;

    memcpy(&pageno, XLogRecGetData(record), sizeof(int));

    LWLockAcquire(CommitTsControlLock, LW_EXCLUSIVE);

    slotno = ZeroCommitTsPage(pageno, false);
    SimpleLruWritePage(CommitTsCtl, slotno);
    Assert(!CommitTsCtl->shared->page_dirty[slotno]);

    LWLockRelease(CommitTsControlLock);
  }
  else if (info == COMMIT_TS_TRUNCATE)
  {
    xl_commit_ts_truncate *trunc = (xl_commit_ts_truncate *)XLogRecGetData(record);

    AdvanceOldestCommitTsXid(trunc->oldestXid);

       
                                                                         
                                                                      
       
    CommitTsCtl->shared->latest_page_number = trunc->pageno;

    SimpleLruTruncate(CommitTsCtl, trunc->pageno);
  }
  else if (info == COMMIT_TS_SETTS)
  {
    xl_commit_ts_set *setts = (xl_commit_ts_set *)XLogRecGetData(record);
    int nsubxids;
    TransactionId *subxids;

    nsubxids = ((XLogRecGetDataLen(record) - SizeOfCommitTsSet) / sizeof(TransactionId));
    if (nsubxids > 0)
    {
      subxids = palloc(sizeof(TransactionId) * nsubxids);
      memcpy(subxids, XLogRecGetData(record) + SizeOfCommitTsSet, sizeof(TransactionId) * nsubxids);
    }
    else
    {
      subxids = NULL;
    }

    TransactionTreeSetCommitTsData(setts->mainxid, nsubxids, subxids, setts->timestamp, setts->nodeid, false);
    if (subxids)
    {
      pfree(subxids);
    }
  }
  else
  {
    elog(PANIC, "commit_ts_redo: unknown op code %u", info);
  }
}
