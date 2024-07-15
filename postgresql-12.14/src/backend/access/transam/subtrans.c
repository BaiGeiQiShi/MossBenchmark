                                                                            
   
              
                                          
   
                                                                            
                                                                         
                                                                        
                                                                              
                                                                      
                       
   
                                                                 
                                                                           
                                                                            
                                                      
   
                                                                        
                                                                      
                                                
   
                                                                         
                                                                        
   
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/slru.h"
#include "access/subtrans.h"
#include "access/transam.h"
#include "pg_trace.h"
#include "utils/snapmgr.h"

   
                                                                          
                                
   
                                                                           
                                                
                                                                
                                                                               
                                                                              
                                                                               
                            
   

                                 
#define SUBTRANS_XACTS_PER_PAGE (BLCKSZ / sizeof(TransactionId))

#define TransactionIdToPage(xid) ((xid) / (TransactionId)SUBTRANS_XACTS_PER_PAGE)
#define TransactionIdToEntry(xid) ((xid) % (TransactionId)SUBTRANS_XACTS_PER_PAGE)

   
                                                              
   
static SlruCtlData SubTransCtlData;

#define SubTransCtl (&SubTransCtlData)

static int
ZeroSUBTRANSPage(int pageno);
static bool
SubTransPagePrecedes(int page1, int page2);

   
                                                              
   
void
SubTransSetParent(TransactionId xid, TransactionId parent)
{
  int pageno = TransactionIdToPage(xid);
  int entryno = TransactionIdToEntry(xid);
  int slotno;
  TransactionId *ptr;

  Assert(TransactionIdIsValid(parent));
  Assert(TransactionIdFollows(xid, parent));

  LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

  slotno = SimpleLruReadPage(SubTransCtl, pageno, true, xid);
  ptr = (TransactionId *)SubTransCtl->shared->page_buffer[slotno];
  ptr += entryno;

     
                                                                         
                                                                            
                                                  
     
  if (*ptr != parent)
  {
    Assert(*ptr == InvalidTransactionId);
    *ptr = parent;
    SubTransCtl->shared->page_dirty[slotno] = true;
  }

  LWLockRelease(SubtransControlLock);
}

   
                                                                
   
TransactionId
SubTransGetParent(TransactionId xid)
{
  int pageno = TransactionIdToPage(xid);
  int entryno = TransactionIdToEntry(xid);
  int slotno;
  TransactionId *ptr;
  TransactionId parent;

                                                              
  Assert(TransactionIdFollowsOrEquals(xid, TransactionXmin));

                                                
  if (!TransactionIdIsNormal(xid))
  {
    return InvalidTransactionId;
  }

                                                      

  slotno = SimpleLruReadPage_ReadOnly(SubTransCtl, pageno, xid);
  ptr = (TransactionId *)SubTransCtl->shared->page_buffer[slotno];
  ptr += entryno;

  parent = *ptr;

  LWLockRelease(SubtransControlLock);

  return parent;
}

   
                                 
   
                                                                
   
                                                                            
                                                                            
                                                                           
                                                                            
                                                                          
                                                                      
   
TransactionId
SubTransGetTopmostTransaction(TransactionId xid)
{
  TransactionId parentXid = xid, previousXid = xid;

                                                              
  Assert(TransactionIdFollowsOrEquals(xid, TransactionXmin));

  while (TransactionIdIsValid(parentXid))
  {
    previousXid = parentXid;
    if (TransactionIdPrecedes(parentXid, TransactionXmin))
    {
      break;
    }
    parentXid = SubTransGetParent(parentXid);

       
                                                                           
                                                                       
                                                               
       
    if (!TransactionIdPrecedes(parentXid, previousXid))
    {
      elog(ERROR, "pg_subtrans contains invalid entry: xid %u points to parent xid %u", previousXid, parentXid);
    }
  }

  Assert(TransactionIdIsValid(previousXid));

  return previousXid;
}

   
                                                
   
Size
SUBTRANSShmemSize(void)
{
  return SimpleLruShmemSize(NUM_SUBTRANS_BUFFERS, 0);
}

void
SUBTRANSShmemInit(void)
{
  SubTransCtl->PagePrecedes = SubTransPagePrecedes;
  SimpleLruInit(SubTransCtl, "subtrans", NUM_SUBTRANS_BUFFERS, 0, SubtransControlLock, "pg_subtrans", LWTRANCHE_SUBTRANS_BUFFERS);
                                                                 
  SubTransCtl->do_fsync = false;
  SlruPagePrecedesUnitTests(SubTransCtl, SUBTRANS_XACTS_PER_PAGE);
}

   
                                                                
                                                                        
                                                                       
                                   
   
                                                                      
                                                                           
                                                       
   
void
BootStrapSUBTRANS(void)
{
  int slotno;

  LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

                                                          
  slotno = ZeroSUBTRANSPage(0);

                                  
  SimpleLruWritePage(SubTransCtl, slotno);
  Assert(!SubTransCtl->shared->page_dirty[slotno]);

  LWLockRelease(SubtransControlLock);
}

   
                                                              
   
                                                                   
                                                
   
                                                                 
   
static int
ZeroSUBTRANSPage(int pageno)
{
  return SimpleLruZeroPage(SubTransCtl, pageno);
}

   
                                                                             
                                                                      
   
                                                                                 
                      
   
void
StartupSUBTRANS(TransactionId oldestActiveXID)
{
  FullTransactionId nextFullXid;
  int startPage;
  int endPage;

     
                                                                      
                                                                       
                                                                            
                                                                     
     
  LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

  startPage = TransactionIdToPage(oldestActiveXID);
  nextFullXid = ShmemVariableCache->nextFullXid;
  endPage = TransactionIdToPage(XidFromFullTransactionId(nextFullXid));

  while (startPage != endPage)
  {
    (void)ZeroSUBTRANSPage(startPage);
    startPage++;
                                     
    if (startPage > TransactionIdToPage(MaxTransactionId))
    {
      startPage = 0;
    }
  }
  (void)ZeroSUBTRANSPage(startPage);

  LWLockRelease(SubtransControlLock);
}

   
                                                                             
   
void
ShutdownSUBTRANS(void)
{
     
                                        
     
                                                                            
                                   
     
  TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_START(false);
  SimpleLruFlush(SubTransCtl, false);
  TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_DONE(false);
}

   
                                                                  
   
void
CheckPointSUBTRANS(void)
{
     
                                        
     
                                                                            
                                                                          
                                                 
     
  TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_START(true);
  SimpleLruFlush(SubTransCtl, true);
  TRACE_POSTGRESQL_SUBTRANS_CHECKPOINT_DONE(true);
}

   
                                                               
   
                                                                            
                                                                           
                                                                       
                     
   
void
ExtendSUBTRANS(TransactionId newestXact)
{
  int pageno;

     
                                                                    
                                                                         
     
  if (TransactionIdToEntry(newestXact) != 0 && !TransactionIdEquals(newestXact, FirstNormalTransactionId))
  {
    return;
  }

  pageno = TransactionIdToPage(newestXact);

  LWLockAcquire(SubtransControlLock, LW_EXCLUSIVE);

                     
  ZeroSUBTRANSPage(pageno);

  LWLockRelease(SubtransControlLock);
}

   
                                                                                 
   
                                                                              
                                     
   
void
TruncateSUBTRANS(TransactionId oldestXact)
{
  int cutoffPage;

     
                                                                            
                                                                          
                                                                          
                                                                             
                                                                             
                                                                 
     
  TransactionIdRetreat(oldestXact);
  cutoffPage = TransactionIdToPage(oldestXact);

  SimpleLruTruncate(SubTransCtl, cutoffPage);
}

   
                                                                             
                                    
   
static bool
SubTransPagePrecedes(int page1, int page2)
{
  TransactionId xid1;
  TransactionId xid2;

  xid1 = ((TransactionId)page1) * SUBTRANS_XACTS_PER_PAGE;
  xid1 += FirstNormalTransactionId + 1;
  xid2 = ((TransactionId)page2) * SUBTRANS_XACTS_PER_PAGE;
  xid2 += FirstNormalTransactionId + 1;

  return (TransactionIdPrecedes(xid1, xid2) && TransactionIdPrecedes(xid1, xid2 + SUBTRANS_XACTS_PER_PAGE - 1));
}
