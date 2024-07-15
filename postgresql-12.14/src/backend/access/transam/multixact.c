                                                                            
   
               
                                             
   
                                                                              
                                                                          
                                                                           
                                                                        
                                                                              
                                                                            
                                                                   
   
                                                                              
                                                                                
                                                                              
                            
   
                                                                        
                                                                          
                                                                            
                                                                             
                                                                              
                                                                            
                                                                        
   
                                                                               
                                                        
                                                                          
                                                                         
                                                                               
                                                                               
                                                                               
                                                                               
                                                                           
                                                                              
                                                                               
                                                                              
                                                                            
                                                                              
                                                  
   
                                                                        
                                                                             
                                                                            
                                                                          
                                                                       
                                                                           
                                  
   
                                                                            
                                                                               
                                                                                
                                                                               
                                                                            
                                                                        
   
                                                                         
                                                                              
                  
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/multixact.h"
#include "access/slru.h"
#include "access/transam.h"
#include "access/twophase.h"
#include "access/twophase_rmgr.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "funcapi.h"
#include "lib/ilist.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "postmaster/autovacuum.h"
#include "storage/lmgr.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"

   
                                                                            
                                     
   
                                                                             
                                                 
                                                                   
                                                                          
                                                                              
                                                      
                                 
   

                                   
#define MULTIXACT_OFFSETS_PER_PAGE (BLCKSZ / sizeof(MultiXactOffset))

#define MultiXactIdToOffsetPage(xid) ((xid) / (MultiXactOffset)MULTIXACT_OFFSETS_PER_PAGE)
#define MultiXactIdToOffsetEntry(xid) ((xid) % (MultiXactOffset)MULTIXACT_OFFSETS_PER_PAGE)
#define MultiXactIdToOffsetSegment(xid) (MultiXactIdToOffsetPage(xid) / SLRU_PAGES_PER_SEGMENT)

   
                                                                         
                                                                            
                                                                     
                                                                                
                                                                              
                                                                              
                                              
   
                                                                              
                                                    
   
                                                             
#define MXACT_MEMBER_BITS_PER_XACT 8
#define MXACT_MEMBER_FLAGS_PER_BYTE 1
#define MXACT_MEMBER_XACT_BITMASK ((1 << MXACT_MEMBER_BITS_PER_XACT) - 1)

                                                        
#define MULTIXACT_FLAGBYTES_PER_GROUP 4
#define MULTIXACT_MEMBERS_PER_MEMBERGROUP (MULTIXACT_FLAGBYTES_PER_GROUP * MXACT_MEMBER_FLAGS_PER_BYTE)
                                       
#define MULTIXACT_MEMBERGROUP_SIZE (sizeof(TransactionId) * MULTIXACT_MEMBERS_PER_MEMBERGROUP + MULTIXACT_FLAGBYTES_PER_GROUP)
#define MULTIXACT_MEMBERGROUPS_PER_PAGE (BLCKSZ / MULTIXACT_MEMBERGROUP_SIZE)
#define MULTIXACT_MEMBERS_PER_PAGE (MULTIXACT_MEMBERGROUPS_PER_PAGE * MULTIXACT_MEMBERS_PER_MEMBERGROUP)

   
                                                                          
                                                                                
                                                                           
                                                                          
                                                                              
                                          
   
                                                                                
   
#define MAX_MEMBERS_IN_LAST_MEMBERS_PAGE ((uint32)((0xFFFFFFFF % MULTIXACT_MEMBERS_PER_PAGE) + 1))

                                           
#define MXOffsetToMemberPage(xid) ((xid) / (TransactionId)MULTIXACT_MEMBERS_PER_PAGE)
#define MXOffsetToMemberSegment(xid) (MXOffsetToMemberPage(xid) / SLRU_PAGES_PER_SEGMENT)

                                                                        
#define MXOffsetToFlagsOffset(xid) ((((xid) / (TransactionId)MULTIXACT_MEMBERS_PER_MEMBERGROUP) % (TransactionId)MULTIXACT_MEMBERGROUPS_PER_PAGE) * (TransactionId)MULTIXACT_MEMBERGROUP_SIZE)
#define MXOffsetToFlagsBitShift(xid) (((xid) % (TransactionId)MULTIXACT_MEMBERS_PER_MEMBERGROUP) * MXACT_MEMBER_BITS_PER_XACT)

                                                                         
#define MXOffsetToMemberOffset(xid) (MXOffsetToFlagsOffset(xid) + MULTIXACT_FLAGBYTES_PER_GROUP + ((xid) % MULTIXACT_MEMBERS_PER_MEMBERGROUP) * sizeof(TransactionId))

                                              
#define MULTIXACT_MEMBER_SAFE_THRESHOLD (MaxMultiXactOffset / 2)
#define MULTIXACT_MEMBER_DANGER_THRESHOLD (MaxMultiXactOffset - MaxMultiXactOffset / 4)

#define PreviousMultiXactId(xid) ((xid) == FirstMultiXactId ? MaxMultiXactId : (xid)-1)

   
                                                                
   
static SlruCtlData MultiXactOffsetCtlData;
static SlruCtlData MultiXactMemberCtlData;

#define MultiXactOffsetCtl (&MultiXactOffsetCtlData)
#define MultiXactMemberCtl (&MultiXactMemberCtlData)

   
                                                                            
                                                                     
                                                                        
                                                                             
                     
   
typedef struct MultiXactStateData
{
                                       
  MultiXactId nextMXact;

                                  
  MultiXactOffset nextOffset;

                                            
  bool finishedStartup;

     
                                                                          
                                                                         
                        
     
  MultiXactId oldestMultiXactId;
  Oid oldestMultiXactDB;

     
                                                                           
                                                                            
                                                             
     
  MultiXactOffset oldestOffset;
  bool oldestOffsetKnown;

                                            
  MultiXactId multiVacLimit;
  MultiXactId multiWarnLimit;
  MultiXactId multiStopLimit;
  MultiXactId multiWrapLimit;

                                                    
  MultiXactOffset offsetStopLimit;                                 

     
                                                                          
                                                                             
                
     
                                                                             
                                                                            
                                                                        
     
                                                                             
                                                                         
                                                                       
                                                                        
                                                                            
                                                                             
                                                                            
                                                                        
                                            
     
                                                                      
                                                                             
                                                                       
                                                                           
                                                                           
                                                                           
                                                                         
                                                                            
                                                                            
                                                                    
     
                                                                       
                                                                            
                                                                           
                                                                            
                                                                        
                                                                           
                                                                            
                                                                           
                                                                             
                                                                           
                                                                        
                                                                             
                                                                             
                                                                
     
  MultiXactId perBackendXactIds[FLEXIBLE_ARRAY_MEMBER];
} MultiXactStateData;

   
                                                                        
                                                                   
   
#define MaxOldestSlot (MaxBackends + max_prepared_xacts)

                                                 
static MultiXactStateData *MultiXactState;
static MultiXactId *OldestMemberMXactId;
static MultiXactId *OldestVisibleMXactId;

   
                                                        
   
                                                                          
                          
   
                                                                           
                                                                           
                                                                          
                                                                          
                                                                         
                                                                          
                                                                     
   
                                                                        
                                                                      
   
typedef struct mXactCacheEnt
{
  MultiXactId multi;
  int nmembers;
  dlist_node node;
  MultiXactMember members[FLEXIBLE_ARRAY_MEMBER];
} mXactCacheEnt;

#define MAX_CACHE_ENTRIES 256
static dlist_head MXactCache = DLIST_STATIC_INIT(MXactCache);
static int MXactCacheMembers = 0;
static MemoryContext MXactContext = NULL;

#ifdef MULTIXACT_DEBUG
#define debug_elog2(a, b) elog(a, b)
#define debug_elog3(a, b, c) elog(a, b, c)
#define debug_elog4(a, b, c, d) elog(a, b, c, d)
#define debug_elog5(a, b, c, d, e) elog(a, b, c, d, e)
#define debug_elog6(a, b, c, d, e, f) elog(a, b, c, d, e, f)
#else
#define debug_elog2(a, b)
#define debug_elog3(a, b, c)
#define debug_elog4(a, b, c, d)
#define debug_elog5(a, b, c, d, e)
#define debug_elog6(a, b, c, d, e, f)
#endif

                                     
static void
MultiXactIdSetOldestVisible(void);
static void
RecordNewMultiXact(MultiXactId multi, MultiXactOffset offset, int nmembers, MultiXactMember *members);
static MultiXactId
GetNewMultiXactId(int nmembers, MultiXactOffset *offset);

                                
static int
mxactMemberComparator(const void *arg1, const void *arg2);
static MultiXactId
mXactCacheGetBySet(int nmembers, MultiXactMember *members);
static int
mXactCacheGetById(MultiXactId multi, MultiXactMember **members);
static void
mXactCachePut(MultiXactId multi, int nmembers, MultiXactMember *members);

static char *
mxstatus_to_string(MultiXactStatus status);

                                       
static int
ZeroMultiXactOffsetPage(int pageno, bool writeXlog);
static int
ZeroMultiXactMemberPage(int pageno, bool writeXlog);
static bool
MultiXactOffsetPagePrecedes(int page1, int page2);
static bool
MultiXactMemberPagePrecedes(int page1, int page2);
static bool
MultiXactOffsetPrecedes(MultiXactOffset offset1, MultiXactOffset offset2);
static void
ExtendMultiXactOffset(MultiXactId multi);
static void
ExtendMultiXactMember(MultiXactOffset offset, int nmembers);
static bool
MultiXactOffsetWouldWrap(MultiXactOffset boundary, MultiXactOffset start, uint32 distance);
static bool
SetOffsetVacuumLimit(bool is_startup);
static bool
find_multixact_start(MultiXactId multi, MultiXactOffset *result);
static void
WriteMZeroPageXlogRec(int pageno, uint8 info);
static void
WriteMTruncateXlogRec(Oid oldestMultiDB, MultiXactId startOff, MultiXactId endOff, MultiXactOffset startMemb, MultiXactOffset endMemb);

   
                     
                                                             
   
                                                                        
   
                                                                            
                                           
   
MultiXactId
MultiXactIdCreate(TransactionId xid1, MultiXactStatus status1, TransactionId xid2, MultiXactStatus status2)
{
  MultiXactId newMulti;
  MultiXactMember members[2];

  AssertArg(TransactionIdIsValid(xid1));
  AssertArg(TransactionIdIsValid(xid2));

  Assert(!TransactionIdEquals(xid1, xid2) || (status1 != status2));

                                                                   
  Assert(MultiXactIdIsValid(OldestMemberMXactId[MyBackendId]));

     
                                                                             
                                                                            
                                                                
     

  members[0].xid = xid1;
  members[0].status = status1;
  members[1].xid = xid2;
  members[1].status = status2;

  newMulti = MultiXactIdCreateFromMembers(2, members);

  debug_elog3(DEBUG2, "Create: %s", mxid_to_string(newMulti, 2, members));

  return newMulti;
}

   
                     
                                                       
   
                                                                               
                                      
   
                                                                        
                                                                         
                                                                               
                          
   
                                                                            
                                           
   
                                                                              
                                                                              
              
   
MultiXactId
MultiXactIdExpand(MultiXactId multi, TransactionId xid, MultiXactStatus status)
{
  MultiXactId newMulti;
  MultiXactMember *members;
  MultiXactMember *newMembers;
  int nmembers;
  int i;
  int j;

  AssertArg(MultiXactIdIsValid(multi));
  AssertArg(TransactionIdIsValid(xid));

                                                                   
  Assert(MultiXactIdIsValid(OldestMemberMXactId[MyBackendId]));

  debug_elog5(DEBUG2, "Expand: received multi %u, xid %u status %s", multi, xid, mxstatus_to_string(status));

     
                                                                            
                                                                          
              
     
  nmembers = GetMultiXactIdMembers(multi, &members, false, false);

  if (nmembers < 0)
  {
    MultiXactMember member;

       
                                                                     
                                                                        
                                                                        
                                                                           
                                                                          
       
    member.xid = xid;
    member.status = status;
    newMulti = MultiXactIdCreateFromMembers(1, &member);

    debug_elog4(DEBUG2, "Expand: %u has no members, create singleton %u", multi, newMulti);
    return newMulti;
  }

     
                                                                          
                                                        
     
  for (i = 0; i < nmembers; i++)
  {
    if (TransactionIdEquals(members[i].xid, xid) && (members[i].status == status))
    {
      debug_elog4(DEBUG2, "Expand: %u is already a member of %u", xid, multi);
      pfree(members);
      return multi;
    }
  }

     
                                                                    
                                                                         
                                                                             
                                                                          
                                                              
     
                                                                           
                                                     
     
                                                                             
                      
     
  newMembers = (MultiXactMember *)palloc(sizeof(MultiXactMember) * (nmembers + 1));

  for (i = 0, j = 0; i < nmembers; i++)
  {
    if (TransactionIdIsInProgress(members[i].xid) || (ISUPDATE_from_mxstatus(members[i].status) && TransactionIdDidCommit(members[i].xid)))
    {
      newMembers[j].xid = members[i].xid;
      newMembers[j++].status = members[i].status;
    }
  }

  newMembers[j].xid = xid;
  newMembers[j++].status = status;
  newMulti = MultiXactIdCreateFromMembers(j, newMembers);

  pfree(members);
  pfree(newMembers);

  debug_elog3(DEBUG2, "Expand: returning new multi %u", newMulti);

  return newMulti;
}

   
                        
                                                
   
                                                                           
                                                                  
                                                                      
   
                                                                             
                                     
   
bool
MultiXactIdIsRunning(MultiXactId multi, bool isLockOnly)
{
  MultiXactMember *members;
  int nmembers;
  int i;

  debug_elog3(DEBUG2, "IsRunning %u?", multi);

     
                                                                          
                                                             
     
  nmembers = GetMultiXactIdMembers(multi, &members, false, isLockOnly);

  if (nmembers <= 0)
  {
    debug_elog2(DEBUG2, "IsRunning: no members");
    return false;
  }

     
                                                                        
                                                                     
                              
     
                                                                
     
  for (i = 0; i < nmembers; i++)
  {
    if (TransactionIdIsCurrentTransactionId(members[i].xid))
    {
      debug_elog3(DEBUG2, "IsRunning: I (%d) am running!", i);
      pfree(members);
      return true;
    }
  }

     
                                                                             
                                                                          
                                                                        
     
  for (i = 0; i < nmembers; i++)
  {
    if (TransactionIdIsInProgress(members[i].xid))
    {
      debug_elog4(DEBUG2, "IsRunning: member %d (%u) is running", i, members[i].xid);
      pfree(members);
      return true;
    }
  }

  pfree(members);

  debug_elog3(DEBUG2, "IsRunning: %u is not running", multi);

  return false;
}

   
                              
                                                                       
   
                                                                              
                                                                            
                                                                    
                                                                          
                                                           
   
                                                                                
                                                                             
   
void
MultiXactIdSetOldestMember(void)
{
  if (!MultiXactIdIsValid(OldestMemberMXactId[MyBackendId]))
  {
    MultiXactId nextMXact;

       
                                                                   
                                                                         
                                                                          
                                                                    
                                                                       
                                                                          
                                    
       
                                                                          
                                                                         
                                                                       
                              
       
    LWLockAcquire(MultiXactGenLock, LW_SHARED);

       
                                                                     
                                                                           
                                                               
       
    nextMXact = MultiXactState->nextMXact;
    if (nextMXact < FirstMultiXactId)
    {
      nextMXact = FirstMultiXactId;
    }

    OldestMemberMXactId[MyBackendId] = nextMXact;

    LWLockRelease(MultiXactGenLock);

    debug_elog4(DEBUG2, "MultiXact: setting OldestMember[%d] = %u", MyBackendId, nextMXact);
  }
}

   
                               
                                                                          
   
                                                                          
                                                                         
                                                                     
                                                      
   
                                                                             
                                                                           
                                                                         
                                                                              
                                                                           
                                                                    
   
static void
MultiXactIdSetOldestVisible(void)
{
  if (!MultiXactIdIsValid(OldestVisibleMXactId[MyBackendId]))
  {
    MultiXactId oldestMXact;
    int i;

    LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);

       
                                                                     
                                                                           
                                                               
       
    oldestMXact = MultiXactState->nextMXact;
    if (oldestMXact < FirstMultiXactId)
    {
      oldestMXact = FirstMultiXactId;
    }

    for (i = 1; i <= MaxOldestSlot; i++)
    {
      MultiXactId thisoldest = OldestMemberMXactId[i];

      if (MultiXactIdIsValid(thisoldest) && MultiXactIdPrecedes(thisoldest, oldestMXact))
      {
        oldestMXact = thisoldest;
      }
    }

    OldestVisibleMXactId[MyBackendId] = oldestMXact;

    LWLockRelease(MultiXactGenLock);

    debug_elog4(DEBUG2, "MultiXact: setting OldestVisible[%d] = %u", MyBackendId, oldestMXact);
  }
}

   
                       
                                                                      
   
MultiXactId
ReadNextMultiXactId(void)
{
  MultiXactId mxid;

                                                       
  LWLockAcquire(MultiXactGenLock, LW_SHARED);
  mxid = MultiXactState->nextMXact;
  LWLockRelease(MultiXactGenLock);

  if (mxid < FirstMultiXactId)
  {
    mxid = FirstMultiXactId;
  }

  return mxid;
}

   
                                
                                                             
   
                                                                          
                                                                            
   
                                                           
   
MultiXactId
MultiXactIdCreateFromMembers(int nmembers, MultiXactMember *members)
{
  MultiXactId multi;
  MultiXactOffset offset;
  xl_multixact_create xlrec;

  debug_elog3(DEBUG2, "Create: %s", mxid_to_string(InvalidMultiXactId, nmembers, members));

     
                                                                             
                                                                        
                                                                    
                                                                         
                                                                          
                                                                         
                                                                         
                                                  
     
  multi = mXactCacheGetBySet(nmembers, members);
  if (MultiXactIdIsValid(multi))
  {
    debug_elog2(DEBUG2, "Create: in cache!");
    return multi;
  }

                                                                         
  {
    int i;
    bool has_update = false;

    for (i = 0; i < nmembers; i++)
    {
      if (ISUPDATE_from_mxstatus(members[i].status))
      {
        if (has_update)
        {
          elog(ERROR, "new multixact has more than one updating member: %s", mxid_to_string(InvalidMultiXactId, nmembers, members));
        }
        has_update = true;
      }
    }
  }

     
                                                                            
                                                              
                           
     
                                                                           
                                                                         
                                                                             
                                                                            
                                                                           
                                                     
     
  multi = GetNewMultiXactId(nmembers, &offset);

                                                   
  xlrec.mid = multi;
  xlrec.moff = offset;
  xlrec.nmembers = nmembers;

     
                                                                            
                                                                           
                                                                            
                                                   
     
  XLogBeginInsert();
  XLogRegisterData((char *)(&xlrec), SizeOfMultiXactCreate);
  XLogRegisterData((char *)members, nmembers * sizeof(MultiXactMember));

  (void)XLogInsert(RM_MULTIXACT_ID, XLOG_MULTIXACT_CREATE_ID);

                                                                   
  RecordNewMultiXact(multi, offset, nmembers, members);

                                  
  END_CRIT_SECTION();

                                                         
  mXactCachePut(multi, nmembers, members);

  debug_elog2(DEBUG2, "Create: all done");

  return multi;
}

   
                      
                                                                        
   
                                                                              
           
   
static void
RecordNewMultiXact(MultiXactId multi, MultiXactOffset offset, int nmembers, MultiXactMember *members)
{
  int pageno;
  int prev_pageno;
  int entryno;
  int slotno;
  MultiXactOffset *offptr;
  int i;

  LWLockAcquire(MultiXactOffsetControlLock, LW_EXCLUSIVE);

  pageno = MultiXactIdToOffsetPage(multi);
  entryno = MultiXactIdToOffsetEntry(multi);

     
                                                                             
                                                                           
                                                                             
                                                                          
                                                                     
     
  slotno = SimpleLruReadPage(MultiXactOffsetCtl, pageno, true, multi);
  offptr = (MultiXactOffset *)MultiXactOffsetCtl->shared->page_buffer[slotno];
  offptr += entryno;

  *offptr = offset;

  MultiXactOffsetCtl->shared->page_dirty[slotno] = true;

                         
  LWLockRelease(MultiXactOffsetControlLock);

  LWLockAcquire(MultiXactMemberControlLock, LW_EXCLUSIVE);

  prev_pageno = -1;

  for (i = 0; i < nmembers; i++, offset++)
  {
    TransactionId *memberptr;
    uint32 *flagsptr;
    uint32 flagsval;
    int bshift;
    int flagsoff;
    int memberoff;

    Assert(members[i].status <= MultiXactStatusUpdate);

    pageno = MXOffsetToMemberPage(offset);
    memberoff = MXOffsetToMemberOffset(offset);
    flagsoff = MXOffsetToFlagsOffset(offset);
    bshift = MXOffsetToFlagsBitShift(offset);

    if (pageno != prev_pageno)
    {
      slotno = SimpleLruReadPage(MultiXactMemberCtl, pageno, true, multi);
      prev_pageno = pageno;
    }

    memberptr = (TransactionId *)(MultiXactMemberCtl->shared->page_buffer[slotno] + memberoff);

    *memberptr = members[i].xid;

    flagsptr = (uint32 *)(MultiXactMemberCtl->shared->page_buffer[slotno] + flagsoff);

    flagsval = *flagsptr;
    flagsval &= ~(((1 << MXACT_MEMBER_BITS_PER_XACT) - 1) << bshift);
    flagsval |= (members[i].status << bshift);
    *flagsptr = flagsval;

    MultiXactMemberCtl->shared->page_dirty[slotno] = true;
  }

  LWLockRelease(MultiXactMemberControlLock);
}

   
                     
                              
   
                                                                        
                                                                 
   
                                                                              
                                                                            
                                                                               
                                                                   
   
                                                                          
                                                                 
   
static MultiXactId
GetNewMultiXactId(int nmembers, MultiXactOffset *offset)
{
  MultiXactId result;
  MultiXactOffset nextOffset;

  debug_elog3(DEBUG2, "GetNew: for %d xids", nmembers);

                                                                  
  if (RecoveryInProgress())
  {
    elog(ERROR, "cannot assign MultiXactIds during recovery");
  }

  LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);

                                                  
  if (MultiXactState->nextMXact < FirstMultiXactId)
  {
    MultiXactState->nextMXact = FirstMultiXactId;
  }

                       
  result = MultiXactState->nextMXact;

               
                                                                             
                                                                            
                
     
                                                                          
                                                                            
                                              
                                                           
                                                                      
     
                                                                             
               
     
  if (!MultiXactIdPrecedes(result, MultiXactState->multiVacLimit))
  {
       
                                                                    
                                                                          
                                                                 
                                                                       
                                                           
       
    MultiXactId multiWarnLimit = MultiXactState->multiWarnLimit;
    MultiXactId multiStopLimit = MultiXactState->multiStopLimit;
    MultiXactId multiWrapLimit = MultiXactState->multiWrapLimit;
    Oid oldest_datoid = MultiXactState->oldestMultiXactDB;

    LWLockRelease(MultiXactGenLock);

    if (IsUnderPostmaster && !MultiXactIdPrecedes(result, multiStopLimit))
    {
      char *oldest_datname = get_database_name(oldest_datoid);

         
                                                                     
                          
         
      SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);

                                                    
      if (oldest_datname)
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("database is not accepting commands that generate new MultiXactIds to avoid wraparound data loss in database \"%s\"", oldest_datname),
                           errhint("Execute a database-wide VACUUM in that database.\n"
                                   "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("database is not accepting commands that generate new MultiXactIds to avoid wraparound data loss in database with OID %u", oldest_datoid),
                           errhint("Execute a database-wide VACUUM in that database.\n"
                                   "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
    }

       
                                                                           
                                                                     
                                                          
       
    if (IsUnderPostmaster && (result % 65536) == 0)
    {
      SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);
    }

    if (!MultiXactIdPrecedes(result, multiWarnLimit))
    {
      char *oldest_datname = get_database_name(oldest_datoid);

                                                    
      if (oldest_datname)
      {
        ereport(WARNING, (errmsg_plural("database \"%s\" must be vacuumed before %u more MultiXactId is used", "database \"%s\" must be vacuumed before %u more MultiXactIds are used", multiWrapLimit - result, oldest_datname, multiWrapLimit - result), errhint("Execute a database-wide VACUUM in that database.\n"
                                                                                                                                                                                                                                                                   "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
      else
      {
        ereport(WARNING, (errmsg_plural("database with OID %u must be vacuumed before %u more MultiXactId is used", "database with OID %u must be vacuumed before %u more MultiXactIds are used", multiWrapLimit - result, oldest_datoid, multiWrapLimit - result), errhint("Execute a database-wide VACUUM in that database.\n"
                                                                                                                                                                                                                                                                            "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
      }
    }

                                        
    LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);
    result = MultiXactState->nextMXact;
    if (result < FirstMultiXactId)
    {
      result = FirstMultiXactId;
    }
  }

                                                          
  ExtendMultiXactOffset(result);

     
                                                                             
                                                               
                                             
     
  nextOffset = MultiXactState->nextOffset;
  if (nextOffset == 0)
  {
    *offset = 1;
    nmembers++;                                 
  }
  else
  {
    *offset = nextOffset;
  }

               
                                                                    
                      
     
                                                                    
                                                        
     
                                                                            
                           
     
                                                                         
                                                                           
     
                                                                      
                                                                             
             
               
     
#define OFFSET_WARN_SEGMENTS 20
  if (MultiXactState->oldestOffsetKnown && MultiXactOffsetWouldWrap(MultiXactState->offsetStopLimit, nextOffset, nmembers))
  {
                                                                  
    SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);

    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("multixact \"members\" limit exceeded"), errdetail_plural("This command would create a multixact with %u members, but the remaining space is only enough for %u member.", "This command would create a multixact with %u members, but the remaining space is only enough for %u members.", MultiXactState->offsetStopLimit - nextOffset - 1, nmembers, MultiXactState->offsetStopLimit - nextOffset - 1), errhint("Execute a database-wide VACUUM in database with OID %u with reduced vacuum_multixact_freeze_min_age and vacuum_multixact_freeze_table_age settings.", MultiXactState->oldestMultiXactDB)));
  }

     
                                                                             
                                                                           
                                                                            
                                                           
     
  if (!MultiXactState->oldestOffsetKnown || (MultiXactState->nextOffset - MultiXactState->oldestOffset > MULTIXACT_MEMBER_SAFE_THRESHOLD))
  {
       
                                                                           
                                                                   
                                                                          
                                                                
       
    if ((MXOffsetToMemberPage(nextOffset) / SLRU_PAGES_PER_SEGMENT) != (MXOffsetToMemberPage(nextOffset + nmembers) / SLRU_PAGES_PER_SEGMENT))
    {
      SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);
    }
  }

  if (MultiXactState->oldestOffsetKnown && MultiXactOffsetWouldWrap(MultiXactState->offsetStopLimit, nextOffset, nmembers + MULTIXACT_MEMBERS_PER_PAGE * SLRU_PAGES_PER_SEGMENT * OFFSET_WARN_SEGMENTS))
  {
    ereport(WARNING, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg_plural("database with OID %u must be vacuumed before %d more multixact member is used", "database with OID %u must be vacuumed before %d more multixact members are used", MultiXactState->offsetStopLimit - nextOffset + nmembers, MultiXactState->oldestMultiXactDB, MultiXactState->offsetStopLimit - nextOffset + nmembers), errhint("Execute a database-wide VACUUM in that database with reduced vacuum_multixact_freeze_min_age and vacuum_multixact_freeze_table_age settings.")));
  }

  ExtendMultiXactMember(nextOffset, nmembers);

     
                                                                           
                                                                        
                                                                        
                                                                          
                          
     
  START_CRIT_SECTION();

     
                                                                          
                                               
     
                                                                            
                                                                            
                                                                       
                                                                            
                                                                            
                                                            
     
  (MultiXactState->nextMXact)++;

  MultiXactState->nextOffset += nmembers;

  LWLockRelease(MultiXactGenLock);

  debug_elog4(DEBUG2, "GetNew: returning %u offset %u", result, *offset);
  return result;
}

   
                         
                                                                  
   
                                                                         
                                                                        
                                                         
   
                                                                           
                                                                         
                                                                 
                                                                             
                                                                          
                                                                             
                                                                         
                                                                            
                                                  
   
                                                                           
                                                                             
                                              
   
                                                                          
                                                                          
                                                                       
                
   
int
GetMultiXactIdMembers(MultiXactId multi, MultiXactMember **members, bool from_pgupgrade, bool onlyLock)
{
  int pageno;
  int prev_pageno;
  int entryno;
  int slotno;
  MultiXactOffset *offptr;
  MultiXactOffset offset;
  int length;
  int truelength;
  int i;
  MultiXactId oldestMXact;
  MultiXactId nextMXact;
  MultiXactId tmpMXact;
  MultiXactOffset nextOffset;
  MultiXactMember *ptr;

  debug_elog3(DEBUG2, "GetMembers: asked for %u", multi);

  if (!MultiXactIdIsValid(multi) || from_pgupgrade)
  {
    *members = NULL;
    return -1;
  }

                                                    
  length = mXactCacheGetById(multi, members);
  if (length >= 0)
  {
    debug_elog3(DEBUG2, "GetMembers: found %s in the cache", mxid_to_string(multi, length, *members));
    return length;
  }

                                                                 
  MultiXactIdSetOldestVisible();

     
                                                                             
                                                                        
                                                  
     
  if (onlyLock && MultiXactIdPrecedes(multi, OldestVisibleMXactId[MyBackendId]))
  {
    debug_elog2(DEBUG2, "GetMembers: a locker-only multi is too old");
    *members = NULL;
    return -1;
  }

     
                                                                           
     
                                                                           
                                                                         
                                                        
     
                                                                             
                                                                        
            
     
                                                                            
                                                                             
                                                    
     
  LWLockAcquire(MultiXactGenLock, LW_SHARED);

  oldestMXact = MultiXactState->oldestMultiXactId;
  nextMXact = MultiXactState->nextMXact;
  nextOffset = MultiXactState->nextOffset;

  LWLockRelease(MultiXactGenLock);

  if (MultiXactIdPrecedes(multi, oldestMXact))
  {
    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("MultiXactId %u does no longer exist -- apparent wraparound", multi)));
  }

  if (!MultiXactIdPrecedes(multi, nextMXact))
  {
    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("MultiXactId %u has not been created yet -- apparent wraparound", multi)));
  }

     
                                                                            
                                                                             
                                                                          
                                                                  
     
                                                                             
                                                                        
                                    
     
                                                                            
                                                                             
                                                                            
                                                                           
                                                                     
                                                                
                                                                           
                                                                             
                                                        
     
                                                                          
                                                                    
                                                                        
                                                                          
                                                                        
                                                                           
                                                                         
                                                         
     
                                                                             
                                                                            
                                       
     
retry:
  LWLockAcquire(MultiXactOffsetControlLock, LW_EXCLUSIVE);

  pageno = MultiXactIdToOffsetPage(multi);
  entryno = MultiXactIdToOffsetEntry(multi);

  slotno = SimpleLruReadPage(MultiXactOffsetCtl, pageno, true, multi);
  offptr = (MultiXactOffset *)MultiXactOffsetCtl->shared->page_buffer[slotno];
  offptr += entryno;
  offset = *offptr;

  Assert(offset != 0);

     
                                                                        
                                                
     
  tmpMXact = multi + 1;

  if (nextMXact == tmpMXact)
  {
                                                   
    length = nextOffset - offset;
  }
  else
  {
    MultiXactOffset nextMXOffset;

                                     
    if (tmpMXact < FirstMultiXactId)
    {
      tmpMXact = FirstMultiXactId;
    }

    prev_pageno = pageno;

    pageno = MultiXactIdToOffsetPage(tmpMXact);
    entryno = MultiXactIdToOffsetEntry(tmpMXact);

    if (pageno != prev_pageno)
    {
      slotno = SimpleLruReadPage(MultiXactOffsetCtl, pageno, true, tmpMXact);
    }

    offptr = (MultiXactOffset *)MultiXactOffsetCtl->shared->page_buffer[slotno];
    offptr += entryno;
    nextMXOffset = *offptr;

    if (nextMXOffset == 0)
    {
                                                                  
      LWLockRelease(MultiXactOffsetControlLock);
      CHECK_FOR_INTERRUPTS();
      pg_usleep(1000L);
      goto retry;
    }

    length = nextMXOffset - offset;
  }

  LWLockRelease(MultiXactOffsetControlLock);

  ptr = (MultiXactMember *)palloc(length * sizeof(MultiXactMember));

                                       
  LWLockAcquire(MultiXactMemberControlLock, LW_EXCLUSIVE);

  truelength = 0;
  prev_pageno = -1;
  for (i = 0; i < length; i++, offset++)
  {
    TransactionId *xactptr;
    uint32 *flagsptr;
    int flagsoff;
    int bshift;
    int memberoff;

    pageno = MXOffsetToMemberPage(offset);
    memberoff = MXOffsetToMemberOffset(offset);

    if (pageno != prev_pageno)
    {
      slotno = SimpleLruReadPage(MultiXactMemberCtl, pageno, true, multi);
      prev_pageno = pageno;
    }

    xactptr = (TransactionId *)(MultiXactMemberCtl->shared->page_buffer[slotno] + memberoff);

    if (!TransactionIdIsValid(*xactptr))
    {
                                                                 
      Assert(offset == 0);
      continue;
    }

    flagsoff = MXOffsetToFlagsOffset(offset);
    bshift = MXOffsetToFlagsBitShift(offset);
    flagsptr = (uint32 *)(MultiXactMemberCtl->shared->page_buffer[slotno] + flagsoff);

    ptr[truelength].xid = *xactptr;
    ptr[truelength].status = (*flagsptr >> bshift) & MXACT_MEMBER_XACT_BITMASK;
    truelength++;
  }

  LWLockRelease(MultiXactMemberControlLock);

                                                      
  Assert(truelength > 0);

     
                                           
     
  mXactCachePut(multi, truelength, ptr);

  debug_elog3(DEBUG2, "GetMembers: no cache for %s", mxid_to_string(multi, truelength, ptr));
  *members = ptr;
  return truelength;
}

   
                         
                                                  
   
                                                                             
                                                         
   
static int
mxactMemberComparator(const void *arg1, const void *arg2)
{
  MultiXactMember member1 = *(const MultiXactMember *)arg1;
  MultiXactMember member2 = *(const MultiXactMember *)arg2;

  if (member1.xid > member2.xid)
  {
    return 1;
  }
  if (member1.xid < member2.xid)
  {
    return -1;
  }
  if (member1.status > member2.status)
  {
    return 1;
  }
  if (member1.status < member2.status)
  {
    return -1;
  }
  return 0;
}

   
                      
                                                             
                                                             
                  
   
                                                                         
                                                                        
                                                                          
                                    
   
                                                         
   
static MultiXactId
mXactCacheGetBySet(int nmembers, MultiXactMember *members)
{
  dlist_iter iter;

  debug_elog3(DEBUG2, "CacheGet: looking for %s", mxid_to_string(InvalidMultiXactId, nmembers, members));

                                            
  qsort(members, nmembers, sizeof(MultiXactMember), mxactMemberComparator);

  dlist_foreach(iter, &MXactCache)
  {
    mXactCacheEnt *entry = dlist_container(mXactCacheEnt, node, iter.cur);

    if (entry->nmembers != nmembers)
    {
      continue;
    }

       
                                                                           
                            
       
    if (memcmp(members, entry->members, nmembers * sizeof(MultiXactMember)) == 0)
    {
      debug_elog3(DEBUG2, "CacheGet: found %u", entry->multi);
      dlist_move_head(&MXactCache, iter.cur);
      return entry->multi;
    }
  }

  debug_elog2(DEBUG2, "CacheGet: not found :-(");
  return InvalidMultiXactId;
}

   
                     
                                                                   
                                   
   
                                                                        
                                                                              
   
static int
mXactCacheGetById(MultiXactId multi, MultiXactMember **members)
{
  dlist_iter iter;

  debug_elog3(DEBUG2, "CacheGet: looking for %u", multi);

  dlist_foreach(iter, &MXactCache)
  {
    mXactCacheEnt *entry = dlist_container(mXactCacheEnt, node, iter.cur);

    if (entry->multi == multi)
    {
      MultiXactMember *ptr;
      Size size;

      size = sizeof(MultiXactMember) * entry->nmembers;
      ptr = (MultiXactMember *)palloc(size);

      memcpy(ptr, entry->members, size);

      debug_elog3(DEBUG2, "CacheGet: found %s", mxid_to_string(multi, entry->nmembers, entry->members));

         
                                                                        
                                                               
                                 
         
      dlist_move_head(&MXactCache, iter.cur);

      *members = ptr;
      return entry->nmembers;
    }
  }

  debug_elog2(DEBUG2, "CacheGet: not found");
  return -1;
}

   
                 
                                                                      
   
static void
mXactCachePut(MultiXactId multi, int nmembers, MultiXactMember *members)
{
  mXactCacheEnt *entry;

  debug_elog3(DEBUG2, "CachePut: storing %s", mxid_to_string(multi, nmembers, members));

  if (MXactContext == NULL)
  {
                                                                 
    debug_elog2(DEBUG2, "CachePut: initializing memory context");
    MXactContext = AllocSetContextCreate(TopTransactionContext, "MultiXact cache context", ALLOCSET_SMALL_SIZES);
  }

  entry = (mXactCacheEnt *)MemoryContextAlloc(MXactContext, offsetof(mXactCacheEnt, members) + nmembers * sizeof(MultiXactMember));

  entry->multi = multi;
  entry->nmembers = nmembers;
  memcpy(entry->members, members, nmembers * sizeof(MultiXactMember));

                                                                       
  qsort(entry->members, nmembers, sizeof(MultiXactMember), mxactMemberComparator);

  dlist_push_head(&MXactCache, &entry->node);
  if (MXactCacheMembers++ >= MAX_CACHE_ENTRIES)
  {
    dlist_node *node;
    mXactCacheEnt *entry;

    node = dlist_tail_node(&MXactCache);
    dlist_delete(node);
    MXactCacheMembers--;

    entry = dlist_container(mXactCacheEnt, node, node);
    debug_elog3(DEBUG2, "CachePut: pruning cached multi %u", entry->multi);

    pfree(entry);
  }
}

static char *
mxstatus_to_string(MultiXactStatus status)
{
  switch (status)
  {
  case MultiXactStatusForKeyShare:
    return "keysh";
  case MultiXactStatusForShare:
    return "sh";
  case MultiXactStatusForNoKeyUpdate:
    return "fornokeyupd";
  case MultiXactStatusForUpdate:
    return "forupd";
  case MultiXactStatusNoKeyUpdate:
    return "nokeyupd";
  case MultiXactStatusUpdate:
    return "upd";
  default:
    elog(ERROR, "unrecognized multixact status %d", status);
    return "";
  }
}

char *
mxid_to_string(MultiXactId multi, int nmembers, MultiXactMember *members)
{
  static char *str = NULL;
  StringInfoData buf;
  int i;

  if (str != NULL)
  {
    pfree(str);
  }

  initStringInfo(&buf);

  appendStringInfo(&buf, "%u %d[%u (%s)", multi, nmembers, members[0].xid, mxstatus_to_string(members[0].status));

  for (i = 1; i < nmembers; i++)
  {
    appendStringInfo(&buf, ", %u (%s)", members[i].xid, mxstatus_to_string(members[i].status));
  }

  appendStringInfoChar(&buf, ']');
  str = MemoryContextStrdup(TopMemoryContext, buf.data);
  pfree(buf.data);
  return str;
}

   
                      
                                         
   
                                                                            
   
void
AtEOXact_MultiXact(void)
{
     
                                                                            
                                                            
     
                                                                            
                                  
     
  OldestMemberMXactId[MyBackendId] = InvalidMultiXactId;
  OldestVisibleMXactId[MyBackendId] = InvalidMultiXactId;

     
                                                                             
                                                                        
     
  MXactContext = NULL;
  dlist_init(&MXactCache);
  MXactCacheMembers = 0;
}

   
                       
                                                    
   
                                                                               
               
   
void
AtPrepare_MultiXact(void)
{
  MultiXactId myOldestMember = OldestMemberMXactId[MyBackendId];

  if (MultiXactIdIsValid(myOldestMember))
  {
    RegisterTwoPhaseRecord(TWOPHASE_RM_MULTIXACT_ID, 0, &myOldestMember, sizeof(MultiXactId));
  }
}

   
                         
                                                  
   
void
PostPrepare_MultiXact(TransactionId xid)
{
  MultiXactId myOldestMember;

     
                                                                         
                           
     
  myOldestMember = OldestMemberMXactId[MyBackendId];
  if (MultiXactIdIsValid(myOldestMember))
  {
    BackendId dummyBackendId = TwoPhaseGetDummyBackendId(xid, false);

       
                                                                       
                                                                           
                                                                         
                         
       
    LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);

    OldestMemberMXactId[dummyBackendId] = myOldestMember;
    OldestMemberMXactId[MyBackendId] = InvalidMultiXactId;

    LWLockRelease(MultiXactGenLock);
  }

     
                                                                       
                                                                             
               
     
                                                                            
                                  
     
  OldestVisibleMXactId[MyBackendId] = InvalidMultiXactId;

     
                                                                 
     
  MXactContext = NULL;
  dlist_init(&MXactCache);
  MXactCacheMembers = 0;
}

   
                              
                                                           
   
void
multixact_twophase_recover(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  BackendId dummyBackendId = TwoPhaseGetDummyBackendId(xid, false);
  MultiXactId oldestMember;

     
                                                                             
                                                                      
     
  Assert(len == sizeof(MultiXactId));
  oldestMember = *((MultiXactId *)recdata);

  OldestMemberMXactId[dummyBackendId] = oldestMember;
}

   
                                 
                                                       
   
void
multixact_twophase_postcommit(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  BackendId dummyBackendId = TwoPhaseGetDummyBackendId(xid, true);

  Assert(len == sizeof(MultiXactId));

  OldestMemberMXactId[dummyBackendId] = InvalidMultiXactId;
}

   
                                
                                                       
   
void
multixact_twophase_postabort(TransactionId xid, uint16 info, void *recdata, uint32 len)
{
  multixact_twophase_postcommit(xid, info, recdata, len);
}

   
                                                                          
                                                                          
                                                                      
   
Size
MultiXactShmemSize(void)
{
  Size size;

                                                               
#define SHARED_MULTIXACT_STATE_SIZE add_size(offsetof(MultiXactStateData, perBackendXactIds) + sizeof(MultiXactId), mul_size(sizeof(MultiXactId) * 2, MaxOldestSlot))

  size = SHARED_MULTIXACT_STATE_SIZE;
  size = add_size(size, SimpleLruShmemSize(NUM_MXACTOFFSET_BUFFERS, 0));
  size = add_size(size, SimpleLruShmemSize(NUM_MXACTMEMBER_BUFFERS, 0));

  return size;
}

void
MultiXactShmemInit(void)
{
  bool found;

  debug_elog2(DEBUG2, "Shared Memory Init for MultiXact");

  MultiXactOffsetCtl->PagePrecedes = MultiXactOffsetPagePrecedes;
  MultiXactMemberCtl->PagePrecedes = MultiXactMemberPagePrecedes;

  SimpleLruInit(MultiXactOffsetCtl, "multixact_offset", NUM_MXACTOFFSET_BUFFERS, 0, MultiXactOffsetControlLock, "pg_multixact/offsets", LWTRANCHE_MXACTOFFSET_BUFFERS);
  SlruPagePrecedesUnitTests(MultiXactOffsetCtl, MULTIXACT_OFFSETS_PER_PAGE);
  SimpleLruInit(MultiXactMemberCtl, "multixact_member", NUM_MXACTMEMBER_BUFFERS, 0, MultiXactMemberControlLock, "pg_multixact/members", LWTRANCHE_MXACTMEMBER_BUFFERS);
                                                                        

                                          
  MultiXactState = ShmemInitStruct("Shared MultiXact State", SHARED_MULTIXACT_STATE_SIZE, &found);
  if (!IsUnderPostmaster)
  {
    Assert(!found);

                                                     
    MemSet(MultiXactState, 0, SHARED_MULTIXACT_STATE_SIZE);
  }
  else
  {
    Assert(found);
  }

     
                                                                            
                                                               
     
  OldestMemberMXactId = MultiXactState->perBackendXactIds;
  OldestVisibleMXactId = OldestMemberMXactId + MaxOldestSlot;
}

   
                                                                            
                                                                             
                                                                             
   
void
BootStrapMultiXact(void)
{
  int slotno;

  LWLockAcquire(MultiXactOffsetControlLock, LW_EXCLUSIVE);

                                                         
  slotno = ZeroMultiXactOffsetPage(0, false);

                                  
  SimpleLruWritePage(MultiXactOffsetCtl, slotno);
  Assert(!MultiXactOffsetCtl->shared->page_dirty[slotno]);

  LWLockRelease(MultiXactOffsetControlLock);

  LWLockAcquire(MultiXactMemberControlLock, LW_EXCLUSIVE);

                                                         
  slotno = ZeroMultiXactMemberPage(0, false);

                                  
  SimpleLruWritePage(MultiXactMemberCtl, slotno);
  Assert(!MultiXactMemberCtl->shared->page_dirty[slotno]);

  LWLockRelease(MultiXactMemberControlLock);
}

   
                                                                     
                                                                      
   
                                                                   
                                                
   
                                                                 
   
static int
ZeroMultiXactOffsetPage(int pageno, bool writeXlog)
{
  int slotno;

  slotno = SimpleLruZeroPage(MultiXactOffsetCtl, pageno);

  if (writeXlog)
  {
    WriteMZeroPageXlogRec(pageno, XLOG_MULTIXACT_ZERO_OFF_PAGE);
  }

  return slotno;
}

   
                              
   
static int
ZeroMultiXactMemberPage(int pageno, bool writeXlog)
{
  int slotno;

  slotno = SimpleLruZeroPage(MultiXactMemberCtl, pageno);

  if (writeXlog)
  {
    WriteMZeroPageXlogRec(pageno, XLOG_MULTIXACT_ZERO_MEM_PAGE);
  }

  return slotno;
}

   
                         
                                               
   
                                                                                
                                                                              
                                                                           
                                                                               
                                                                             
                                                                            
                                                                             
                                                                              
                                                                             
               
   
static void
MaybeExtendOffsetSlru(void)
{
  int pageno;

  pageno = MultiXactIdToOffsetPage(MultiXactState->nextMXact);

  LWLockAcquire(MultiXactOffsetControlLock, LW_EXCLUSIVE);

  if (!SimpleLruDoesPhysicalPageExist(MultiXactOffsetCtl, pageno))
  {
    int slotno;

       
                                                                          
                                                                          
                                               
       
    slotno = ZeroMultiXactOffsetPage(pageno, false);
    SimpleLruWritePage(MultiXactOffsetCtl, slotno);
  }

  LWLockRelease(MultiXactOffsetControlLock);
}

   
                                                                             
   
                                                                       
                                                                               
                                                                          
                 
   
void
StartupMultiXact(void)
{
  MultiXactId multi = MultiXactState->nextMXact;
  MultiXactOffset offset = MultiXactState->nextOffset;
  int pageno;

     
                                                         
     
  pageno = MultiXactIdToOffsetPage(multi);
  MultiXactOffsetCtl->shared->latest_page_number = pageno;

     
                                                         
     
  pageno = MXOffsetToMemberPage(offset);
  MultiXactMemberCtl->shared->latest_page_number = pageno;
}

   
                                                            
   
void
TrimMultiXact(void)
{
  MultiXactId nextMXact;
  MultiXactOffset offset;
  MultiXactId oldestMXact;
  Oid oldestMXactDB;
  int pageno;
  int entryno;
  int flagsoff;

  LWLockAcquire(MultiXactGenLock, LW_SHARED);
  nextMXact = MultiXactState->nextMXact;
  offset = MultiXactState->nextOffset;
  oldestMXact = MultiXactState->oldestMultiXactId;
  oldestMXactDB = MultiXactState->oldestMultiXactDB;
  LWLockRelease(MultiXactGenLock);

                              
  LWLockAcquire(MultiXactOffsetControlLock, LW_EXCLUSIVE);

     
                                                                     
     
  pageno = MultiXactIdToOffsetPage(nextMXact);
  MultiXactOffsetCtl->shared->latest_page_number = pageno;

     
                                                                       
                                                                           
                                                                             
                                                                             
                                                                             
                        
     
  entryno = MultiXactIdToOffsetEntry(nextMXact);
  if (entryno != 0)
  {
    int slotno;
    MultiXactOffset *offptr;

    slotno = SimpleLruReadPage(MultiXactOffsetCtl, pageno, true, nextMXact);
    offptr = (MultiXactOffset *)MultiXactOffsetCtl->shared->page_buffer[slotno];
    offptr += entryno;

    MemSet(offptr, 0, BLCKSZ - (entryno * sizeof(MultiXactOffset)));

    MultiXactOffsetCtl->shared->page_dirty[slotno] = true;
  }

  LWLockRelease(MultiXactOffsetControlLock);

                                
  LWLockAcquire(MultiXactMemberControlLock, LW_EXCLUSIVE);

     
                                                                     
     
  pageno = MXOffsetToMemberPage(offset);
  MultiXactMemberCtl->shared->latest_page_number = pageno;

     
                                                                       
                                
     
  flagsoff = MXOffsetToFlagsOffset(offset);
  if (flagsoff != 0)
  {
    int slotno;
    TransactionId *xidptr;
    int memberoff;

    memberoff = MXOffsetToMemberOffset(offset);
    slotno = SimpleLruReadPage(MultiXactMemberCtl, pageno, true, offset);
    xidptr = (TransactionId *)(MultiXactMemberCtl->shared->page_buffer[slotno] + memberoff);

    MemSet(xidptr, 0, BLCKSZ - memberoff);

       
                                                                      
                                                                          
                
       

    MultiXactMemberCtl->shared->page_dirty[slotno] = true;
  }

  LWLockRelease(MultiXactMemberControlLock);

                                       
  LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);
  MultiXactState->finishedStartup = true;
  LWLockRelease(MultiXactGenLock);

                                                                
  SetMultiXactIdLimit(oldestMXact, oldestMXactDB, true);
}

   
                                                                             
   
void
ShutdownMultiXact(void)
{
                                           
  TRACE_POSTGRESQL_MULTIXACT_CHECKPOINT_START(false);
  SimpleLruFlush(MultiXactOffsetCtl, false);
  SimpleLruFlush(MultiXactMemberCtl, false);
  TRACE_POSTGRESQL_MULTIXACT_CHECKPOINT_DONE(false);
}

   
                                                         
   
void
MultiXactGetCheckptMulti(bool is_shutdown, MultiXactId *nextMulti, MultiXactOffset *nextMultiOffset, MultiXactId *oldestMulti, Oid *oldestMultiDB)
{
  LWLockAcquire(MultiXactGenLock, LW_SHARED);
  *nextMulti = MultiXactState->nextMXact;
  *nextMultiOffset = MultiXactState->nextOffset;
  *oldestMulti = MultiXactState->oldestMultiXactId;
  *oldestMultiDB = MultiXactState->oldestMultiXactDB;
  LWLockRelease(MultiXactGenLock);

  debug_elog6(DEBUG2, "MultiXact: checkpoint is nextMulti %u, nextOffset %u, oldestMulti %u in DB %u", *nextMulti, *nextMultiOffset, *oldestMulti, *oldestMultiDB);
}

   
                                                                  
   
void
CheckPointMultiXact(void)
{
  TRACE_POSTGRESQL_MULTIXACT_CHECKPOINT_START(true);

                                           
  SimpleLruFlush(MultiXactOffsetCtl, true);
  SimpleLruFlush(MultiXactMemberCtl, true);

  TRACE_POSTGRESQL_MULTIXACT_CHECKPOINT_DONE(true);
}

   
                                                      
   
                                                                         
                                                                            
                                                                          
                         
   
void
MultiXactSetNextMXact(MultiXactId nextMulti, MultiXactOffset nextMultiOffset)
{
  debug_elog4(DEBUG2, "MultiXact: setting next multi to %u offset %u", nextMulti, nextMultiOffset);
  LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);
  MultiXactState->nextMXact = nextMulti;
  MultiXactState->nextOffset = nextMultiOffset;
  LWLockRelease(MultiXactGenLock);

     
                                                                       
                                                             
     
                                                                        
                                                                           
                                                                      
                                                                        
                                                                   
                                  
     
  if (IsBinaryUpgrade)
  {
    MaybeExtendOffsetSlru();
  }
}

   
                                                                              
                                                                           
                                                                        
   
                                                                           
                                                                             
   
void
SetMultiXactIdLimit(MultiXactId oldest_datminmxid, Oid oldest_datoid, bool is_startup)
{
  MultiXactId multiVacLimit;
  MultiXactId multiWarnLimit;
  MultiXactId multiStopLimit;
  MultiXactId multiWrapLimit;
  MultiXactId curMulti;
  bool needs_offset_vacuum;

  Assert(MultiXactIdIsValid(oldest_datminmxid));

     
                                                                         
                                                                            
                                                                         
                                                                          
                                                                       
     
  multiWrapLimit = oldest_datminmxid + (MaxMultiXactId >> 1);
  if (multiWrapLimit < FirstMultiXactId)
  {
    multiWrapLimit += FirstMultiXactId;
  }

     
                                                                            
                         
     
                                                      
                                                                         
                                                                      
     
  multiStopLimit = multiWrapLimit - 100;
  if (multiStopLimit < FirstMultiXactId)
  {
    multiStopLimit -= FirstMultiXactId;
  }

     
                                                                         
                                                                            
                                                                            
                                                                           
                                                                             
                                                                     
                                                                           
                                                          
     
  multiWarnLimit = multiStopLimit - 10000000;
  if (multiWarnLimit < FirstMultiXactId)
  {
    multiWarnLimit -= FirstMultiXactId;
  }

     
                                                                            
                                                                 
     
                                                                             
                                                                             
                                            
     
  multiVacLimit = oldest_datminmxid + autovacuum_multixact_freeze_max_age;
  if (multiVacLimit < FirstMultiXactId)
  {
    multiVacLimit += FirstMultiXactId;
  }

                                                                  
  LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);
  MultiXactState->oldestMultiXactId = oldest_datminmxid;
  MultiXactState->oldestMultiXactDB = oldest_datoid;
  MultiXactState->multiVacLimit = multiVacLimit;
  MultiXactState->multiWarnLimit = multiWarnLimit;
  MultiXactState->multiStopLimit = multiStopLimit;
  MultiXactState->multiWrapLimit = multiWrapLimit;
  curMulti = MultiXactState->nextMXact;
  LWLockRelease(MultiXactGenLock);

                    
  ereport(DEBUG1, (errmsg("MultiXactId wrap limit is %u, limited by database with OID %u", multiWrapLimit, oldest_datoid)));

     
                                                                             
                                                                        
                                                                       
                                                                            
                                                   
     
  if (!MultiXactState->finishedStartup)
  {
    return;
  }

  Assert(!InRecovery);

                                     
  needs_offset_vacuum = SetOffsetVacuumLimit(is_startup);

     
                                                                       
                                                                      
                                                                         
                                                                         
                                                                         
     
  if ((MultiXactIdPrecedes(multiVacLimit, curMulti) || needs_offset_vacuum) && IsUnderPostmaster)
  {
    SendPostmasterSignal(PMSIGNAL_START_AUTOVAC_LAUNCHER);
  }

                                                             
  if (MultiXactIdPrecedes(multiWarnLimit, curMulti))
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
      ereport(WARNING, (errmsg_plural("database \"%s\" must be vacuumed before %u more MultiXactId is used", "database \"%s\" must be vacuumed before %u more MultiXactIds are used", multiWrapLimit - curMulti, oldest_datname, multiWrapLimit - curMulti), errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
                                                                                                                                                                                                                                                                     "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
    }
    else
    {
      ereport(WARNING, (errmsg_plural("database with OID %u must be vacuumed before %u more MultiXactId is used", "database with OID %u must be vacuumed before %u more MultiXactIds are used", multiWrapLimit - curMulti, oldest_datoid, multiWrapLimit - curMulti), errhint("To avoid a database shutdown, execute a database-wide VACUUM in that database.\n"
                                                                                                                                                                                                                                                                              "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
    }
  }
}

   
                                                                    
                                                        
   
                                                                       
                                                                         
                                                                             
                                                      
   
void
MultiXactAdvanceNextMXact(MultiXactId minMulti, MultiXactOffset minMultiOffset)
{
  LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);
  if (MultiXactIdPrecedes(MultiXactState->nextMXact, minMulti))
  {
    debug_elog3(DEBUG2, "MultiXact: setting next multi to %u", minMulti);
    MultiXactState->nextMXact = minMulti;
  }
  if (MultiXactOffsetPrecedes(MultiXactState->nextOffset, minMultiOffset))
  {
    debug_elog3(DEBUG2, "MultiXact: setting next offset to %u", minMultiOffset);
    MultiXactState->nextOffset = minMultiOffset;
  }
  LWLockRelease(MultiXactGenLock);
}

   
                                                                              
           
   
                                              
   
void
MultiXactAdvanceOldest(MultiXactId oldestMulti, Oid oldestMultiDB)
{
  Assert(InRecovery);

  if (MultiXactIdPrecedes(MultiXactState->oldestMultiXactId, oldestMulti))
  {
    SetMultiXactIdLimit(oldestMulti, oldestMultiDB, false);
  }
}

   
                                                                              
   
                                                                             
                                                                         
                                                                            
                          
   
static void
ExtendMultiXactOffset(MultiXactId multi)
{
  int pageno;

     
                                                                            
                                                                         
     
  if (MultiXactIdToOffsetEntry(multi) != 0 && multi != FirstMultiXactId)
  {
    return;
  }

  pageno = MultiXactIdToOffsetPage(multi);

  LWLockAcquire(MultiXactOffsetControlLock, LW_EXCLUSIVE);

                                                     
  ZeroMultiXactOffsetPage(pageno, true);

  LWLockRelease(MultiXactOffsetControlLock);
}

   
                                                                       
                          
   
                                                                          
                        
   
static void
ExtendMultiXactMember(MultiXactOffset offset, int nmembers)
{
     
                                                                           
                                                                          
                                                                       
                                     
     
  while (nmembers > 0)
  {
    int flagsoff;
    int flagsbit;
    uint32 difference;

       
                                                
       
    flagsoff = MXOffsetToFlagsOffset(offset);
    flagsbit = MXOffsetToFlagsBitShift(offset);
    if (flagsoff == 0 && flagsbit == 0)
    {
      int pageno;

      pageno = MXOffsetToMemberPage(offset);

      LWLockAcquire(MultiXactMemberControlLock, LW_EXCLUSIVE);

                                                         
      ZeroMultiXactMemberPage(pageno, true);

      LWLockRelease(MultiXactMemberControlLock);
    }

       
                                                                          
                                                                         
                                                                           
                                                       
       
    if (offset + MAX_MEMBERS_IN_LAST_MEMBERS_PAGE < offset)
    {
         
                                                                       
                                                               
                     
         
      difference = MaxMultiXactOffset - offset + 1;
    }
    else
    {
      difference = MULTIXACT_MEMBERS_PER_PAGE - offset % MULTIXACT_MEMBERS_PER_PAGE;
    }

       
                                                                           
                                            
       
    nmembers -= difference;
    offset += difference;
  }
}

   
                        
   
                                                                             
                                                                               
                                               
   
                                                                              
                                                                            
                                                                       
   
MultiXactId
GetOldestMultiXactId(void)
{
  MultiXactId oldestMXact;
  MultiXactId nextMXact;
  int i;

     
                                                                            
                                                                     
     
  LWLockAcquire(MultiXactGenLock, LW_SHARED);

     
                                                                   
                                                                         
                                                           
     
  nextMXact = MultiXactState->nextMXact;
  if (nextMXact < FirstMultiXactId)
  {
    nextMXact = FirstMultiXactId;
  }

  oldestMXact = nextMXact;
  for (i = 1; i <= MaxOldestSlot; i++)
  {
    MultiXactId thisoldest;

    thisoldest = OldestMemberMXactId[i];
    if (MultiXactIdIsValid(thisoldest) && MultiXactIdPrecedes(thisoldest, oldestMXact))
    {
      oldestMXact = thisoldest;
    }
    thisoldest = OldestVisibleMXactId[i];
    if (MultiXactIdIsValid(thisoldest) && MultiXactIdPrecedes(thisoldest, oldestMXact))
    {
      oldestMXact = thisoldest;
    }
  }

  LWLockRelease(MultiXactGenLock);

  return oldestMXact;
}

   
                                                                           
               
   
                                                                            
                                                                               
                             
   
                                                                          
              
   
static bool
SetOffsetVacuumLimit(bool is_startup)
{
  MultiXactId oldestMultiXactId;
  MultiXactId nextMXact;
  MultiXactOffset oldestOffset = 0;                       
  MultiXactOffset prevOldestOffset;
  MultiXactOffset nextOffset;
  bool oldestOffsetKnown = false;
  bool prevOldestOffsetKnown;
  MultiXactOffset offsetStopLimit = 0;
  MultiXactOffset prevOffsetStopLimit;

     
                                                                          
                                                                       
     
  LWLockAcquire(MultiXactTruncationLock, LW_SHARED);

                                                
  LWLockAcquire(MultiXactGenLock, LW_SHARED);
  oldestMultiXactId = MultiXactState->oldestMultiXactId;
  nextMXact = MultiXactState->nextMXact;
  nextOffset = MultiXactState->nextOffset;
  prevOldestOffsetKnown = MultiXactState->oldestOffsetKnown;
  prevOldestOffset = MultiXactState->oldestOffset;
  prevOffsetStopLimit = MultiXactState->offsetStopLimit;
  Assert(MultiXactState->finishedStartup);
  LWLockRelease(MultiXactGenLock);

     
                                                                          
                                                                            
                                                                       
                                                                           
                                                           
     
  if (oldestMultiXactId == nextMXact)
  {
       
                                                                           
               
       
    oldestOffset = nextOffset;
    oldestOffsetKnown = true;
  }
  else
  {
       
                                                                    
                                                                           
                                                                         
                                         
       
    oldestOffsetKnown = find_multixact_start(oldestMultiXactId, &oldestOffset);

    if (oldestOffsetKnown)
    {
      ereport(DEBUG1, (errmsg("oldest MultiXactId member is at offset %u", oldestOffset)));
    }
    else
    {
      ereport(LOG, (errmsg("MultiXact member wraparound protections are disabled because oldest checkpointed MultiXact %u does not exist on disk", oldestMultiXactId)));
    }
  }

  LWLockRelease(MultiXactTruncationLock);

     
                                                                            
                                                                            
                                    
     
  if (oldestOffsetKnown)
  {
                                                         
    offsetStopLimit = oldestOffset - (oldestOffset % (MULTIXACT_MEMBERS_PER_PAGE * SLRU_PAGES_PER_SEGMENT));

                                                              
    offsetStopLimit -= (MULTIXACT_MEMBERS_PER_PAGE * SLRU_PAGES_PER_SEGMENT);

    if (!prevOldestOffsetKnown && !is_startup)
    {
      ereport(LOG, (errmsg("MultiXact member wraparound protections are now enabled")));
    }

    ereport(DEBUG1, (errmsg("MultiXact member stop limit is now %u based on MultiXact %u", offsetStopLimit, oldestMultiXactId)));
  }
  else if (prevOldestOffsetKnown)
  {
       
                                                                      
                                                                     
                                                                        
                    
       
    oldestOffset = prevOldestOffset;
    oldestOffsetKnown = true;
    offsetStopLimit = prevOffsetStopLimit;
  }

                                   
  LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);
  MultiXactState->oldestOffset = oldestOffset;
  MultiXactState->oldestOffsetKnown = oldestOffsetKnown;
  MultiXactState->offsetStopLimit = offsetStopLimit;
  LWLockRelease(MultiXactGenLock);

     
                                                                        
     
  return !oldestOffsetKnown || (nextOffset - oldestOffset > MULTIXACT_MEMBER_SAFE_THRESHOLD);
}

   
                                                                           
   
                                                                          
                                                                                
                                                                              
                                                                      
              
   
static bool
MultiXactOffsetWouldWrap(MultiXactOffset boundary, MultiXactOffset start, uint32 distance)
{
  MultiXactOffset finish;

     
                                                                           
                                                                          
     
  finish = start + distance;
  if (finish < start)
  {
    finish++;
  }

                                                                            
                                                                           
                                                       
     
                      
                                                
                               
                                   
     
                                                                             
                                                                         
                          
     
                      
                                                                 
                                                 
                                
                                                                            
     
  if (start < boundary)
  {
    return finish >= boundary || finish < start;
  }
  else
  {
    return finish >= boundary && finish < start;
  }
}

   
                                                      
   
                                                                          
                                                                           
   
                                                                      
                                                     
   
static bool
find_multixact_start(MultiXactId multi, MultiXactOffset *result)
{
  MultiXactOffset offset;
  int pageno;
  int entryno;
  int slotno;
  MultiXactOffset *offptr;

  Assert(MultiXactState->finishedStartup);

  pageno = MultiXactIdToOffsetPage(multi);
  entryno = MultiXactIdToOffsetEntry(multi);

     
                                                                     
                                                                         
                                                                             
                                                                         
                             
     
  SimpleLruFlush(MultiXactOffsetCtl, true);
  SimpleLruFlush(MultiXactMemberCtl, true);

  if (!SimpleLruDoesPhysicalPageExist(MultiXactOffsetCtl, pageno))
  {
    return false;
  }

                                                      
  slotno = SimpleLruReadPage_ReadOnly(MultiXactOffsetCtl, pageno, multi);
  offptr = (MultiXactOffset *)MultiXactOffsetCtl->shared->page_buffer[slotno];
  offptr += entryno;
  offset = *offptr;
  LWLockRelease(MultiXactOffsetControlLock);

  *result = offset;
  return true;
}

   
                                                                            
                                                
   
static bool
ReadMultiXactCounts(uint32 *multixacts, MultiXactOffset *members)
{
  MultiXactOffset nextOffset;
  MultiXactOffset oldestOffset;
  MultiXactId oldestMultiXactId;
  MultiXactId nextMultiXactId;
  bool oldestOffsetKnown;

  LWLockAcquire(MultiXactGenLock, LW_SHARED);
  nextOffset = MultiXactState->nextOffset;
  oldestMultiXactId = MultiXactState->oldestMultiXactId;
  nextMultiXactId = MultiXactState->nextMXact;
  oldestOffset = MultiXactState->oldestOffset;
  oldestOffsetKnown = MultiXactState->oldestOffsetKnown;
  LWLockRelease(MultiXactGenLock);

  if (!oldestOffsetKnown)
  {
    return false;
  }

  *members = nextOffset - oldestOffset;
  *multixacts = nextMultiXactId - oldestMultiXactId;
  return true;
}

   
                                                                           
                                                                             
                                                                              
                                                                           
                                                                        
                                                                            
                                                                             
                                                                           
                                                                          
                  
   
                                                                             
                                                                       
                                                                            
                                                                           
                                                                              
                 
   
                                                                         
                                                                            
                                                                      
                                                                        
                                                                  
                                                                            
                                                                         
                                             
   
                                                                           
                      
   
int
MultiXactMemberFreezeThreshold(void)
{
  MultiXactOffset members;
  uint32 multixacts;
  uint32 victim_multixacts;
  double fraction;

                                                                         
  if (!ReadMultiXactCounts(&multixacts, &members))
  {
    return 0;
  }

                                                                          
  if (members <= MULTIXACT_MEMBER_SAFE_THRESHOLD)
  {
    return autovacuum_multixact_freeze_max_age;
  }

     
                                                                            
                                                                         
                                      
     
  fraction = (double)(members - MULTIXACT_MEMBER_SAFE_THRESHOLD) / (MULTIXACT_MEMBER_DANGER_THRESHOLD - MULTIXACT_MEMBER_SAFE_THRESHOLD);
  victim_multixacts = multixacts * fraction;

                                                                       
  if (victim_multixacts > multixacts)
  {
    return 0;
  }
  return multixacts - victim_multixacts;
}

typedef struct mxtruncinfo
{
  int earliestExistingPage;
} mxtruncinfo;

   
                              
                                                                
   
static bool
SlruScanDirCbFindEarliest(SlruCtl ctl, char *filename, int segpage, void *data)
{
  mxtruncinfo *trunc = (mxtruncinfo *)data;

  if (trunc->earliestExistingPage == -1 || ctl->PagePrecedes(segpage, trunc->earliestExistingPage))
  {
    trunc->earliestExistingPage = segpage;
  }

  return false;                 
}

   
                                               
   
                                                                             
                                                                             
                                                                        
                                   
   
static void
PerformMembersTruncation(MultiXactOffset oldestOffset, MultiXactOffset newOldestOffset)
{
  const int maxsegment = MXOffsetToMemberSegment(MaxMultiXactOffset);
  int startsegment = MXOffsetToMemberSegment(oldestOffset);
  int endsegment = MXOffsetToMemberSegment(newOldestOffset);
  int segment = startsegment;

     
                                                                          
                                              
     
  while (segment != endsegment)
  {
    elog(DEBUG2, "truncating multixact members segment %x", segment);
    SlruDeleteSegment(MultiXactMemberCtl, segment);

                                                             
    if (segment == maxsegment)
    {
      segment = 0;
    }
    else
    {
      segment += 1;
    }
  }
}

   
                                               
   
static void
PerformOffsetsTruncation(MultiXactId oldestMulti, MultiXactId newOldestMulti)
{
     
                                                                           
                                                                           
                                                                       
                                                                      
                
     
  SimpleLruTruncate(MultiXactOffsetCtl, MultiXactIdToOffsetPage(PreviousMultiXactId(newOldestMulti)));
}

   
                                                                             
                           
   
                                                           
                                                                         
                                       
   
                                                                               
                                                                      
   
void
TruncateMultiXact(MultiXactId newOldestMulti, Oid newOldestMultiDB)
{
  MultiXactId oldestMulti;
  MultiXactId nextMulti;
  MultiXactOffset newOldestOffset;
  MultiXactOffset oldestOffset;
  MultiXactOffset nextOffset;
  mxtruncinfo trunc;
  MultiXactId earliest;

  Assert(!RecoveryInProgress());
  Assert(MultiXactState->finishedStartup);

     
                                                                            
                                                                           
                                                                             
                                                                    
     
  LWLockAcquire(MultiXactTruncationLock, LW_EXCLUSIVE);

  LWLockAcquire(MultiXactGenLock, LW_SHARED);
  nextMulti = MultiXactState->nextMXact;
  nextOffset = MultiXactState->nextOffset;
  oldestMulti = MultiXactState->oldestMultiXactId;
  LWLockRelease(MultiXactGenLock);
  Assert(MultiXactIdIsValid(oldestMulti));

     
                                                                        
                                                                           
                                                            
     
  if (MultiXactIdPrecedesOrEquals(newOldestMulti, oldestMulti))
  {
    LWLockRelease(MultiXactTruncationLock);
    return;
  }

     
                                                                           
                                                                          
                                                                     
                                                                             
                                                                      
     
                                                                       
                                                                          
                                                                      
                                                                            
                                                                            
                                                                    
                                                                       
     
                                                                             
                                                                      
     
  trunc.earliestExistingPage = -1;
  SlruScanDirectory(MultiXactOffsetCtl, SlruScanDirCbFindEarliest, &trunc);
  earliest = trunc.earliestExistingPage * MULTIXACT_OFFSETS_PER_PAGE;
  if (earliest < FirstMultiXactId)
  {
    earliest = FirstMultiXactId;
  }

                                                            
  if (MultiXactIdPrecedes(oldestMulti, earliest))
  {
    LWLockRelease(MultiXactTruncationLock);
    return;
  }

     
                                                                           
                                                  
     
                                                                          
                                                                             
                                                                  
     
  if (oldestMulti == nextMulti)
  {
                                 
    oldestOffset = nextOffset;
  }
  else if (!find_multixact_start(oldestMulti, &oldestOffset))
  {
    ereport(LOG, (errmsg("oldest MultiXact %u not found, earliest MultiXact %u, skipping truncation", oldestMulti, earliest)));
    LWLockRelease(MultiXactTruncationLock);
    return;
  }

     
                                                                        
                                                
     
  if (newOldestMulti == nextMulti)
  {
                                 
    newOldestOffset = nextOffset;
  }
  else if (!find_multixact_start(newOldestMulti, &newOldestOffset))
  {
    ereport(LOG, (errmsg("cannot truncate up to MultiXact %u because it does not exist on disk, skipping truncation", newOldestMulti)));
    LWLockRelease(MultiXactTruncationLock);
    return;
  }

  elog(DEBUG1,
      "performing multixact truncation: "
      "offsets [%u, %u), offsets segments [%x, %x), "
      "members [%u, %u), members segments [%x, %x)",
      oldestMulti, newOldestMulti, MultiXactIdToOffsetSegment(oldestMulti), MultiXactIdToOffsetSegment(newOldestMulti), oldestOffset, newOldestOffset, MXOffsetToMemberSegment(oldestOffset), MXOffsetToMemberSegment(newOldestOffset));

     
                                                                         
                                                                            
                                                                           
                                        
     
  START_CRIT_SECTION();

     
                                                                             
                                                                         
                                                                         
                 
     
  Assert(!MyPgXact->delayChkpt);
  MyPgXact->delayChkpt = true;

                          
  WriteMTruncateXlogRec(newOldestMultiDB, oldestMulti, newOldestMulti, oldestOffset, newOldestOffset);

     
                                                                            
                                                                       
                                                                       
                                                                          
                                                                          
                                 
     
  LWLockAcquire(MultiXactGenLock, LW_EXCLUSIVE);
  MultiXactState->oldestMultiXactId = newOldestMulti;
  MultiXactState->oldestMultiXactDB = newOldestMultiDB;
  LWLockRelease(MultiXactGenLock);

                              
  PerformMembersTruncation(oldestOffset, newOldestOffset);

                    
  PerformOffsetsTruncation(oldestMulti, newOldestMulti);

  MyPgXact->delayChkpt = false;

  END_CRIT_SECTION();
  LWLockRelease(MultiXactTruncationLock);
}

   
                                                                          
                                               
   
                                                                        
                           
   
static bool
MultiXactOffsetPagePrecedes(int page1, int page2)
{
  MultiXactId multi1;
  MultiXactId multi2;

  multi1 = ((MultiXactId)page1) * MULTIXACT_OFFSETS_PER_PAGE;
  multi1 += FirstMultiXactId + 1;
  multi2 = ((MultiXactId)page2) * MULTIXACT_OFFSETS_PER_PAGE;
  multi2 += FirstMultiXactId + 1;

  return (MultiXactIdPrecedes(multi1, multi2) && MultiXactIdPrecedes(multi1, multi2 + MULTIXACT_OFFSETS_PER_PAGE - 1));
}

   
                                                                          
                                                                               
   
static bool
MultiXactMemberPagePrecedes(int page1, int page2)
{
  MultiXactOffset offset1;
  MultiXactOffset offset2;

  offset1 = ((MultiXactOffset)page1) * MULTIXACT_MEMBERS_PER_PAGE;
  offset2 = ((MultiXactOffset)page2) * MULTIXACT_MEMBERS_PER_PAGE;

  return (MultiXactOffsetPrecedes(offset1, offset2) && MultiXactOffsetPrecedes(offset1, offset2 + MULTIXACT_MEMBERS_PER_PAGE - 1));
}

   
                                                
   
                                                                  
                           
   
bool
MultiXactIdPrecedes(MultiXactId multi1, MultiXactId multi2)
{
  int32 diff = (int32)(multi1 - multi2);

  return (diff < 0);
}

   
                                                                 
   
                                                                  
                           
   
bool
MultiXactIdPrecedesOrEquals(MultiXactId multi1, MultiXactId multi2)
{
  int32 diff = (int32)(multi1 - multi2);

  return (diff <= 0);
}

   
                                           
   
static bool
MultiXactOffsetPrecedes(MultiXactOffset offset1, MultiXactOffset offset2)
{
  int32 diff = (int32)(offset1 - offset2);

  return (diff < 0);
}

   
                                                                      
                                   
   
static void
WriteMZeroPageXlogRec(int pageno, uint8 info)
{
  XLogBeginInsert();
  XLogRegisterData((char *)(&pageno), sizeof(int));
  (void)XLogInsert(RM_MULTIXACT_ID, info);
}

   
                                
   
                                                                           
                   
   
static void
WriteMTruncateXlogRec(Oid oldestMultiDB, MultiXactId startTruncOff, MultiXactId endTruncOff, MultiXactOffset startTruncMemb, MultiXactOffset endTruncMemb)
{
  XLogRecPtr recptr;
  xl_multixact_truncate xlrec;

  xlrec.oldestMultiDB = oldestMultiDB;

  xlrec.startTruncOff = startTruncOff;
  xlrec.endTruncOff = endTruncOff;

  xlrec.startTruncMemb = startTruncMemb;
  xlrec.endTruncMemb = endTruncMemb;

  XLogBeginInsert();
  XLogRegisterData((char *)(&xlrec), SizeOfMultiXactTruncate);
  recptr = XLogInsert(RM_MULTIXACT_ID, XLOG_MULTIXACT_TRUNCATE_ID);
  XLogFlush(recptr);
}

   
                                         
   
void
multixact_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

                                                       
  Assert(!XLogRecHasAnyBlockRefs(record));

  if (info == XLOG_MULTIXACT_ZERO_OFF_PAGE)
  {
    int pageno;
    int slotno;

    memcpy(&pageno, XLogRecGetData(record), sizeof(int));

    LWLockAcquire(MultiXactOffsetControlLock, LW_EXCLUSIVE);

    slotno = ZeroMultiXactOffsetPage(pageno, false);
    SimpleLruWritePage(MultiXactOffsetCtl, slotno);
    Assert(!MultiXactOffsetCtl->shared->page_dirty[slotno]);

    LWLockRelease(MultiXactOffsetControlLock);
  }
  else if (info == XLOG_MULTIXACT_ZERO_MEM_PAGE)
  {
    int pageno;
    int slotno;

    memcpy(&pageno, XLogRecGetData(record), sizeof(int));

    LWLockAcquire(MultiXactMemberControlLock, LW_EXCLUSIVE);

    slotno = ZeroMultiXactMemberPage(pageno, false);
    SimpleLruWritePage(MultiXactMemberCtl, slotno);
    Assert(!MultiXactMemberCtl->shared->page_dirty[slotno]);

    LWLockRelease(MultiXactMemberControlLock);
  }
  else if (info == XLOG_MULTIXACT_CREATE_ID)
  {
    xl_multixact_create *xlrec = (xl_multixact_create *)XLogRecGetData(record);
    TransactionId max_xid;
    int i;

                                                 
    RecordNewMultiXact(xlrec->mid, xlrec->moff, xlrec->nmembers, xlrec->members);

                                                                        
    MultiXactAdvanceNextMXact(xlrec->mid + 1, xlrec->moff + xlrec->nmembers);

       
                                                                        
                                                                          
                                                      
       
    max_xid = XLogRecGetXid(record);
    for (i = 0; i < xlrec->nmembers; i++)
    {
      if (TransactionIdPrecedes(max_xid, xlrec->members[i].xid))
      {
        max_xid = xlrec->members[i].xid;
      }
    }

    AdvanceNextFullTransactionIdPastXid(max_xid);
  }
  else if (info == XLOG_MULTIXACT_TRUNCATE_ID)
  {
    xl_multixact_truncate xlrec;
    int pageno;

    memcpy(&xlrec, XLogRecGetData(record), SizeOfMultiXactTruncate);

    elog(DEBUG1,
        "replaying multixact truncation: "
        "offsets [%u, %u), offsets segments [%x, %x), "
        "members [%u, %u), members segments [%x, %x)",
        xlrec.startTruncOff, xlrec.endTruncOff, MultiXactIdToOffsetSegment(xlrec.startTruncOff), MultiXactIdToOffsetSegment(xlrec.endTruncOff), xlrec.startTruncMemb, xlrec.endTruncMemb, MXOffsetToMemberSegment(xlrec.startTruncMemb), MXOffsetToMemberSegment(xlrec.endTruncMemb));

                                                            
    LWLockAcquire(MultiXactTruncationLock, LW_EXCLUSIVE);

       
                                                                    
                 
       
    SetMultiXactIdLimit(xlrec.endTruncOff, xlrec.oldestMultiDB, false);

    PerformMembersTruncation(xlrec.startTruncMemb, xlrec.endTruncMemb);

       
                                                                       
                                                                 
                          
       
    pageno = MultiXactIdToOffsetPage(xlrec.endTruncOff);
    MultiXactOffsetCtl->shared->latest_page_number = pageno;
    PerformOffsetsTruncation(xlrec.startTruncOff, xlrec.endTruncOff);

    LWLockRelease(MultiXactTruncationLock);
  }
  else
  {
    elog(PANIC, "multixact_redo: unknown op code %u", info);
  }
}

Datum
pg_get_multixact_members(PG_FUNCTION_ARGS)
{
  typedef struct
  {
    MultiXactMember *members;
    int nmembers;
    int iter;
  } mxact;
  MultiXactId mxid = PG_GETARG_UINT32(0);
  mxact *multi;
  FuncCallContext *funccxt;

  if (mxid < FirstMultiXactId)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid MultiXactId: %u", mxid)));
  }

  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcxt;
    TupleDesc tupdesc;

    funccxt = SRF_FIRSTCALL_INIT();
    oldcxt = MemoryContextSwitchTo(funccxt->multi_call_memory_ctx);

    multi = palloc(sizeof(mxact));
                                              
    multi->nmembers = GetMultiXactIdMembers(mxid, &multi->members, false, false);
    multi->iter = 0;

    tupdesc = CreateTemplateTupleDesc(2);
    TupleDescInitEntry(tupdesc, (AttrNumber)1, "xid", XIDOID, -1, 0);
    TupleDescInitEntry(tupdesc, (AttrNumber)2, "mode", TEXTOID, -1, 0);

    funccxt->attinmeta = TupleDescGetAttInMetadata(tupdesc);
    funccxt->user_fctx = multi;

    MemoryContextSwitchTo(oldcxt);
  }

  funccxt = SRF_PERCALL_SETUP();
  multi = (mxact *)funccxt->user_fctx;

  while (multi->iter < multi->nmembers)
  {
    HeapTuple tuple;
    char *values[2];

    values[0] = psprintf("%u", multi->members[multi->iter].xid);
    values[1] = mxstatus_to_string(multi->members[multi->iter].status);

    tuple = BuildTupleFromCStrings(funccxt->attinmeta, values);

    multi->iter++;
    pfree(values[0]);
    SRF_RETURN_NEXT(funccxt, HeapTupleGetDatum(tuple));
  }

  if (multi->nmembers > 0)
  {
    pfree(multi->members);
  }
  pfree(multi);

  SRF_RETURN_DONE(funccxt);
}
