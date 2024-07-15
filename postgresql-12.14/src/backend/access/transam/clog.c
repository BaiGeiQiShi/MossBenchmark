                                                                            
   
          
                                              
   
                                                                           
                                                                           
                                                                         
                                                                       
                                                                           
                                                                  
   
                                                                          
                                                                       
                                                                           
                                                                           
                                                                            
                                                                               
                                                                            
                                                                           
                                                                            
                                                                              
                                                                             
                                            
   
                                                                         
                                                                        
   
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/clog.h"
#include "access/slru.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "pg_trace.h"
#include "storage/proc.h"

   
                                                                      
                                
   
                                                                           
                                                                            
                                 
                                                                           
                                                                              
                                                            
   

                                                            
#define CLOG_BITS_PER_XACT 2
#define CLOG_XACTS_PER_BYTE 4
#define CLOG_XACTS_PER_PAGE (BLCKSZ * CLOG_XACTS_PER_BYTE)
#define CLOG_XACT_BITMASK ((1 << CLOG_BITS_PER_XACT) - 1)

#define TransactionIdToPage(xid) ((xid) / (TransactionId)CLOG_XACTS_PER_PAGE)
#define TransactionIdToPgIndex(xid) ((xid) % (TransactionId)CLOG_XACTS_PER_PAGE)
#define TransactionIdToByte(xid) (TransactionIdToPgIndex(xid) / CLOG_XACTS_PER_BYTE)
#define TransactionIdToBIndex(xid) ((xid) % (TransactionId)CLOG_XACTS_PER_BYTE)

                                                                  
#define CLOG_XACTS_PER_LSN_GROUP 32                             
#define CLOG_LSNS_PER_PAGE (CLOG_XACTS_PER_PAGE / CLOG_XACTS_PER_LSN_GROUP)

#define GetLSNIndex(slotno, xid) ((slotno) * CLOG_LSNS_PER_PAGE + ((xid) % (TransactionId)CLOG_XACTS_PER_PAGE) / CLOG_XACTS_PER_LSN_GROUP)

   
                                                                             
                                                                              
                     
   
#define THRESHOLD_SUBTRANS_CLOG_OPT 5

   
                                                          
   
static SlruCtlData ClogCtlData;

#define ClogCtl (&ClogCtlData)

static int
ZeroCLOGPage(int pageno, bool writeXlog);
static bool
CLOGPagePrecedes(int page1, int page2);
static void
WriteZeroPageXlogRec(int pageno);
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXact, Oid oldestXidDb);
static void
TransactionIdSetPageStatus(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno, bool all_xact_same_page);
static void
TransactionIdSetStatusBit(TransactionId xid, XidStatus status, XLogRecPtr lsn, int slotno);
static void
set_status_by_pages(int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn);
static bool
TransactionGroupUpdateXidStatus(TransactionId xid, XidStatus status, XLogRecPtr lsn, int pageno);
static void
TransactionIdSetPageStatusInternal(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno);

   
                              
   
                                                                       
                                                                          
                                         
   
                                                                 
                                                                       
                                                               
   
                                                                                
                                                              
   
                                                                             
                                                                            
                                                                            
                                                     
   
                                                                              
                                                                              
                                                                           
           
                                                                          
             
                                                                              
                                                            
                                                                           
                                      
   
            
                                                           
                                                                        
                        
                                         
                                      
                     
                                
                                
                                                                 
                        
                                     
                                  
   
                                                                        
                                                                   
   
                                                                        
                                                                        
              
   
void
TransactionIdSetTreeStatus(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn)
{
  int pageno = TransactionIdToPage(xid);                         
  int i;

  Assert(status == TRANSACTION_STATUS_COMMITTED || status == TRANSACTION_STATUS_ABORTED);

     
                                                                          
          
     
  for (i = 0; i < nsubxids; i++)
  {
    if (TransactionIdToPage(subxids[i]) != pageno)
    {
      break;
    }
  }

     
                                        
     
  if (i == nsubxids)
  {
       
                                                               
       
    TransactionIdSetPageStatus(xid, nsubxids, subxids, status, lsn, pageno, true);
  }
  else
  {
    int nsubxids_on_first_page = i;

       
                                                                         
                                                                      
                                                                          
                                                                          
                                                                      
       
                                                                         
                                           
       
    if (status == TRANSACTION_STATUS_COMMITTED)
    {
      set_status_by_pages(nsubxids - nsubxids_on_first_page, subxids + nsubxids_on_first_page, TRANSACTION_STATUS_SUB_COMMITTED, lsn);
    }

       
                                                                          
              
       
    pageno = TransactionIdToPage(xid);
    TransactionIdSetPageStatus(xid, nsubxids_on_first_page, subxids, status, lsn, pageno, false);

       
                                                                         
                                                                 
       
    set_status_by_pages(nsubxids - nsubxids_on_first_page, subxids + nsubxids_on_first_page, status, lsn);
  }
}

   
                                                                        
                                                                        
                                                                          
                                                                
   
static void
set_status_by_pages(int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn)
{
  int pageno = TransactionIdToPage(subxids[0]);
  int offset = 0;
  int i = 0;

  Assert(nsubxids > 0);                                            

  while (i < nsubxids)
  {
    int num_on_page = 0;
    int nextpageno;

    do
    {
      nextpageno = TransactionIdToPage(subxids[i]);
      if (nextpageno != pageno)
      {
        break;
      }
      num_on_page++;
      i++;
    } while (i < nsubxids);

    TransactionIdSetPageStatus(InvalidTransactionId, num_on_page, subxids + offset, status, lsn, pageno, false);
    offset = i;
    pageno = nextpageno;
  }
}

   
                                                                           
                                                        
   
static void
TransactionIdSetPageStatus(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno, bool all_xact_same_page)
{
                                                     
  StaticAssertStmt(THRESHOLD_SUBTRANS_CLOG_OPT <= PGPROC_MAX_CACHED_SUBXIDS, "group clog threshold less than PGPROC cached subxids");

     
                                                                           
                                                                      
                                                               
                                                      
     
                                                                           
                                                                        
                                           
     
                                                                       
                                                                           
                                                     
     
  if (all_xact_same_page && xid == MyPgXact->xid && nsubxids <= THRESHOLD_SUBTRANS_CLOG_OPT && nsubxids == MyPgXact->nxids && (nsubxids == 0 || memcmp(subxids, MyProc->subxids.xids, nsubxids * sizeof(TransactionId)) == 0))
  {
       
                                                                           
                                                                       
                                                                       
                                                            
       
    if (LWLockConditionalAcquire(CLogControlLock, LW_EXCLUSIVE))
    {
                                                         
      TransactionIdSetPageStatusInternal(xid, nsubxids, subxids, status, lsn, pageno);
      LWLockRelease(CLogControlLock);
      return;
    }
    else if (TransactionGroupUpdateXidStatus(xid, status, lsn, pageno))
    {
                                                     
      return;
    }

                                                     
  }

                                                                         
  LWLockAcquire(CLogControlLock, LW_EXCLUSIVE);
  TransactionIdSetPageStatusInternal(xid, nsubxids, subxids, status, lsn, pageno);
  LWLockRelease(CLogControlLock);
}

   
                                                                 
   
                                                          
   
static void
TransactionIdSetPageStatusInternal(TransactionId xid, int nsubxids, TransactionId *subxids, XidStatus status, XLogRecPtr lsn, int pageno)
{
  int slotno;
  int i;

  Assert(status == TRANSACTION_STATUS_COMMITTED || status == TRANSACTION_STATUS_ABORTED || (status == TRANSACTION_STATUS_SUB_COMMITTED && !TransactionIdIsValid(xid)));
  Assert(LWLockHeldByMeInMode(CLogControlLock, LW_EXCLUSIVE));

     
                                                                          
                                                                       
                                                                       
                                                                           
                                                                        
                                                                            
               
     
  slotno = SimpleLruReadPage(ClogCtl, pageno, XLogRecPtrIsInvalid(lsn), xid);

     
                                          
     
                                                                           
                                                                           
                                                                        
                                                                             
                           
     
  if (TransactionIdIsValid(xid))
  {
                                              
    if (status == TRANSACTION_STATUS_COMMITTED)
    {
      for (i = 0; i < nsubxids; i++)
      {
        Assert(ClogCtl->shared->page_number[slotno] == TransactionIdToPage(subxids[i]));
        TransactionIdSetStatusBit(subxids[i], TRANSACTION_STATUS_SUB_COMMITTED, lsn, slotno);
      }
    }

                                       
    TransactionIdSetStatusBit(xid, status, lsn, slotno);
  }

                               
  for (i = 0; i < nsubxids; i++)
  {
    Assert(ClogCtl->shared->page_number[slotno] == TransactionIdToPage(subxids[i]));
    TransactionIdSetStatusBit(subxids[i], status, lsn, slotno);
  }

  ClogCtl->shared->page_dirty[slotno] = true;
}

   
                                                                           
                                                                          
                                                                            
                                                                            
                                                                           
                                                                            
                                                                        
                        
   
                                                                          
                                                                          
                                                                          
   
static bool
TransactionGroupUpdateXidStatus(TransactionId xid, XidStatus status, XLogRecPtr lsn, int pageno)
{
  volatile PROC_HDR *procglobal = ProcGlobal;
  PGPROC *proc = MyProc;
  uint32 nextidx;
  uint32 wakeidx;

                                                                          
  Assert(TransactionIdIsValid(xid));

     
                                                                       
             
     
  proc->clogGroupMember = true;
  proc->clogGroupMemberXid = xid;
  proc->clogGroupMemberXidStatus = status;
  proc->clogGroupMemberPage = pageno;
  proc->clogGroupMemberLsn = lsn;

  nextidx = pg_atomic_read_u32(&procglobal->clogGroupFirst);

  while (true)
  {
       
                                                                          
                                                                       
       
                                                                           
                                                                       
                                                                          
                                                                           
                                                                   
                                                                         
                         
       
    if (nextidx != INVALID_PGPROCNO && ProcGlobal->allProcs[nextidx].clogGroupMemberPage != proc->clogGroupMemberPage)
    {
         
                                                                      
                                     
         
      proc->clogGroupMember = false;
      pg_atomic_write_u32(&proc->clogGroupNext, INVALID_PGPROCNO);
      return false;
    }

    pg_atomic_write_u32(&proc->clogGroupNext, nextidx);

    if (pg_atomic_compare_exchange_u32(&procglobal->clogGroupFirst, &nextidx, (uint32)proc->pgprocno))
    {
      break;
    }
  }

     
                                                                         
                                                                          
                                                                      
                                  
     
  if (nextidx != INVALID_PGPROCNO)
  {
    int extraWaits = 0;

                                                        
    pgstat_report_wait_start(WAIT_EVENT_CLOG_GROUP_UPDATE);
    for (;;)
    {
                                  
      PGSemaphoreLock(proc->sem);
      if (!proc->clogGroupMember)
      {
        break;
      }
      extraWaits++;
    }
    pgstat_report_wait_end();

    Assert(pg_atomic_read_u32(&proc->clogGroupNext) == INVALID_PGPROCNO);

                                                      
    while (extraWaits-- > 0)
    {
      PGSemaphoreUnlock(proc->sem);
    }
    return true;
  }

                                                                   
  LWLockAcquire(CLogControlLock, LW_EXCLUSIVE);

     
                                                                          
                                                                        
                                                                        
     
  nextidx = pg_atomic_exchange_u32(&procglobal->clogGroupFirst, INVALID_PGPROCNO);

                                                                            
  wakeidx = nextidx;

                                                        
  while (nextidx != INVALID_PGPROCNO)
  {
    PGPROC *proc = &ProcGlobal->allProcs[nextidx];
    PGXACT *pgxact = &ProcGlobal->allPgXact[nextidx];

       
                                                                        
                                                         
       
    Assert(pgxact->nxids <= THRESHOLD_SUBTRANS_CLOG_OPT);

    TransactionIdSetPageStatusInternal(proc->clogGroupMemberXid, pgxact->nxids, proc->subxids.xids, proc->clogGroupMemberXidStatus, proc->clogGroupMemberLsn, proc->clogGroupMemberPage);

                                    
    nextidx = pg_atomic_read_u32(&proc->clogGroupNext);
  }

                                     
  LWLockRelease(CLogControlLock);

     
                                                                          
                                                                     
              
     
  while (wakeidx != INVALID_PGPROCNO)
  {
    PGPROC *proc = &ProcGlobal->allProcs[wakeidx];

    wakeidx = pg_atomic_read_u32(&proc->clogGroupNext);
    pg_atomic_write_u32(&proc->clogGroupNext, INVALID_PGPROCNO);

                                                                           
    pg_write_barrier();

    proc->clogGroupMember = false;

    if (proc != MyProc)
    {
      PGSemaphoreUnlock(proc->sem);
    }
  }

  return true;
}

   
                                                   
   
                                            
   
static void
TransactionIdSetStatusBit(TransactionId xid, XidStatus status, XLogRecPtr lsn, int slotno)
{
  int byteno = TransactionIdToByte(xid);
  int bshift = TransactionIdToBIndex(xid) * CLOG_BITS_PER_XACT;
  char *byteptr;
  char byteval;
  char curval;

  byteptr = ClogCtl->shared->page_buffer[slotno] + byteno;
  curval = (*byteptr >> bshift) & CLOG_XACT_BITMASK;

     
                                                                          
                                                                             
                                                                           
                                                                        
     
  if (InRecovery && status == TRANSACTION_STATUS_SUB_COMMITTED && curval == TRANSACTION_STATUS_COMMITTED)
  {
    return;
  }

     
                                                                           
                                                                           
     
  Assert(curval == 0 || (curval == TRANSACTION_STATUS_SUB_COMMITTED && status != TRANSACTION_STATUS_IN_PROGRESS) || curval == status);

                                                           
  byteval = *byteptr;
  byteval &= ~(((1 << CLOG_BITS_PER_XACT) - 1) << bshift);
  byteval |= (status << bshift);
  *byteptr = byteval;

     
                                                                       
     
                                                                           
                                                                         
                                                                          
                    
     
  if (!XLogRecPtrIsInvalid(lsn))
  {
    int lsnindex = GetLSNIndex(slotno, xid);

    if (ClogCtl->shared->group_lsn[lsnindex] < lsn)
    {
      ClogCtl->shared->group_lsn[lsnindex] = lsn;
    }
  }
}

   
                                                             
   
                                                                          
                                                                             
                                                                               
                                                                           
                                                                           
                                                                             
                                                                             
                                                                         
   
                                                                        
                                                                             
   
XidStatus
TransactionIdGetStatus(TransactionId xid, XLogRecPtr *lsn)
{
  int pageno = TransactionIdToPage(xid);
  int byteno = TransactionIdToByte(xid);
  int bshift = TransactionIdToBIndex(xid) * CLOG_BITS_PER_XACT;
  int slotno;
  int lsnindex;
  char *byteptr;
  XidStatus status;

                                                      

  slotno = SimpleLruReadPage_ReadOnly(ClogCtl, pageno, xid);
  byteptr = ClogCtl->shared->page_buffer[slotno] + byteno;

  status = (*byteptr >> bshift) & CLOG_XACT_BITMASK;

  lsnindex = GetLSNIndex(slotno, xid);
  *lsn = ClogCtl->shared->group_lsn[lsnindex];

  LWLockRelease(CLogControlLock);

  return status;
}

   
                                  
   
                                                                            
                                                                           
                                                                               
                                                                              
                                
   
                                                                               
                                                                              
                                                                             
                                                                          
                                                                             
                                                         
   
Size
CLOGShmemBuffers(void)
{
  return Min(128, Max(4, NBuffers / 512));
}

   
                                            
   
Size
CLOGShmemSize(void)
{
  return SimpleLruShmemSize(CLOGShmemBuffers(), CLOG_LSNS_PER_PAGE);
}

void
CLOGShmemInit(void)
{
  ClogCtl->PagePrecedes = CLOGPagePrecedes;
  SimpleLruInit(ClogCtl, "clog", CLOGShmemBuffers(), CLOG_LSNS_PER_PAGE, CLogControlLock, "pg_xact", LWTRANCHE_CLOG_BUFFERS);
  SlruPagePrecedesUnitTests(ClogCtl, CLOG_XACTS_PER_PAGE);
}

   
                                                                
                                                                
                                                                 
                    
   
void
BootStrapCLOG(void)
{
  int slotno;

  LWLockAcquire(CLogControlLock, LW_EXCLUSIVE);

                                                        
  slotno = ZeroCLOGPage(0, false);

                                  
  SimpleLruWritePage(ClogCtl, slotno);
  Assert(!ClogCtl->shared->page_dirty[slotno]);

  LWLockRelease(CLogControlLock);
}

   
                                                          
                                                                      
   
                                                                   
                                                
   
                                                                 
   
static int
ZeroCLOGPage(int pageno, bool writeXlog)
{
  int slotno;

  slotno = SimpleLruZeroPage(ClogCtl, pageno);

  if (writeXlog)
  {
    WriteZeroPageXlogRec(pageno);
  }

  return slotno;
}

   
                                                                             
                                                                      
   
void
StartupCLOG(void)
{
  TransactionId xid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  int pageno = TransactionIdToPage(xid);

  LWLockAcquire(CLogControlLock, LW_EXCLUSIVE);

     
                                                    
     
  ClogCtl->shared->latest_page_number = pageno;

  LWLockRelease(CLogControlLock);
}

   
                                                            
   
void
TrimCLOG(void)
{
  TransactionId xid = XidFromFullTransactionId(ShmemVariableCache->nextFullXid);
  int pageno = TransactionIdToPage(xid);

  LWLockAcquire(CLogControlLock, LW_EXCLUSIVE);

     
                                                       
     
  ClogCtl->shared->latest_page_number = pageno;

     
                                                                    
                                                                      
                                                                            
                                                                          
                                                                          
                                                                            
                                                                         
                                                                      
                                                                         
                                                     
     
  if (TransactionIdToPgIndex(xid) != 0)
  {
    int byteno = TransactionIdToByte(xid);
    int bshift = TransactionIdToBIndex(xid) * CLOG_BITS_PER_XACT;
    int slotno;
    char *byteptr;

    slotno = SimpleLruReadPage(ClogCtl, pageno, false, xid);
    byteptr = ClogCtl->shared->page_buffer[slotno] + byteno;

                                                          
    *byteptr &= (1 << bshift) - 1;
                                   
    MemSet(byteptr + 1, 0, BLCKSZ - byteno - 1);

    ClogCtl->shared->page_dirty[slotno] = true;
  }

  LWLockRelease(CLogControlLock);
}

   
                                                                             
   
void
ShutdownCLOG(void)
{
                                      
  TRACE_POSTGRESQL_CLOG_CHECKPOINT_START(false);
  SimpleLruFlush(ClogCtl, false);

     
                                                                           
              
     
  fsync_fname("pg_xact", true);

  TRACE_POSTGRESQL_CLOG_CHECKPOINT_DONE(false);
}

   
                                                                  
   
void
CheckPointCLOG(void)
{
                                      
  TRACE_POSTGRESQL_CLOG_CHECKPOINT_START(true);
  SimpleLruFlush(ClogCtl, true);
  TRACE_POSTGRESQL_CLOG_CHECKPOINT_DONE(true);
}

   
                                                           
   
                                                                            
                                                                           
                                                                           
                     
   
void
ExtendCLOG(TransactionId newestXact)
{
  int pageno;

     
                                                                    
                                                                         
     
  if (TransactionIdToPgIndex(newestXact) != 0 && !TransactionIdEquals(newestXact, FirstNormalTransactionId))
  {
    return;
  }

  pageno = TransactionIdToPage(newestXact);

  LWLockAcquire(CLogControlLock, LW_EXCLUSIVE);

                                                     
  ZeroCLOGPage(pageno, true);

  LWLockRelease(CLogControlLock);
}

   
                                                                             
   
                                                                        
                                                                              
                                                                            
                                                                             
                                                                           
                                                                           
                                                                         
   
                                                                               
                                                                            
                                                                              
   
void
TruncateCLOG(TransactionId oldestXact, Oid oldestxid_datoid)
{
  int cutoffPage;

     
                                                                            
                                                                 
     
  cutoffPage = TransactionIdToPage(oldestXact);

                                                               
  if (!SlruScanDirectory(ClogCtl, SlruScanDirCbReportPresence, &cutoffPage))
  {
    return;                        
  }

     
                                                                             
                                                                          
     
                                                                           
            
     
  AdvanceOldestClogXid(oldestXact);

     
                                                                        
                                                                            
                                                                           
                                                   
     
  WriteTruncateXlogRec(cutoffPage, oldestXact, oldestxid_datoid);

                                                 
  SimpleLruTruncate(ClogCtl, cutoffPage);
}

   
                                                                         
   
                                                                             
                                                                           
                                                                              
                                                                             
                                                           
   
                                                                        
                                                                             
                                                                              
                                                                       
                                                                               
                                                                       
                                                                           
                                  
   
static bool
CLOGPagePrecedes(int page1, int page2)
{
  TransactionId xid1;
  TransactionId xid2;

  xid1 = ((TransactionId)page1) * CLOG_XACTS_PER_PAGE;
  xid1 += FirstNormalTransactionId + 1;
  xid2 = ((TransactionId)page2) * CLOG_XACTS_PER_PAGE;
  xid2 += FirstNormalTransactionId + 1;

  return (TransactionIdPrecedes(xid1, xid2) && TransactionIdPrecedes(xid1, xid2 + CLOG_XACTS_PER_PAGE - 1));
}

   
                                
   
static void
WriteZeroPageXlogRec(int pageno)
{
  XLogBeginInsert();
  XLogRegisterData((char *)(&pageno), sizeof(int));
  (void)XLogInsert(RM_CLOG_ID, CLOG_ZEROPAGE);
}

   
                                
   
                                                                        
                      
   
static void
WriteTruncateXlogRec(int pageno, TransactionId oldestXact, Oid oldestXactDb)
{
  XLogRecPtr recptr;
  xl_clog_truncate xlrec;

  xlrec.pageno = pageno;
  xlrec.oldestXact = oldestXact;
  xlrec.oldestXactDb = oldestXactDb;

  XLogBeginInsert();
  XLogRegisterData((char *)(&xlrec), sizeof(xl_clog_truncate));
  recptr = XLogInsert(RM_CLOG_ID, CLOG_TRUNCATE);
  XLogFlush(recptr);
}

   
                                    
   
void
clog_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                  
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == CLOG_ZEROPAGE)
  {
    int pageno;
    int slotno;

    memcpy(&pageno, XLogRecGetData(record), sizeof(int));

    LWLockAcquire(CLogControlLock, LW_EXCLUSIVE);

    slotno = ZeroCLOGPage(pageno, false);
    SimpleLruWritePage(ClogCtl, slotno);
    Assert(!ClogCtl->shared->page_dirty[slotno]);

    LWLockRelease(CLogControlLock);
  }
  else if (info == CLOG_TRUNCATE)
  {
    xl_clog_truncate xlrec;

    memcpy(&xlrec, XLogRecGetData(record), sizeof(xl_clog_truncate));

       
                                                                         
                                                                      
       
    ClogCtl->shared->latest_page_number = xlrec.pageno;

    AdvanceOldestClogXid(xlrec.oldestXact);

    SimpleLruTruncate(ClogCtl, xlrec.pageno);
  }
  else
  {
    elog(PANIC, "clog_redo: unknown op code %u", info);
  }
}
