                                                                            
   
            
                                       
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
   
                           
   
                                                                       
                                                                
                 
   
                                     
   
                                                                    
                                                                      
   
                         
                                                        
                                                   
   
#include "postgres.h"

#include <sys/file.h>
#include <unistd.h>

#include "access/tableam.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/storage.h"
#include "executor/instrument.h"
#include "lib/binaryheap.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "storage/buf_internals.h"
#include "storage/bufmgr.h"
#include "storage/ipc.h"
#include "storage/proc.h"
#include "storage/smgr.h"
#include "storage/standby.h"
#include "utils/rel.h"
#include "utils/resowner_private.h"
#include "utils/timestamp.h"

                                                                         
#define BufHdrGetBlock(bufHdr) ((Block)(BufferBlocks + ((Size)(bufHdr)->buf_id) * BLCKSZ))
#define BufferGetLSN(bufHdr) (PageGetLSN(BufHdrGetBlock(bufHdr)))

                                                                    
#define LocalBufHdrGetBlock(bufHdr) LocalBufferBlockPointers[-((bufHdr)->buf_id + 2)]

                                          
#define BUF_WRITTEN 0x01
#define BUF_REUSABLE 0x02

#define DROP_RELS_BSEARCH_THRESHOLD 20

typedef struct PrivateRefCountEntry
{
  Buffer buffer;
  int32 refcount;
} PrivateRefCountEntry;

                                                                
#define REFCOUNT_ARRAY_ENTRIES 8

   
                                                                     
                             
   
typedef struct CkptTsStatus
{
                             
  Oid tsId;

     
                                                                          
                                                                             
                                                                             
                                                                           
                        
     
  float8 progress;
  float8 progress_slice;

                                                             
  int num_to_scan;
                                                  
  int num_scanned;

                                                           
  int index;
} CkptTsStatus;

                   
bool zero_damaged_pages = false;
int bgwriter_lru_maxpages = 100;
double bgwriter_lru_multiplier = 2.0;
bool track_io_timing = false;
int effective_io_concurrency = 0;

   
                                                                           
                                                     
   
int checkpoint_flush_after = 0;
int bgwriter_flush_after = 0;
int backend_flush_after = 0;

   
                                                                             
                                                                   
                                                                          
                                                                      
                                           
   
int target_prefetch_pages = 0;

                                                         
static BufferDesc *InProgressBuf = NULL;
static bool IsForInput;

                                          
static BufferDesc *PinCountWaitBuf = NULL;

   
                                        
   
                                                                             
                                                                           
                                                                             
                                                                                
                                                                 
   
   
                                                                               
                                                                      
                                                                              
                                     
   
                                                                             
                                                                           
                                                                           
                                                                            
   
                                                                            
                           
   
   
                                                                               
                                                                           
                                                                          
                                                                          
                                                                 
   
static struct PrivateRefCountEntry PrivateRefCountArray[REFCOUNT_ARRAY_ENTRIES];
static HTAB *PrivateRefCountHash = NULL;
static int32 PrivateRefCountOverflowed = 0;
static uint32 PrivateRefCountClock = 0;
static PrivateRefCountEntry *ReservedRefCountEntry = NULL;

static void
ReservePrivateRefCountEntry(void);
static PrivateRefCountEntry *
NewPrivateRefCountEntry(Buffer buffer);
static PrivateRefCountEntry *
GetPrivateRefCountEntry(Buffer buffer, bool do_move);
static inline int32
GetPrivateRefCount(Buffer buffer);
static void
ForgetPrivateRefCountEntry(PrivateRefCountEntry *ref);

   
                                                                               
                                                                               
                                                                      
   
static void
ReservePrivateRefCountEntry(void)
{
                                                  
  if (ReservedRefCountEntry != NULL)
  {
    return;
  }

     
                                                                           
                        
     
  {
    int i;

    for (i = 0; i < REFCOUNT_ARRAY_ENTRIES; i++)
    {
      PrivateRefCountEntry *res;

      res = &PrivateRefCountArray[i];

      if (res->buffer == InvalidBuffer)
      {
        ReservedRefCountEntry = res;
        return;
      }
    }
  }

     
                                                                             
            
     
  {
       
                                                                        
                                 
       
    PrivateRefCountEntry *hashent;
    bool found;

                            
    ReservedRefCountEntry = &PrivateRefCountArray[PrivateRefCountClock++ % REFCOUNT_ARRAY_ENTRIES];

                                                          
    Assert(ReservedRefCountEntry->buffer != InvalidBuffer);

                                                 
    hashent = hash_search(PrivateRefCountHash, (void *)&(ReservedRefCountEntry->buffer), HASH_ENTER, &found);
    Assert(!found);
    hashent->refcount = ReservedRefCountEntry->refcount;

                                       
    ReservedRefCountEntry->buffer = InvalidBuffer;
    ReservedRefCountEntry->refcount = 0;

    PrivateRefCountOverflowed++;
  }
}

   
                                              
   
static PrivateRefCountEntry *
NewPrivateRefCountEntry(Buffer buffer)
{
  PrivateRefCountEntry *res;

                                                                  
  Assert(ReservedRefCountEntry != NULL);

                                 
  res = ReservedRefCountEntry;
  ReservedRefCountEntry = NULL;

                   
  res->buffer = buffer;
  res->refcount = 0;

  return res;
}

   
                                                           
   
                                                                         
                                                                        
                                                            
   
static PrivateRefCountEntry *
GetPrivateRefCountEntry(Buffer buffer, bool do_move)
{
  PrivateRefCountEntry *res;
  int i;

  Assert(BufferIsValid(buffer));
  Assert(!BufferIsLocal(buffer));

     
                                                                            
                        
     
  for (i = 0; i < REFCOUNT_ARRAY_ENTRIES; i++)
  {
    res = &PrivateRefCountArray[i];

    if (res->buffer == buffer)
    {
      return res;
    }
  }

     
                                                                           
                
     
                                                                             
              
     
  if (PrivateRefCountOverflowed == 0)
  {
    return NULL;
  }

  res = hash_search(PrivateRefCountHash, (void *)&buffer, HASH_FIND, NULL);

  if (res == NULL)
  {
    return NULL;
  }
  else if (!do_move)
  {
                                                                      
    return res;
  }
  else
  {
                                                             
    bool found;
    PrivateRefCountEntry *free;

                                          
    ReservePrivateRefCountEntry();

                                  
    Assert(ReservedRefCountEntry != NULL);
    free = ReservedRefCountEntry;
    ReservedRefCountEntry = NULL;
    Assert(free->buffer == InvalidBuffer);

                     
    free->buffer = buffer;
    free->refcount = res->refcount;

                               
    hash_search(PrivateRefCountHash, (void *)&buffer, HASH_REMOVE, &found);
    Assert(found);
    Assert(PrivateRefCountOverflowed > 0);
    PrivateRefCountOverflowed--;

    return free;
  }
}

   
                                                                       
   
                                         
   
static inline int32
GetPrivateRefCount(Buffer buffer)
{
  PrivateRefCountEntry *ref;

  Assert(BufferIsValid(buffer));
  Assert(!BufferIsLocal(buffer));

     
                                                                          
                                  
     
  ref = GetPrivateRefCountEntry(buffer, false);

  if (ref == NULL)
  {
    return 0;
  }
  return ref->refcount;
}

   
                                                                               
                                                               
   
static void
ForgetPrivateRefCountEntry(PrivateRefCountEntry *ref)
{
  Assert(ref->refcount == 0);

  if (ref >= &PrivateRefCountArray[0] && ref < &PrivateRefCountArray[REFCOUNT_ARRAY_ENTRIES])
  {
    ref->buffer = InvalidBuffer;

       
                                                                     
                                                                        
                
       
    ReservedRefCountEntry = ref;
  }
  else
  {
    bool found;
    Buffer buffer = ref->buffer;

    hash_search(PrivateRefCountHash, (void *)&buffer, HASH_REMOVE, &found);
    Assert(found);
    Assert(PrivateRefCountOverflowed > 0);
    PrivateRefCountOverflowed--;
  }
}

   
                  
                                                                         
   
                                                                   
                                                                 
   
#define BufferIsPinned(bufnum) (!BufferIsValid(bufnum) ? false : BufferIsLocal(bufnum) ? (LocalRefCount[-(bufnum)-1] > 0) : (GetPrivateRefCount(bufnum) > 0))

static Buffer
ReadBuffer_common(SMgrRelation reln, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy, bool *hit);
static bool
PinBuffer(BufferDesc *buf, BufferAccessStrategy strategy);
static void
PinBuffer_Locked(BufferDesc *buf);
static void
UnpinBuffer(BufferDesc *buf, bool fixOwner);
static void
BufferSync(int flags);
static uint32
WaitBufHdrUnlocked(BufferDesc *buf);
static int
SyncOneBuffer(int buf_id, bool skip_recently_used, WritebackContext *flush_context);
static void
WaitIO(BufferDesc *buf);
static bool
StartBufferIO(BufferDesc *buf, bool forInput);
static void
TerminateBufferIO(BufferDesc *buf, bool clear_dirty, uint32 set_flag_bits);
static void
shared_buffer_write_error_callback(void *arg);
static void
local_buffer_write_error_callback(void *arg);
static BufferDesc *
BufferAlloc(SMgrRelation smgr, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, BufferAccessStrategy strategy, bool *foundPtr);
static void
FlushBuffer(BufferDesc *buf, SMgrRelation reln);
static void
AtProcExit_Buffers(int code, Datum arg);
static void
CheckForBufferLeaks(void);
static int
rnode_comparator(const void *p1, const void *p2);
static int
buffertag_comparator(const void *p1, const void *p2);
static int
ckpt_buforder_comparator(const void *pa, const void *pb);
static int
ts_ckpt_progress_comparator(Datum a, Datum b, void *arg);

   
                                                                           
                        
   
bool
ComputeIoConcurrency(int io_concurrency, double *target)
{
  double new_prefetch_pages = 0.0;
  int i;

     
                                                                           
                                                     
     
  io_concurrency = Min(Max(io_concurrency, 0), MAX_IO_CONCURRENCY);

               
                                                                        
                                                                         
                                                                           
                                                  
     
                                                                            
     
                             
                              
              
                          
                                    
                                          
                     
     
                                                                          
                                                                        
                                                                            
     
                                                                            
                                                                      
                                                                             
                                                                         
                            
     
                                                                             
                                                            
     
                                                                         
               
     

  for (i = 1; i <= io_concurrency; i++)
  {
    new_prefetch_pages += (double)io_concurrency / (double)i;
  }

  *target = new_prefetch_pages;

                                                              
  return (new_prefetch_pages >= 0.0 && new_prefetch_pages < (double)INT_MAX);
}

   
                                                                         
   
                                                                          
                                                                              
                                                                   
                                           
   
void
PrefetchBuffer(Relation reln, ForkNumber forkNum, BlockNumber blockNum)
{
#ifdef USE_PREFETCH
  Assert(RelationIsValid(reln));
  Assert(BlockNumberIsValid(blockNum));

  if (RelationUsesLocalBuffers(reln))
  {
                                            
    if (RELATION_IS_OTHER_TEMP(reln))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot access temporary tables of other sessions")));
    }

                                   
    LocalPrefetchBuffer(RelationGetSmgr(reln), forkNum, blockNum);
  }
  else
  {
    BufferTag newTag;                                          
    uint32 newHash;                                      
    LWLock *newPartitionLock;                                   
    int buf_id;

                                                  
    INIT_BUFFERTAG(newTag, RelationGetSmgr(reln)->smgr_rnode.node, forkNum, blockNum);

                                                       
    newHash = BufTableHashCode(&newTag);
    newPartitionLock = BufMappingPartitionLock(newHash);

                                                        
    LWLockAcquire(newPartitionLock, LW_SHARED);
    buf_id = BufTableLookup(&newTag, newHash);
    LWLockRelease(newPartitionLock);

                                              
    if (buf_id < 0)
    {
      smgrprefetch(RelationGetSmgr(reln), forkNum, blockNum);
    }

       
                                                                        
                                                                          
                                                                        
                                                                          
                                                                        
                                                                 
                                                                         
                                                                         
                                                                   
       
  }
#endif                   
}

   
                                                                           
                                                    
   
Buffer
ReadBuffer(Relation reln, BlockNumber blockNum)
{
  return ReadBufferExtended(reln, MAIN_FORKNUM, blockNum, RBM_NORMAL, NULL);
}

   
                                                                   
                                                    
                                                     
                                                      
                                                     
                                
   
                                                        
                                                          
                                                 
   
                                                                           
   
                                                                          
                                                                         
                                                         
                              
   
                                                                            
                                                                            
                                                                         
   
                                                                              
                                                                              
                                                                           
                                                                     
                                                                         
                                                           
                                                                              
                                                                       
                                                              
   
                                                                            
                                        
   
                                                                  
   
                                                                         
                                  
   
Buffer
ReadBufferExtended(Relation reln, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy)
{
  bool hit;
  Buffer buf;

     
                                                                        
                                                                          
                              
     
  if (RELATION_IS_OTHER_TEMP(reln))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot access temporary tables of other sessions")));
  }

     
                                                                           
           
     
  pgstat_count_buffer_read(reln);
  buf = ReadBuffer_common(RelationGetSmgr(reln), reln->rd_rel->relpersistence, forkNum, blockNum, mode, strategy, &hit);
  if (hit)
  {
    pgstat_count_buffer_hit(reln);
  }
  return buf;
}

   
                                                                             
                                       
   
                                                                                
                                                                          
                                                                               
               
   
Buffer
ReadBufferWithoutRelcache(RelFileNode rnode, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy)
{
  bool hit;

  SMgrRelation smgr = smgropen(rnode, InvalidBackendId);

  Assert(InRecovery);

  return ReadBuffer_common(smgr, RELPERSISTENCE_PERMANENT, forkNum, blockNum, mode, strategy, &hit);
}

   
                                                                 
   
                                                                              
   
static Buffer
ReadBuffer_common(SMgrRelation smgr, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, ReadBufferMode mode, BufferAccessStrategy strategy, bool *hit)
{
  BufferDesc *bufHdr;
  Block bufBlock;
  bool found;
  bool isExtend;
  bool isLocalBuf = SmgrIsTemp(smgr);

  *hit = false;

                                                              
  ResourceOwnerEnlargeBuffers(CurrentResourceOwner);

  isExtend = (blockNum == P_NEW);

  TRACE_POSTGRESQL_BUFFER_READ_START(forkNum, blockNum, smgr->smgr_rnode.node.spcNode, smgr->smgr_rnode.node.dbNode, smgr->smgr_rnode.node.relNode, smgr->smgr_rnode.backend, isExtend);

                                                                
  if (isExtend)
  {
    blockNum = smgrnblocks(smgr, forkNum);
                                                                
    if (blockNum == P_NEW)
    {
      ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("cannot extend relation %s beyond %u blocks", relpath(smgr->smgr_rnode, forkNum), P_NEW)));
    }
  }

  if (isLocalBuf)
  {
    bufHdr = LocalBufferAlloc(smgr, forkNum, blockNum, &found);
    if (found)
    {
      pgBufferUsage.local_blks_hit++;
    }
    else if (isExtend)
    {
      pgBufferUsage.local_blks_written++;
    }
    else if (mode == RBM_NORMAL || mode == RBM_NORMAL_NO_LOG || mode == RBM_ZERO_ON_ERROR)
    {
      pgBufferUsage.local_blks_read++;
    }
  }
  else
  {
       
                                                                           
                                
       
    bufHdr = BufferAlloc(smgr, relpersistence, forkNum, blockNum, strategy, &found);
    if (found)
    {
      pgBufferUsage.shared_blks_hit++;
    }
    else if (isExtend)
    {
      pgBufferUsage.shared_blks_written++;
    }
    else if (mode == RBM_NORMAL || mode == RBM_NORMAL_NO_LOG || mode == RBM_ZERO_ON_ERROR)
    {
      pgBufferUsage.shared_blks_read++;
    }
  }

                                               

                                                        
  if (found)
  {
    if (!isExtend)
    {
                                                    
      *hit = true;
      VacuumPageHit++;

      if (VacuumCostActive)
      {
        VacuumCostBalance += VacuumCostPageHit;
      }

      TRACE_POSTGRESQL_BUFFER_READ_DONE(forkNum, blockNum, smgr->smgr_rnode.node.spcNode, smgr->smgr_rnode.node.dbNode, smgr->smgr_rnode.node.relNode, smgr->smgr_rnode.backend, isExtend, found);

         
                                                                     
                           
         
      if (!isLocalBuf)
      {
        if (mode == RBM_ZERO_AND_LOCK)
        {
          LWLockAcquire(BufferDescriptorGetContentLock(bufHdr), LW_EXCLUSIVE);
        }
        else if (mode == RBM_ZERO_AND_CLEANUP_LOCK)
        {
          LockBufferForCleanup(BufferDescriptorGetBuffer(bufHdr));
        }
      }

      return BufferDescriptorGetBuffer(bufHdr);
    }

       
                                                                         
                                                                        
                                                                          
                                                                        
                                                                     
                                                                     
                                                               
                                                                          
                                                                        
                                                                          
                                                                         
       
    bufBlock = isLocalBuf ? LocalBufHdrGetBlock(bufHdr) : BufHdrGetBlock(bufHdr);
    if (!PageIsNew((Page)bufBlock))
    {
      ereport(ERROR, (errmsg("unexpected data beyond EOF in block %u of relation %s", blockNum, relpath(smgr->smgr_rnode, forkNum)), errhint("This has been seen to occur with buggy kernels; consider updating your system.")));
    }

       
                                                                         
                                                                         
                                                                           
                                                  
       
    if (isLocalBuf)
    {
                                     
      uint32 buf_state = pg_atomic_read_u32(&bufHdr->state);

      Assert(buf_state & BM_VALID);
      buf_state &= ~BM_VALID;
      pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
    }
    else
    {
         
                                                                        
                                                                       
             
         
      do
      {
        uint32 buf_state = LockBufHdr(bufHdr);

        Assert(buf_state & BM_VALID);
        buf_state &= ~BM_VALID;
        UnlockBufHdr(bufHdr, buf_state);
      } while (!StartBufferIO(bufHdr, true));
    }
  }

     
                                                                         
                                                                             
                              
     
                                                                     
                                                                          
                                                                           
                                                                             
                                                                        
            
     
  Assert(!(pg_atomic_read_u32(&bufHdr->state) & BM_VALID));                          

  bufBlock = isLocalBuf ? LocalBufHdrGetBlock(bufHdr) : BufHdrGetBlock(bufHdr);

  if (isExtend)
  {
                                     
    MemSet((char *)bufBlock, 0, BLCKSZ);
                                              
    smgrextend(smgr, forkNum, blockNum, (char *)bufBlock, false);

       
                                                                   
                                                                        
                                                                       
                                     
       
  }
  else
  {
       
                                                                       
                                           
       
    if (mode == RBM_ZERO_AND_LOCK || mode == RBM_ZERO_AND_CLEANUP_LOCK)
    {
      MemSet((char *)bufBlock, 0, BLCKSZ);
    }
    else
    {
      instr_time io_start, io_time;

      if (track_io_timing)
      {
        INSTR_TIME_SET_CURRENT(io_start);
      }

      smgrread(smgr, forkNum, blockNum, (char *)bufBlock);

      if (track_io_timing)
      {
        INSTR_TIME_SET_CURRENT(io_time);
        INSTR_TIME_SUBTRACT(io_time, io_start);
        pgstat_count_buffer_read_time(INSTR_TIME_GET_MICROSEC(io_time));
        INSTR_TIME_ADD(pgBufferUsage.blk_read_time, io_time);
      }

                                  
      if (!PageIsVerifiedExtended((Page)bufBlock, blockNum, PIV_LOG_WARNING | PIV_REPORT_STAT))
      {
        if (mode == RBM_ZERO_ON_ERROR || zero_damaged_pages)
        {
          ereport(WARNING, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("invalid page in block %u of relation %s; zeroing out page", blockNum, relpath(smgr->smgr_rnode, forkNum))));
          MemSet((char *)bufBlock, 0, BLCKSZ);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg("invalid page in block %u of relation %s", blockNum, relpath(smgr->smgr_rnode, forkNum))));
        }
      }
    }
  }

     
                                                                            
                                                                           
                                                               
     
                                                                            
                                                                             
                                                                             
                                                    
     
  if ((mode == RBM_ZERO_AND_LOCK || mode == RBM_ZERO_AND_CLEANUP_LOCK) && !isLocalBuf)
  {
    LWLockAcquire(BufferDescriptorGetContentLock(bufHdr), LW_EXCLUSIVE);
  }

  if (isLocalBuf)
  {
                                   
    uint32 buf_state = pg_atomic_read_u32(&bufHdr->state);

    buf_state |= BM_VALID;
    pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);
  }
  else
  {
                                                             
    TerminateBufferIO(bufHdr, false, BM_VALID);
  }

  VacuumPageMiss++;
  if (VacuumCostActive)
  {
    VacuumCostBalance += VacuumCostPageMiss;
  }

  TRACE_POSTGRESQL_BUFFER_READ_DONE(forkNum, blockNum, smgr->smgr_rnode.node.spcNode, smgr->smgr_rnode.node.dbNode, smgr->smgr_rnode.node.relNode, smgr->smgr_rnode.backend, isExtend, found);

  return BufferDescriptorGetBuffer(bufHdr);
}

   
                                                                         
                                                                
                                                                   
   
                                                                       
                                                                             
                                                                           
   
                                                                      
                                                                        
                                                                         
                                                                     
   
                                                                        
                                            
   
                                              
   
static BufferDesc *
BufferAlloc(SMgrRelation smgr, char relpersistence, ForkNumber forkNum, BlockNumber blockNum, BufferAccessStrategy strategy, bool *foundPtr)
{
  BufferTag newTag;                                          
  uint32 newHash;                                      
  LWLock *newPartitionLock;                                   
  BufferTag oldTag;                                                   
  uint32 oldHash;                                      
  LWLock *oldPartitionLock;                                   
  uint32 oldFlags;
  int buf_id;
  BufferDesc *buf;
  bool valid;
  uint32 buf_state;

                                                
  INIT_BUFFERTAG(newTag, smgr->smgr_rnode.node, forkNum, blockNum);

                                                     
  newHash = BufTableHashCode(&newTag);
  newPartitionLock = BufMappingPartitionLock(newHash);

                                                      
  LWLockAcquire(newPartitionLock, LW_SHARED);
  buf_id = BufTableLookup(&newTag, newHash);
  if (buf_id >= 0)
  {
       
                                                                      
                                                                         
                        
       
    buf = GetBufferDescriptor(buf_id);

    valid = PinBuffer(buf, strategy);

                                                                 
    LWLockRelease(newPartitionLock);

    *foundPtr = true;

    if (!valid)
    {
         
                                                                      
                                                                      
                                                                         
                                                             
                                    
         
      if (StartBufferIO(buf, true))
      {
           
                                                                     
                                                           
           
        *foundPtr = false;
      }
    }

    return buf;
  }

     
                                                                        
                                                                        
     
  LWLockRelease(newPartitionLock);

                                                              
  for (;;)
  {
       
                                                                      
                       
       
    ReservePrivateRefCountEntry();

       
                                                                       
                            
       
    buf = StrategyGetBuffer(strategy, &buf_state);

    Assert(BUF_STATE_GET_REFCOUNT(buf_state) == 0);

                                                                 
    oldFlags = buf_state & BUF_FLAG_MASK;

                                                             
    PinBuffer_Locked(buf);

       
                                                                      
                                                                           
                                                                        
                                                                       
                                           
       
    if (oldFlags & BM_DIRTY)
    {
         
                                                                     
                                                                       
                                                                      
                                                                    
                                                                    
                                                                        
                                                                         
                                                                        
                                                                    
                                                                      
                                                                    
                                                                       
                             
         
      if (LWLockConditionalAcquire(BufferDescriptorGetContentLock(buf), LW_SHARED))
      {
           
                                                                  
                                                                      
                                                                       
                                                                  
                                                   
           
        if (strategy != NULL)
        {
          XLogRecPtr lsn;

                                                             
          buf_state = LockBufHdr(buf);
          lsn = BufferGetLSN(buf);
          UnlockBufHdr(buf, buf_state);

          if (XLogNeedsFlush(lsn) && StrategyRejectBuffer(strategy, buf))
          {
                                                                  
            LWLockRelease(BufferDescriptorGetContentLock(buf));
            UnpinBuffer(buf, true);
            continue;
          }
        }

                            
        TRACE_POSTGRESQL_BUFFER_WRITE_DIRTY_START(forkNum, blockNum, smgr->smgr_rnode.node.spcNode, smgr->smgr_rnode.node.dbNode, smgr->smgr_rnode.node.relNode);

        FlushBuffer(buf, NULL);
        LWLockRelease(BufferDescriptorGetContentLock(buf));

        ScheduleBufferTagForWriteback(&BackendWritebackContext, &buf->tag);

        TRACE_POSTGRESQL_BUFFER_WRITE_DIRTY_DONE(forkNum, blockNum, smgr->smgr_rnode.node.spcNode, smgr->smgr_rnode.node.dbNode, smgr->smgr_rnode.node.relNode);
      }
      else
      {
           
                                                                      
                                    
           
        UnpinBuffer(buf, true);
        continue;
      }
    }

       
                                                                       
                                                                  
       
    if (oldFlags & BM_TAG_VALID)
    {
         
                                                                       
                                                                       
                                               
         
      oldTag = buf->tag;
      oldHash = BufTableHashCode(&oldTag);
      oldPartitionLock = BufMappingPartitionLock(oldHash);

         
                                                               
                    
         
      if (oldPartitionLock < newPartitionLock)
      {
        LWLockAcquire(oldPartitionLock, LW_EXCLUSIVE);
        LWLockAcquire(newPartitionLock, LW_EXCLUSIVE);
      }
      else if (oldPartitionLock > newPartitionLock)
      {
        LWLockAcquire(newPartitionLock, LW_EXCLUSIVE);
        LWLockAcquire(oldPartitionLock, LW_EXCLUSIVE);
      }
      else
      {
                                               
        LWLockAcquire(newPartitionLock, LW_EXCLUSIVE);
      }
    }
    else
    {
                                                              
      LWLockAcquire(newPartitionLock, LW_EXCLUSIVE);
                                                         
      oldPartitionLock = NULL;
                                                                     
      oldHash = 0;
    }

       
                                                                       
                                                                  
                                                                       
                                                                         
            
       
    buf_id = BufTableInsert(&newTag, newHash, buf->buf_id);

    if (buf_id >= 0)
    {
         
                                                                         
                                                                      
                                                                     
                          
         
      UnpinBuffer(buf, true);

                                                                
      if (oldPartitionLock != NULL && oldPartitionLock != newPartitionLock)
      {
        LWLockRelease(oldPartitionLock);
      }

                                                              

      buf = GetBufferDescriptor(buf_id);

      valid = PinBuffer(buf, strategy);

                                                                   
      LWLockRelease(newPartitionLock);

      *foundPtr = true;

      if (!valid)
      {
           
                                                                     
                                                                   
                                                                   
                                                                     
                                                 
           
        if (StartBufferIO(buf, true))
        {
             
                                                                  
                                                                  
             
          *foundPtr = false;
        }
      }

      return buf;
    }

       
                                                                      
       
    buf_state = LockBufHdr(buf);

       
                                                                         
                                                                          
                                                                         
                                      
       
    oldFlags = buf_state & BUF_FLAG_MASK;
    if (BUF_STATE_GET_REFCOUNT(buf_state) == 1 && !(oldFlags & BM_DIRTY))
    {
      break;
    }

    UnlockBufHdr(buf, buf_state);
    BufTableDelete(&newTag, newHash);
    if (oldPartitionLock != NULL && oldPartitionLock != newPartitionLock)
    {
      LWLockRelease(oldPartitionLock);
    }
    LWLockRelease(newPartitionLock);
    UnpinBuffer(buf, true);
  }

     
                                                   
     
                                                                         
                                                                          
                                                                            
                                                             
     
                                                                             
                                                                       
                                                                          
                                    
     
  buf->tag = newTag;
  buf_state &= ~(BM_VALID | BM_DIRTY | BM_JUST_DIRTIED | BM_CHECKPOINT_NEEDED | BM_IO_ERROR | BM_PERMANENT | BUF_USAGECOUNT_MASK);
  if (relpersistence == RELPERSISTENCE_PERMANENT || forkNum == INIT_FORKNUM)
  {
    buf_state |= BM_TAG_VALID | BM_PERMANENT | BUF_USAGECOUNT_ONE;
  }
  else
  {
    buf_state |= BM_TAG_VALID | BUF_USAGECOUNT_ONE;
  }

  UnlockBufHdr(buf, buf_state);

  if (oldPartitionLock != NULL)
  {
    BufTableDelete(&oldTag, oldHash);
    if (oldPartitionLock != newPartitionLock)
    {
      LWLockRelease(oldPartitionLock);
    }
  }

  LWLockRelease(newPartitionLock);

     
                                                                           
                                                                         
                                                                             
     
  if (StartBufferIO(buf, true))
  {
    *foundPtr = false;
  }
  else
  {
    *foundPtr = true;
  }

  return buf;
}

   
                                                                         
             
   
                                                                        
                                                                     
                                                     
   
                                                                         
                                                                         
                                                                       
                                                                     
                       
   
                                                                       
                                                            
   
static void
InvalidateBuffer(BufferDesc *buf)
{
  BufferTag oldTag;
  uint32 oldHash;                                      
  LWLock *oldPartitionLock;                                   
  uint32 oldFlags;
  uint32 buf_state;

                                                                 
  oldTag = buf->tag;

  buf_state = pg_atomic_read_u32(&buf->state);
  Assert(buf_state & BM_LOCKED);
  UnlockBufHdr(buf, buf_state);

     
                                                                             
                                                                          
                          
     
  oldHash = BufTableHashCode(&oldTag);
  oldPartitionLock = BufMappingPartitionLock(oldHash);

retry:

     
                                                                             
                  
     
  LWLockAcquire(oldPartitionLock, LW_EXCLUSIVE);

                                 
  buf_state = LockBufHdr(buf);

                                                                  
  if (!BUFFERTAGS_EQUAL(buf->tag, oldTag))
  {
    UnlockBufHdr(buf, buf_state);
    LWLockRelease(oldPartitionLock);
    return;
  }

     
                                                                           
                                                                         
                                                                            
                                                                            
                                                                          
                                                                            
                            
     
  if (BUF_STATE_GET_REFCOUNT(buf_state) != 0)
  {
    UnlockBufHdr(buf, buf_state);
    LWLockRelease(oldPartitionLock);
                                                              
    if (GetPrivateRefCount(BufferDescriptorGetBuffer(buf)) > 0)
    {
      elog(ERROR, "buffer is pinned in InvalidateBuffer");
    }
    WaitIO(buf);
    goto retry;
  }

     
                                                                           
                                                                       
     
  oldFlags = buf_state & BUF_FLAG_MASK;
  CLEAR_BUFFERTAG(buf->tag);
  buf_state &= ~(BUF_FLAG_MASK | BUF_USAGECOUNT_MASK);
  UnlockBufHdr(buf, buf_state);

     
                                                                      
     
  if (oldFlags & BM_TAG_VALID)
  {
    BufTableDelete(&oldTag, oldHash);
  }

     
                             
     
  LWLockRelease(oldPartitionLock);

     
                                                                
     
  StrategyFreeBuffer(buf);
}

   
                   
   
                                                                 
   
                                                                         
                                                                            
                                                 
   
void
MarkBufferDirty(Buffer buffer)
{
  BufferDesc *bufHdr;
  uint32 buf_state;
  uint32 old_buf_state;

  if (!BufferIsValid(buffer))
  {
    elog(ERROR, "bad buffer ID: %d", buffer);
  }

  if (BufferIsLocal(buffer))
  {
    MarkLocalBufferDirty(buffer);
    return;
  }

  bufHdr = GetBufferDescriptor(buffer - 1);

  Assert(BufferIsPinned(buffer));
  Assert(LWLockHeldByMeInMode(BufferDescriptorGetContentLock(bufHdr), LW_EXCLUSIVE));

  old_buf_state = pg_atomic_read_u32(&bufHdr->state);
  for (;;)
  {
    if (old_buf_state & BM_LOCKED)
    {
      old_buf_state = WaitBufHdrUnlocked(bufHdr);
    }

    buf_state = old_buf_state;

    Assert(BUF_STATE_GET_REFCOUNT(buf_state) > 0);
    buf_state |= BM_DIRTY | BM_JUST_DIRTIED;

    if (pg_atomic_compare_exchange_u32(&bufHdr->state, &old_buf_state, buf_state))
    {
      break;
    }
  }

     
                                                                
     
  if (!(old_buf_state & BM_DIRTY))
  {
    VacuumPageDirty++;
    pgBufferUsage.shared_blks_dirtied++;
    if (VacuumCostActive)
    {
      VacuumCostBalance += VacuumCostPageDirty;
    }
  }
}

   
                                                                    
   
                                                                        
                                                                          
                                                                       
                                                                         
                                                                         
   
                                                                          
                                                                               
                                          
   
Buffer
ReleaseAndReadBuffer(Buffer buffer, Relation relation, BlockNumber blockNum)
{
  ForkNumber forkNum = MAIN_FORKNUM;
  BufferDesc *bufHdr;

  if (BufferIsValid(buffer))
  {
    Assert(BufferIsPinned(buffer));
    if (BufferIsLocal(buffer))
    {
      bufHdr = GetLocalBufferDescriptor(-buffer - 1);
      if (bufHdr->tag.blockNum == blockNum && RelFileNodeEquals(bufHdr->tag.rnode, relation->rd_node) && bufHdr->tag.forkNum == forkNum)
      {
        return buffer;
      }
      ResourceOwnerForgetBuffer(CurrentResourceOwner, buffer);
      LocalRefCount[-buffer - 1]--;
    }
    else
    {
      bufHdr = GetBufferDescriptor(buffer - 1);
                                                                   
      if (bufHdr->tag.blockNum == blockNum && RelFileNodeEquals(bufHdr->tag.rnode, relation->rd_node) && bufHdr->tag.forkNum == forkNum)
      {
        return buffer;
      }
      UnpinBuffer(bufHdr, true);
    }
  }

  return ReadBuffer(relation, blockNum);
}

   
                                                         
   
                                                                            
                                                                                
                                                                           
                                                                                
                                                                            
                                                                          
                                                                           
   
                                                                    
   
                                                                          
                                                                               
                                                     
   
                                                                      
   
                                                                          
                                                  
   
static bool
PinBuffer(BufferDesc *buf, BufferAccessStrategy strategy)
{
  Buffer b = BufferDescriptorGetBuffer(buf);
  bool result;
  PrivateRefCountEntry *ref;

  ref = GetPrivateRefCountEntry(b, true);

  if (ref == NULL)
  {
    uint32 buf_state;
    uint32 old_buf_state;

    ReservePrivateRefCountEntry();
    ref = NewPrivateRefCountEntry(b);

    old_buf_state = pg_atomic_read_u32(&buf->state);
    for (;;)
    {
      if (old_buf_state & BM_LOCKED)
      {
        old_buf_state = WaitBufHdrUnlocked(buf);
      }

      buf_state = old_buf_state;

                             
      buf_state += BUF_REFCOUNT_ONE;

      if (strategy == NULL)
      {
                                                                   
        if (BUF_STATE_GET_USAGECOUNT(buf_state) < BM_MAX_USAGE_COUNT)
        {
          buf_state += BUF_USAGECOUNT_ONE;
        }
      }
      else
      {
           
                                                                   
                                              
           
        if (BUF_STATE_GET_USAGECOUNT(buf_state) == 0)
        {
          buf_state += BUF_USAGECOUNT_ONE;
        }
      }

      if (pg_atomic_compare_exchange_u32(&buf->state, &old_buf_state, buf_state))
      {
        result = (buf_state & BM_VALID) != 0;
        break;
      }
    }
  }
  else
  {
                                                                     
    result = true;
  }

  ref->refcount++;
  Assert(ref->refcount > 0);
  ResourceOwnerRememberBuffer(CurrentResourceOwner, b);
  return result;
}

   
                                                                              
                                           
   
                                                                        
                                                  
   
                                                                      
                                                                    
                                                                              
            
   
                                                                         
                                                                               
                                                                             
                           
   
                                                                               
                                                                            
                                  
   
static void
PinBuffer_Locked(BufferDesc *buf)
{
  Buffer b;
  PrivateRefCountEntry *ref;
  uint32 buf_state;

     
                                                                           
                                                                 
     
  Assert(GetPrivateRefCountEntry(BufferDescriptorGetBuffer(buf), false) == NULL);

     
                                                                           
                                        
     
  buf_state = pg_atomic_read_u32(&buf->state);
  Assert(buf_state & BM_LOCKED);
  buf_state += BUF_REFCOUNT_ONE;
  UnlockBufHdr(buf, buf_state);

  b = BufferDescriptorGetBuffer(buf);

  ref = NewPrivateRefCountEntry(b);
  ref->refcount++;

  ResourceOwnerRememberBuffer(CurrentResourceOwner, b);
}

   
                                                         
   
                                                                    
   
                                                                      
                                                  
   
static void
UnpinBuffer(BufferDesc *buf, bool fixOwner)
{
  PrivateRefCountEntry *ref;
  Buffer b = BufferDescriptorGetBuffer(buf);

                                                          
  ref = GetPrivateRefCountEntry(b, false);
  Assert(ref != NULL);

  if (fixOwner)
  {
    ResourceOwnerForgetBuffer(CurrentResourceOwner, b);
  }

  Assert(ref->refcount > 0);
  ref->refcount--;
  if (ref->refcount == 0)
  {
    uint32 buf_state;
    uint32 old_buf_state;

                                                           
    Assert(!LWLockHeldByMe(BufferDescriptorGetContentLock(buf)));
    Assert(!LWLockHeldByMe(BufferDescriptorGetIOLock(buf)));

       
                                             
       
                                                                        
                                                                        
       
    old_buf_state = pg_atomic_read_u32(&buf->state);
    for (;;)
    {
      if (old_buf_state & BM_LOCKED)
      {
        old_buf_state = WaitBufHdrUnlocked(buf);
      }

      buf_state = old_buf_state;

      buf_state -= BUF_REFCOUNT_ONE;

      if (pg_atomic_compare_exchange_u32(&buf->state, &old_buf_state, buf_state))
      {
        break;
      }
    }

                                        
    if (buf_state & BM_PIN_COUNT_WAITER)
    {
         
                                                                         
                                                                      
                                                                     
                                                                    
                 
         
      buf_state = LockBufHdr(buf);

      if ((buf_state & BM_PIN_COUNT_WAITER) && BUF_STATE_GET_REFCOUNT(buf_state) == 1)
      {
                                                                   
        int wait_backend_pid = buf->wait_backend_pid;

        buf_state &= ~BM_PIN_COUNT_WAITER;
        UnlockBufHdr(buf, buf_state);
        ProcSendSignal(wait_backend_pid);
      }
      else
      {
        UnlockBufHdr(buf, buf_state);
      }
    }
    ForgetPrivateRefCountEntry(ref);
  }
}

   
                                                          
   
                                                                            
                                                                              
                                                                        
                                                                            
                                                                       
                                  
   
static void
BufferSync(int flags)
{
  uint32 buf_state;
  int buf_id;
  int num_to_scan;
  int num_spaces;
  int num_processed;
  int num_written;
  CkptTsStatus *per_ts_stat = NULL;
  Oid last_tsid;
  binaryheap *ts_heap;
  int i;
  int mask = BM_DIRTY;
  WritebackContext wb_context;

                                                            
  ResourceOwnerEnlargeBuffers(CurrentResourceOwner);

     
                                                                           
                                                                        
                                           
     
  if (!((flags & (CHECKPOINT_IS_SHUTDOWN | CHECKPOINT_END_OF_RECOVERY | CHECKPOINT_FLUSH_ALL))))
  {
    mask |= BM_PERMANENT;
  }

     
                                                                           
                                                                          
                                                  
     
                                                                       
                                                                         
                                                                            
                                                                            
                                                                           
                        
     
                                                                          
                                                                             
                                                                        
     
  num_to_scan = 0;
  for (buf_id = 0; buf_id < NBuffers; buf_id++)
  {
    BufferDesc *bufHdr = GetBufferDescriptor(buf_id);

       
                                                                     
                      
       
    buf_state = LockBufHdr(bufHdr);

    if ((buf_state & mask) == mask)
    {
      CkptSortItem *item;

      buf_state |= BM_CHECKPOINT_NEEDED;

      item = &CkptBufferIds[num_to_scan++];
      item->buf_id = buf_id;
      item->tsId = bufHdr->tag.rnode.spcNode;
      item->relNode = bufHdr->tag.rnode.relNode;
      item->forkNum = bufHdr->tag.forkNum;
      item->blockNum = bufHdr->tag.blockNum;
    }

    UnlockBufHdr(bufHdr, buf_state);
  }

  if (num_to_scan == 0)
  {
    return;                    
  }

  WritebackContextInit(&wb_context, &checkpoint_flush_after);

  TRACE_POSTGRESQL_BUFFER_SYNC_START(NBuffers, num_to_scan);

     
                                                                             
                                                                           
                                                                           
                                                                            
                        
     
  qsort(CkptBufferIds, num_to_scan, sizeof(CkptSortItem), ckpt_buforder_comparator);

  num_spaces = 0;

     
                                                                            
                                                                     
     
  last_tsid = InvalidOid;
  for (i = 0; i < num_to_scan; i++)
  {
    CkptTsStatus *s;
    Oid cur_tsid;

    cur_tsid = CkptBufferIds[i].tsId;

       
                                                                     
                            
       
    if (last_tsid == InvalidOid || last_tsid != cur_tsid)
    {
      Size sz;

      num_spaces++;

         
                                                                      
                                                      
         
      sz = sizeof(CkptTsStatus) * num_spaces;

      if (per_ts_stat == NULL)
      {
        per_ts_stat = (CkptTsStatus *)palloc(sz);
      }
      else
      {
        per_ts_stat = (CkptTsStatus *)repalloc(per_ts_stat, sz);
      }

      s = &per_ts_stat[num_spaces - 1];
      memset(s, 0, sizeof(*s));
      s->tsId = cur_tsid;

         
                                                                         
                                                                       
                                 
         
      s->index = i;

         
                                                                         
                                                       
         

      last_tsid = cur_tsid;
    }
    else
    {
      s = &per_ts_stat[num_spaces - 1];
    }

    s->num_to_scan++;
  }

  Assert(num_spaces > 0);

     
                                                                             
                                                                    
                          
     
  ts_heap = binaryheap_allocate(num_spaces, ts_ckpt_progress_comparator, NULL);

  for (i = 0; i < num_spaces; i++)
  {
    CkptTsStatus *ts_stat = &per_ts_stat[i];

    ts_stat->progress_slice = (float8)num_to_scan / ts_stat->num_to_scan;

    binaryheap_add_unordered(ts_heap, PointerGetDatum(ts_stat));
  }

  binaryheap_build(ts_heap);

     
                                                                           
                                                                       
                                                                          
                                                                         
     
  num_processed = 0;
  num_written = 0;
  while (!binaryheap_empty(ts_heap))
  {
    BufferDesc *bufHdr = NULL;
    CkptTsStatus *ts_stat = (CkptTsStatus *)DatumGetPointer(binaryheap_first(ts_heap));

    buf_id = CkptBufferIds[ts_stat->index].buf_id;
    Assert(buf_id != -1);

    bufHdr = GetBufferDescriptor(buf_id);

    num_processed++;

       
                                                                          
                                                                          
                                                                         
                                                                      
                                                                         
                                                                          
                                                                           
                                                                         
                                                                         
                                      
       
    if (pg_atomic_read_u32(&bufHdr->state) & BM_CHECKPOINT_NEEDED)
    {
      if (SyncOneBuffer(buf_id, false, &wb_context) & BUF_WRITTEN)
      {
        TRACE_POSTGRESQL_BUFFER_SYNC_WRITTEN(buf_id);
        BgWriterStats.m_buf_written_checkpoints++;
        num_written++;
      }
    }

       
                                                                           
                                              
       
    ts_stat->progress += ts_stat->progress_slice;
    ts_stat->num_scanned++;
    ts_stat->index++;

                                                                  
    if (ts_stat->num_scanned == ts_stat->num_to_scan)
    {
      binaryheap_remove_first(ts_heap);
    }
    else
    {
                                             
      binaryheap_replace_first(ts_heap, PointerGetDatum(ts_stat));
    }

       
                                       
       
    CheckpointWriteDelay(flags, (double)num_processed / num_to_scan);
  }

                                 
  IssuePendingWritebacks(&wb_context);

  pfree(per_ts_stat);
  per_ts_stat = NULL;
  binaryheap_free(ts_heap);

     
                                                                        
                                                         
     
  CheckpointStats.ckpt_bufs_written += num_written;

  TRACE_POSTGRESQL_BUFFER_SYNC_DONE(NBuffers, num_written, num_to_scan);
}

   
                                                             
   
                                                                 
   
                                                                        
                                                                          
                                                                       
                                                               
                                
   
bool
BgBufferSync(WritebackContext *wb_context)
{
                                     
  int strategy_buf_id;
  uint32 strategy_passes;
  uint32 recent_alloc;

     
                                                                      
                                                                      
     
  static bool saved_info_valid = false;
  static int prev_strategy_buf_id;
  static uint32 prev_strategy_passes;
  static int next_to_clean;
  static uint32 next_passes;

                                                                   
  static float smoothed_alloc = 0;
  static float smoothed_density = 10.0;

                                                             
  float smoothing_samples = 16;
  float scan_whole_pool_milliseconds = 120000.0;

                                             
  long strategy_delta;
  int bufs_to_lap;
  int bufs_ahead;
  float scans_per_alloc;
  int reusable_buffers_est;
  int upcoming_alloc_est;
  int min_scan_buffers;

                                              
  int num_to_scan;
  int num_written;
  int reusable_buffers;

                                                   
  long new_strategy_delta;
  uint32 new_recent_alloc;

     
                                                                        
                                                           
     
  strategy_buf_id = StrategySyncStart(&strategy_passes, &recent_alloc);

                                            
  BgWriterStats.m_buf_alloc += recent_alloc;

     
                                                                        
                                                                           
                                          
     
  if (bgwriter_lru_maxpages <= 0)
  {
    saved_info_valid = false;
    return true;
  }

     
                                                                        
                                                                            
                                                                       
                                                                            
                                                                       
                                                  
     
  if (saved_info_valid)
  {
    int32 passes_delta = strategy_passes - prev_strategy_passes;

    strategy_delta = strategy_buf_id - prev_strategy_buf_id;
    strategy_delta += (long)passes_delta * NBuffers;

    Assert(strategy_delta >= 0);

    if ((int32)(next_passes - strategy_passes) > 0)
    {
                                                      
      bufs_to_lap = strategy_buf_id - next_to_clean;
#ifdef BGW_DEBUG
      elog(DEBUG2, "bgwriter ahead: bgw %u-%u strategy %u-%u delta=%ld lap=%d", next_passes, next_to_clean, strategy_passes, strategy_buf_id, strategy_delta, bufs_to_lap);
#endif
    }
    else if (next_passes == strategy_passes && next_to_clean >= strategy_buf_id)
    {
                                                          
      bufs_to_lap = NBuffers - (next_to_clean - strategy_buf_id);
#ifdef BGW_DEBUG
      elog(DEBUG2, "bgwriter ahead: bgw %u-%u strategy %u-%u delta=%ld lap=%d", next_passes, next_to_clean, strategy_passes, strategy_buf_id, strategy_delta, bufs_to_lap);
#endif
    }
    else
    {
         
                                                                       
                              
         
#ifdef BGW_DEBUG
      elog(DEBUG2, "bgwriter behind: bgw %u-%u strategy %u-%u delta=%ld", next_passes, next_to_clean, strategy_passes, strategy_buf_id, strategy_delta);
#endif
      next_to_clean = strategy_buf_id;
      next_passes = strategy_passes;
      bufs_to_lap = NBuffers;
    }
  }
  else
  {
       
                                                                          
                                    
       
#ifdef BGW_DEBUG
    elog(DEBUG2, "bgwriter initializing: strategy %u-%u", strategy_passes, strategy_buf_id);
#endif
    strategy_delta = 0;
    next_to_clean = strategy_buf_id;
    next_passes = strategy_passes;
    bufs_to_lap = NBuffers;
  }

                                       
  prev_strategy_buf_id = strategy_buf_id;
  prev_strategy_passes = strategy_passes;
  saved_info_valid = true;

     
                                                                             
                                                                        
     
                                                                             
     
  if (strategy_delta > 0 && recent_alloc > 0)
  {
    scans_per_alloc = (float)strategy_delta / (float)recent_alloc;
    smoothed_density += (scans_per_alloc - smoothed_density) / smoothing_samples;
  }

     
                                                                      
                                                                            
                       
     
  bufs_ahead = NBuffers - bufs_to_lap;
  reusable_buffers_est = (float)bufs_ahead / smoothed_density;

     
                                                                             
                                                                     
                                      
     
  if (smoothed_alloc <= (float)recent_alloc)
  {
    smoothed_alloc = recent_alloc;
  }
  else
  {
    smoothed_alloc += ((float)recent_alloc - smoothed_alloc) / smoothing_samples;
  }

                                                                    
  upcoming_alloc_est = (int)(smoothed_alloc * bgwriter_lru_multiplier);

     
                                                                          
                                                                       
                                                                             
                                                                      
                                                                    
                                                                       
     
  if (upcoming_alloc_est == 0)
  {
    smoothed_alloc = 0;
  }

     
                                                                     
                                                                             
                                                                           
                  
     
                                                                            
                                                                       
                                          
     
  min_scan_buffers = (int)(NBuffers / (scan_whole_pool_milliseconds / BgWriterDelay));

  if (upcoming_alloc_est < (min_scan_buffers + reusable_buffers_est))
  {
#ifdef BGW_DEBUG
    elog(DEBUG2, "bgwriter: alloc_est=%d too small, using min=%d + reusable_est=%d", upcoming_alloc_est, min_scan_buffers, reusable_buffers_est);
#endif
    upcoming_alloc_est = min_scan_buffers + reusable_buffers_est;
  }

     
                                                                    
                                                                             
                                                                         
                                                           
     

                                                            
  ResourceOwnerEnlargeBuffers(CurrentResourceOwner);

  num_to_scan = bufs_to_lap;
  num_written = 0;
  reusable_buffers = reusable_buffers_est;

                            
  while (num_to_scan > 0 && reusable_buffers < upcoming_alloc_est)
  {
    int sync_state = SyncOneBuffer(next_to_clean, true, wb_context);

    if (++next_to_clean >= NBuffers)
    {
      next_to_clean = 0;
      next_passes++;
    }
    num_to_scan--;

    if (sync_state & BUF_WRITTEN)
    {
      reusable_buffers++;
      if (++num_written >= bgwriter_lru_maxpages)
      {
        BgWriterStats.m_maxwritten_clean++;
        break;
      }
    }
    else if (sync_state & BUF_REUSABLE)
    {
      reusable_buffers++;
    }
  }

  BgWriterStats.m_buf_written_clean += num_written;

#ifdef BGW_DEBUG
  elog(DEBUG1, "bgwriter: recent_alloc=%u smoothed=%.2f delta=%ld ahead=%d density=%.2f reusable_est=%d upcoming_est=%d scanned=%d wrote=%d reusable=%d", recent_alloc, smoothed_alloc, strategy_delta, bufs_ahead, smoothed_density, reusable_buffers_est, upcoming_alloc_est, bufs_to_lap - num_to_scan, num_written, reusable_buffers - reusable_buffers_est);
#endif

     
                                                                  
                                                                            
                                                                          
                                                                        
                                                                      
                        
     
  new_strategy_delta = bufs_to_lap - num_to_scan;
  new_recent_alloc = reusable_buffers - reusable_buffers_est;
  if (new_strategy_delta > 0 && new_recent_alloc > 0)
  {
    scans_per_alloc = (float)new_strategy_delta / (float)new_recent_alloc;
    smoothed_density += (scans_per_alloc - smoothed_density) / smoothing_samples;

#ifdef BGW_DEBUG
    elog(DEBUG2, "bgwriter: cleaner density alloc=%u scan=%ld density=%.2f new smoothed=%.2f", new_recent_alloc, new_strategy_delta, scans_per_alloc, smoothed_density);
#endif
  }

                                      
  return (bufs_to_lap == 0 && recent_alloc == 0);
}

   
                                                            
   
                                                                               
                                                                          
   
                                                         
                                     
                                                                 
                                   
   
                                                                             
                                                       
   
                                                            
   
static int
SyncOneBuffer(int buf_id, bool skip_recently_used, WritebackContext *wb_context)
{
  BufferDesc *bufHdr = GetBufferDescriptor(buf_id);
  int result = 0;
  uint32 buf_state;
  BufferTag tag;

  ReservePrivateRefCountEntry();

     
                                         
     
                                                                           
                                                                            
                                                                             
                                                                          
                                                                             
     
  buf_state = LockBufHdr(bufHdr);

  if (BUF_STATE_GET_REFCOUNT(buf_state) == 0 && BUF_STATE_GET_USAGECOUNT(buf_state) == 0)
  {
    result |= BUF_REUSABLE;
  }
  else if (skip_recently_used)
  {
                                                           
    UnlockBufHdr(bufHdr, buf_state);
    return result;
  }

  if (!(buf_state & BM_VALID) || !(buf_state & BM_DIRTY))
  {
                                      
    UnlockBufHdr(bufHdr, buf_state);
    return result;
  }

     
                                                                           
                                                   
     
  PinBuffer_Locked(bufHdr);
  LWLockAcquire(BufferDescriptorGetContentLock(bufHdr), LW_SHARED);

  FlushBuffer(bufHdr, NULL);

  LWLockRelease(BufferDescriptorGetContentLock(bufHdr));

  tag = bufHdr->tag;

  UnpinBuffer(bufHdr, true);

  ScheduleBufferTagForWriteback(wb_context, &tag);

  return result | BUF_WRITTEN;
}

   
                                                       
   
                                                                 
                                                               
                                     
   
void
AtEOXact_Buffers(bool isCommit)
{
  CheckForBufferLeaks();

  AtEOXact_LocalBuffers(isCommit);

  Assert(PrivateRefCountOverflowed == 0);
}

   
                                           
   
                                                                          
                                                                              
                
   
                                                                           
                                                                    
                                                                 
                                    
   
void
InitBufferPoolAccess(void)
{
  HASHCTL hash_ctl;

  memset(&PrivateRefCountArray, 0, sizeof(PrivateRefCountArray));

  MemSet(&hash_ctl, 0, sizeof(hash_ctl));
  hash_ctl.keysize = sizeof(int32);
  hash_ctl.entrysize = sizeof(PrivateRefCountEntry);

  PrivateRefCountHash = hash_create("PrivateRefCount", 100, &hash_ctl, HASH_ELEM | HASH_BLOBS);
}

   
                                                                          
   
                                                                        
                                                                      
                                                                           
                                                                      
                     
   
void
InitBufferPoolBackend(void)
{
  on_shmem_exit(AtProcExit_Buffers, 0);
}

   
                                                                            
                                          
   
static void
AtProcExit_Buffers(int code, Datum arg)
{
  AbortBufferIO();
  UnlockBuffers();

  CheckForBufferLeaks();

                                     
  AtProcExit_LocalBuffers();
}

   
                                                                   
   
                                                                 
                                                               
                                     
   
static void
CheckForBufferLeaks(void)
{
#ifdef USE_ASSERT_CHECKING
  int RefCountErrors = 0;
  PrivateRefCountEntry *res;
  int i;

                       
  for (i = 0; i < REFCOUNT_ARRAY_ENTRIES; i++)
  {
    res = &PrivateRefCountArray[i];

    if (res->buffer != InvalidBuffer)
    {
      PrintBufferLeakWarning(res->buffer);
      RefCountErrors++;
    }
  }

                                    
  if (PrivateRefCountOverflowed)
  {
    HASH_SEQ_STATUS hstat;

    hash_seq_init(&hstat, PrivateRefCountHash);
    while ((res = (PrivateRefCountEntry *)hash_seq_search(&hstat)) != NULL)
    {
      PrintBufferLeakWarning(res->buffer);
      RefCountErrors++;
    }
  }

  Assert(RefCountErrors == 0);
#endif
}

   
                                                                         
   
void
PrintBufferLeakWarning(Buffer buffer)
{
  BufferDesc *buf;
  int32 loccount;
  char *path;
  BackendId backend;
  uint32 buf_state;

  Assert(BufferIsValid(buffer));
  if (BufferIsLocal(buffer))
  {
    buf = GetLocalBufferDescriptor(-buffer - 1);
    loccount = LocalRefCount[-buffer - 1];
    backend = MyBackendId;
  }
  else
  {
    buf = GetBufferDescriptor(buffer - 1);
    loccount = GetPrivateRefCount(buffer);
    backend = InvalidBackendId;
  }

                                                    
  path = relpathbackend(buf->tag.rnode, backend, buf->tag.forkNum);
  buf_state = pg_atomic_read_u32(&buf->state);
  elog(WARNING,
      "buffer refcount leak: [%03d] "
      "(rel=%s, blockNum=%u, flags=0x%x, refcount=%u %d)",
      buffer, path, buf->tag.blockNum, buf_state & BUF_FLAG_MASK, BUF_STATE_GET_REFCOUNT(buf_state), loccount);
  pfree(path);
}

   
                     
   
                                                                     
   
                                                                              
                       
   
void
CheckPointBuffers(int flags)
{
  TRACE_POSTGRESQL_BUFFER_CHECKPOINT_START(flags);
  CheckpointStats.ckpt_write_t = GetCurrentTimestamp();
  BufferSync(flags);
  CheckpointStats.ckpt_sync_t = GetCurrentTimestamp();
  TRACE_POSTGRESQL_BUFFER_CHECKPOINT_SYNC_START();
  ProcessSyncRequests();
  CheckpointStats.ckpt_sync_end_t = GetCurrentTimestamp();
  TRACE_POSTGRESQL_BUFFER_CHECKPOINT_DONE();
}

   
                                                                             
   
void
BufmgrCommit(void)
{
                                          
}

   
                        
                                                       
   
         
                                                          
                                         
   
BlockNumber
BufferGetBlockNumber(Buffer buffer)
{
  BufferDesc *bufHdr;

  Assert(BufferIsPinned(buffer));

  if (BufferIsLocal(buffer))
  {
    bufHdr = GetLocalBufferDescriptor(-buffer - 1);
  }
  else
  {
    bufHdr = GetBufferDescriptor(buffer - 1);
  }

                                                  
  return bufHdr->tag.blockNum;
}

   
                
                                                                          
              
   
void
BufferGetTag(Buffer buffer, RelFileNode *rnode, ForkNumber *forknum, BlockNumber *blknum)
{
  BufferDesc *bufHdr;

                                                   
  Assert(BufferIsPinned(buffer));

  if (BufferIsLocal(buffer))
  {
    bufHdr = GetLocalBufferDescriptor(-buffer - 1);
  }
  else
  {
    bufHdr = GetBufferDescriptor(buffer - 1);
  }

                                                  
  *rnode = bufHdr->tag.rnode;
  *forknum = bufHdr->tag.forkNum;
  *blknum = bufHdr->tag.blockNum;
}

   
               
                                          
   
                                                                          
                                                                         
                                                                          
                                                                       
                          
   
                                                                      
                                                                     
                                                                     
                                                                        
             
   
                                                                          
                                                
   
static void
FlushBuffer(BufferDesc *buf, SMgrRelation reln)
{
  XLogRecPtr recptr;
  ErrorContextCallback errcallback;
  instr_time io_start, io_time;
  Block bufBlock;
  char *bufToWrite;
  uint32 buf_state;

     
                                                                         
                                                                             
                      
     
  if (!StartBufferIO(buf, false))
  {
    return;
  }

                                                   
  errcallback.callback = shared_buffer_write_error_callback;
  errcallback.arg = (void *)buf;
  errcallback.previous = error_context_stack;
  error_context_stack = &errcallback;

                                     
  if (reln == NULL)
  {
    reln = smgropen(buf->tag.rnode, InvalidBackendId);
  }

  TRACE_POSTGRESQL_BUFFER_FLUSH_START(buf->tag.forkNum, buf->tag.blockNum, reln->smgr_rnode.node.spcNode, reln->smgr_rnode.node.dbNode, reln->smgr_rnode.node.relNode);

  buf_state = LockBufHdr(buf);

     
                                                                       
                                             
     
  recptr = BufferGetLSN(buf);

                                                                          
  buf_state &= ~BM_JUST_DIRTIED;
  UnlockBufHdr(buf, buf_state);

     
                                                                         
                                                                             
                       
     
                                                                            
                                                                          
                                                                           
                                                                         
                                                                          
                                                       
                                                                          
                                                                           
                                                                            
                                                                           
                                                   
     
  if (buf_state & BM_PERMANENT)
  {
    XLogFlush(recptr);
  }

     
                                                                         
                                                                             
                                      
     
  bufBlock = BufHdrGetBlock(buf);

     
                                                                             
                                                                           
                                                             
     
  bufToWrite = PageSetChecksumCopy((Page)bufBlock, buf->tag.blockNum);

  if (track_io_timing)
  {
    INSTR_TIME_SET_CURRENT(io_start);
  }

     
                                                                       
     
  smgrwrite(reln, buf->tag.forkNum, buf->tag.blockNum, bufToWrite, false);

  if (track_io_timing)
  {
    INSTR_TIME_SET_CURRENT(io_time);
    INSTR_TIME_SUBTRACT(io_time, io_start);
    pgstat_count_buffer_write_time(INSTR_TIME_GET_MICROSEC(io_time));
    INSTR_TIME_ADD(pgBufferUsage.blk_write_time, io_time);
  }

  pgBufferUsage.shared_blks_written++;

     
                                                                          
                                   
     
  TerminateBufferIO(buf, true, 0);

  TRACE_POSTGRESQL_BUFFER_FLUSH_DONE(buf->tag.forkNum, buf->tag.blockNum, reln->smgr_rnode.node.spcNode, reln->smgr_rnode.node.dbNode, reln->smgr_rnode.node.relNode);

                                   
  error_context_stack = errcallback.previous;
}

   
                                   
                                                                           
   
                                                                          
                                                                               
                    
   
BlockNumber
RelationGetNumberOfBlocksInFork(Relation relation, ForkNumber forkNum)
{
  switch (relation->rd_rel->relkind)
  {
  case RELKIND_SEQUENCE:
  case RELKIND_INDEX:
  case RELKIND_PARTITIONED_INDEX:
    return smgrnblocks(RelationGetSmgr(relation), forkNum);

  case RELKIND_RELATION:
  case RELKIND_TOASTVALUE:
  case RELKIND_MATVIEW:
  {
       
                                                              
                                                                 
                                                              
                                      
       
    uint64 szbytes;

    szbytes = table_relation_size(relation, forkNum);

    return (szbytes + (BLCKSZ - 1)) / BLCKSZ;
  }
  case RELKIND_VIEW:
  case RELKIND_COMPOSITE_TYPE:
  case RELKIND_FOREIGN_TABLE:
  case RELKIND_PARTITIONED_TABLE:
  default:
    Assert(false);
    break;
  }

  return 0;                          
}

   
                     
                                                                       
                                             
   
bool
BufferIsPermanent(Buffer buffer)
{
  BufferDesc *bufHdr;

                                                       
  if (BufferIsLocal(buffer))
  {
    return false;
  }

                                                                        
  Assert(BufferIsValid(buffer));
  Assert(BufferIsPinned(buffer));

     
                                                                            
                                                                            
                                                                          
                                                                           
                         
     
  bufHdr = GetBufferDescriptor(buffer - 1);
  return (pg_atomic_read_u32(&bufHdr->state) & BM_PERMANENT) != 0;
}

   
                      
                                                                           
                                                                          
                   
   
XLogRecPtr
BufferGetLSNAtomic(Buffer buffer)
{
  BufferDesc *bufHdr = GetBufferDescriptor(buffer - 1);
  char *page = BufferGetPage(buffer);
  XLogRecPtr lsn;
  uint32 buf_state;

     
                                                             
     
  if (!XLogHintBitIsNeeded() || BufferIsLocal(buffer))
  {
    return PageGetLSN(page);
  }

                                                                        
  Assert(BufferIsValid(buffer));
  Assert(BufferIsPinned(buffer));

  buf_state = LockBufHdr(bufHdr);
  lsn = PageGetLSN(page);
  UnlockBufHdr(bufHdr, buf_state);

  return lsn;
}

                                                                         
                           
   
                                                                    
                                                                      
                                                                    
                                                                    
                                                                       
                                    
   
                                                                        
                                                                     
                                                                          
                                                                        
                                                                     
                                                                       
                                                                         
                                                                    
                           
   
                                                                      
                                                                     
                                                                      
                                                                   
                                                                        
   
void
DropRelFileNodeBuffers(RelFileNodeBackend rnode, ForkNumber forkNum, BlockNumber firstDelBlock)
{
  int i;

                                                            
  if (RelFileNodeBackendIsTemp(rnode))
  {
    if (rnode.backend == MyBackendId)
    {
      DropRelFileNodeLocalBuffers(rnode.node, forkNum, firstDelBlock);
    }
    return;
  }

  for (i = 0; i < NBuffers; i++)
  {
    BufferDesc *bufHdr = GetBufferDescriptor(i);
    uint32 buf_state;

       
                                                                          
                                                               
                                                                     
                                                                           
                                                                        
                                                                       
                                                                       
                                                                         
                                                                         
                                                                        
                    
       
                                                                         
                                                  
       
    if (!RelFileNodeEquals(bufHdr->tag.rnode, rnode.node))
    {
      continue;
    }

    buf_state = LockBufHdr(bufHdr);
    if (RelFileNodeEquals(bufHdr->tag.rnode, rnode.node) && bufHdr->tag.forkNum == forkNum && bufHdr->tag.blockNum >= firstDelBlock)
    {
      InvalidateBuffer(bufHdr);                        
    }
    else
    {
      UnlockBufHdr(bufHdr, buf_state);
    }
  }
}

                                                                         
                               
   
                                                                    
                                                                  
                                                           
                       
                                                                        
   
void
DropRelFileNodesAllBuffers(RelFileNodeBackend *rnodes, int nnodes)
{
  int i, n = 0;
  RelFileNode *nodes;
  bool use_bsearch;

  if (nnodes == 0)
  {
    return;
  }

  nodes = palloc(sizeof(RelFileNode) * nnodes);                          

                                                            
  for (i = 0; i < nnodes; i++)
  {
    if (RelFileNodeBackendIsTemp(rnodes[i]))
    {
      if (rnodes[i].backend == MyBackendId)
      {
        DropRelFileNodeAllLocalBuffers(rnodes[i].node);
      }
    }
    else
    {
      nodes[n++] = rnodes[i].node;
    }
  }

     
                                                                       
                        
     
  if (n == 0)
  {
    pfree(nodes);
    return;
  }

     
                                                                            
                                                                            
                                                                             
                                             
     
  use_bsearch = n > DROP_RELS_BSEARCH_THRESHOLD;

                                            
  if (use_bsearch)
  {
    pg_qsort(nodes, n, sizeof(RelFileNode), rnode_comparator);
  }

  for (i = 0; i < NBuffers; i++)
  {
    RelFileNode *rnode = NULL;
    BufferDesc *bufHdr = GetBufferDescriptor(i);
    uint32 buf_state;

       
                                                                         
                              
       

    if (!use_bsearch)
    {
      int j;

      for (j = 0; j < n; j++)
      {
        if (RelFileNodeEquals(bufHdr->tag.rnode, nodes[j]))
        {
          rnode = &nodes[j];
          break;
        }
      }
    }
    else
    {
      rnode = bsearch((const void *)&(bufHdr->tag.rnode), nodes, n, sizeof(RelFileNode), rnode_comparator);
    }

                                                                         
    if (rnode == NULL)
    {
      continue;
    }

    buf_state = LockBufHdr(bufHdr);
    if (RelFileNodeEquals(bufHdr->tag.rnode, (*rnode)))
    {
      InvalidateBuffer(bufHdr);                        
    }
    else
    {
      UnlockBufHdr(bufHdr, buf_state);
    }
  }

  pfree(nodes);
}

                                                                         
                        
   
                                                                    
                                                                  
                                                                       
                                                                       
                                                                
                                                                        
                                                                        
   
void
DropDatabaseBuffers(Oid dbid)
{
  int i;

     
                                                                       
                             
     

  for (i = 0; i < NBuffers; i++)
  {
    BufferDesc *bufHdr = GetBufferDescriptor(i);
    uint32 buf_state;

       
                                                                         
                              
       
    if (bufHdr->tag.rnode.dbNode != dbid)
    {
      continue;
    }

    buf_state = LockBufHdr(bufHdr);
    if (bufHdr->tag.rnode.dbNode == dbid)
    {
      InvalidateBuffer(bufHdr);                        
    }
    else
    {
      UnlockBufHdr(bufHdr, buf_state);
    }
  }
}

                                                                     
                     
   
                                                                   
              
                                                                     
   
#ifdef NOT_USED
void
PrintBufferDescs(void)
{
  int i;

  for (i = 0; i < NBuffers; ++i)
  {
    BufferDesc *buf = GetBufferDescriptor(i);
    Buffer b = BufferDescriptorGetBuffer(buf);

                                                      
    elog(LOG,
        "[%02d] (freeNext=%d, rel=%s, "
        "blockNum=%u, flags=0x%x, refcount=%u %d)",
        i, buf->freeNext, relpathbackend(buf->tag.rnode, InvalidBackendId, buf->tag.forkNum), buf->tag.blockNum, buf->flags, buf->refcount, GetPrivateRefCount(b));
  }
}
#endif

#ifdef NOT_USED
void
PrintPinnedBufs(void)
{
  int i;

  for (i = 0; i < NBuffers; ++i)
  {
    BufferDesc *buf = GetBufferDescriptor(i);
    Buffer b = BufferDescriptorGetBuffer(buf);

    if (GetPrivateRefCount(b) > 0)
    {
                                                        
      elog(LOG,
          "[%02d] (freeNext=%d, rel=%s, "
          "blockNum=%u, flags=0x%x, refcount=%u %d)",
          i, buf->freeNext, relpathperm(buf->tag.rnode, buf->tag.forkNum), buf->tag.blockNum, buf->flags, buf->refcount, GetPrivateRefCount(b));
    }
  }
}
#endif

                                                                         
                         
   
                                                                   
                                                                        
                                                   
   
                                                                       
                                                                     
                                                                       
                                
   
                                                                      
                                                                   
                                                                   
                                                                     
                                         
                                                                        
   
void
FlushRelationBuffers(Relation rel)
{
  int i;
  BufferDesc *bufHdr;

  if (RelationUsesLocalBuffers(rel))
  {
    for (i = 0; i < NLocBuffer; i++)
    {
      uint32 buf_state;

      bufHdr = GetLocalBufferDescriptor(i);
      if (RelFileNodeEquals(bufHdr->tag.rnode, rel->rd_node) && ((buf_state = pg_atomic_read_u32(&bufHdr->state)) & (BM_VALID | BM_DIRTY)) == (BM_VALID | BM_DIRTY))
      {
        ErrorContextCallback errcallback;
        Page localpage;

        localpage = (char *)LocalBufHdrGetBlock(bufHdr);

                                                         
        errcallback.callback = local_buffer_write_error_callback;
        errcallback.arg = (void *)bufHdr;
        errcallback.previous = error_context_stack;
        error_context_stack = &errcallback;

        PageSetChecksumInplace(localpage, bufHdr->tag.blockNum);

        smgrwrite(RelationGetSmgr(rel), bufHdr->tag.forkNum, bufHdr->tag.blockNum, localpage, false);

        buf_state &= ~(BM_DIRTY | BM_JUST_DIRTIED);
        pg_atomic_unlocked_write_u32(&bufHdr->state, buf_state);

                                         
        error_context_stack = errcallback.previous;
      }
    }

    return;
  }

                                                       
  ResourceOwnerEnlargeBuffers(CurrentResourceOwner);

  for (i = 0; i < NBuffers; i++)
  {
    uint32 buf_state;

    bufHdr = GetBufferDescriptor(i);

       
                                                                         
                              
       
    if (!RelFileNodeEquals(bufHdr->tag.rnode, rel->rd_node))
    {
      continue;
    }

    ReservePrivateRefCountEntry();

    buf_state = LockBufHdr(bufHdr);
    if (RelFileNodeEquals(bufHdr->tag.rnode, rel->rd_node) && (buf_state & (BM_VALID | BM_DIRTY)) == (BM_VALID | BM_DIRTY))
    {
      PinBuffer_Locked(bufHdr);
      LWLockAcquire(BufferDescriptorGetContentLock(bufHdr), LW_SHARED);
      FlushBuffer(bufHdr, RelationGetSmgr(rel));
      LWLockRelease(BufferDescriptorGetContentLock(bufHdr));
      UnpinBuffer(bufHdr, true);
    }
    else
    {
      UnlockBufHdr(bufHdr, buf_state);
    }
  }
}

                                                                         
                         
   
                                                                   
                                                                        
                                                   
   
                                                                          
                                                                      
                             
   
                                                                         
                                                
                                                                        
   
void
FlushDatabaseBuffers(Oid dbid)
{
  int i;
  BufferDesc *bufHdr;

                                                       
  ResourceOwnerEnlargeBuffers(CurrentResourceOwner);

  for (i = 0; i < NBuffers; i++)
  {
    uint32 buf_state;

    bufHdr = GetBufferDescriptor(i);

       
                                                                         
                              
       
    if (bufHdr->tag.rnode.dbNode != dbid)
    {
      continue;
    }

    ReservePrivateRefCountEntry();

    buf_state = LockBufHdr(bufHdr);
    if (bufHdr->tag.rnode.dbNode == dbid && (buf_state & (BM_VALID | BM_DIRTY)) == (BM_VALID | BM_DIRTY))
    {
      PinBuffer_Locked(bufHdr);
      LWLockAcquire(BufferDescriptorGetContentLock(bufHdr), LW_SHARED);
      FlushBuffer(bufHdr, NULL);
      LWLockRelease(BufferDescriptorGetContentLock(bufHdr));
      UnpinBuffer(bufHdr, true);
    }
    else
    {
      UnlockBufHdr(bufHdr, buf_state);
    }
  }
}

   
                                                                              
       
   
void
FlushOneBuffer(Buffer buffer)
{
  BufferDesc *bufHdr;

                                                                      
  Assert(!BufferIsLocal(buffer));

  Assert(BufferIsPinned(buffer));

  bufHdr = GetBufferDescriptor(buffer - 1);

  Assert(LWLockHeldByMe(BufferDescriptorGetContentLock(bufHdr)));

  FlushBuffer(bufHdr, NULL);
}

   
                                                
   
void
ReleaseBuffer(Buffer buffer)
{
  if (!BufferIsValid(buffer))
  {
    elog(ERROR, "bad buffer ID: %d", buffer);
  }

  if (BufferIsLocal(buffer))
  {
    ResourceOwnerForgetBuffer(CurrentResourceOwner, buffer);

    Assert(LocalRefCount[-buffer - 1] > 0);
    LocalRefCount[-buffer - 1]--;
    return;
  }

  UnpinBuffer(GetBufferDescriptor(buffer - 1), true);
}

   
                                                                       
   
                                                      
   
void
UnlockReleaseBuffer(Buffer buffer)
{
  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
  ReleaseBuffer(buffer);
}

   
                      
                                                                      
                   
   
                                                                    
                                                       
   
void
IncrBufferRefCount(Buffer buffer)
{
  Assert(BufferIsPinned(buffer));
  ResourceOwnerEnlargeBuffers(CurrentResourceOwner);
  if (BufferIsLocal(buffer))
  {
    LocalRefCount[-buffer - 1]++;
  }
  else
  {
    PrivateRefCountEntry *ref;

    ref = GetPrivateRefCountEntry(buffer, true);
    Assert(ref != NULL);
    ref->refcount++;
  }
  ResourceOwnerRememberBuffer(CurrentResourceOwner, buffer);
}

   
                       
   
                                                 
   
                                                            
   
                                                                              
                                                                    
                                                                             
                            
                                                                              
                                                                            
   
void
MarkBufferDirtyHint(Buffer buffer, bool buffer_std)
{
  BufferDesc *bufHdr;
  Page page = BufferGetPage(buffer);

  if (!BufferIsValid(buffer))
  {
    elog(ERROR, "bad buffer ID: %d", buffer);
  }

  if (BufferIsLocal(buffer))
  {
    MarkLocalBufferDirty(buffer);
    return;
  }

  bufHdr = GetBufferDescriptor(buffer - 1);

  Assert(GetPrivateRefCount(buffer) > 0);
                                                  
  Assert(LWLockHeldByMe(BufferDescriptorGetContentLock(bufHdr)));

     
                                                                          
                                                                           
                                                                           
                                                                            
                                                                         
                                                                            
                                                                            
                                                                         
                                                              
     
  if ((pg_atomic_read_u32(&bufHdr->state) & (BM_DIRTY | BM_JUST_DIRTIED)) != (BM_DIRTY | BM_JUST_DIRTIED))
  {
    XLogRecPtr lsn = InvalidXLogRecPtr;
    bool dirtied = false;
    bool delayChkpt = false;
    uint32 buf_state;

       
                                                                          
                                                                           
                                                                        
                        
       
                                                                           
                                                                      
       
    if (XLogHintBitIsNeeded() && (pg_atomic_read_u32(&bufHdr->state) & BM_PERMANENT))
    {
         
                                                                        
                                                                         
                                                          
         
                                                                    
         
      if (RecoveryInProgress())
      {
        return;
      }

         
                                                                       
                                                                        
                                                                         
                                                                        
                                            
         
                                                                       
                                                                         
                                                                         
                                                                       
                                                                  
                                                                      
                                                                     
                                                                   
                                                                        
                                                                      
                                                         
         
                                                                  
                                                                        
                                          
         
      Assert(!MyPgXact->delayChkpt);
      MyPgXact->delayChkpt = true;
      delayChkpt = true;
      lsn = XLogSaveBufferForHint(buffer, buffer_std);
    }

    buf_state = LockBufHdr(bufHdr);

    Assert(BUF_STATE_GET_REFCOUNT(buf_state) > 0);

    if (!(buf_state & BM_DIRTY))
    {
      dirtied = true;                                             

         
                                                                         
                                                                      
                                                                   
                                                                       
                                                                        
                                                              
                               
         
                                                                       
                                                                  
                                                  
         
      if (!XLogRecPtrIsInvalid(lsn))
      {
        PageSetLSN(page, lsn);
      }
    }

    buf_state |= BM_DIRTY | BM_JUST_DIRTIED;
    UnlockBufHdr(bufHdr, buf_state);

    if (delayChkpt)
    {
      MyPgXact->delayChkpt = false;
    }

    if (dirtied)
    {
      VacuumPageDirty++;
      pgBufferUsage.shared_blks_dirtied++;
      if (VacuumCostActive)
      {
        VacuumCostBalance += VacuumCostPageDirty;
      }
    }
  }
}

   
                                                    
   
                                  
   
                                                                         
                                                                            
                                                                     
   
void
UnlockBuffers(void)
{
  BufferDesc *buf = PinCountWaitBuf;

  if (buf)
  {
    uint32 buf_state;

    buf_state = LockBufHdr(buf);

       
                                                                           
                                                             
       
    if ((buf_state & BM_PIN_COUNT_WAITER) != 0 && buf->wait_backend_pid == MyProcPid)
    {
      buf_state &= ~BM_PIN_COUNT_WAITER;
    }

    UnlockBufHdr(buf, buf_state);

    PinCountWaitBuf = NULL;
  }
}

   
                                                       
   
void
LockBuffer(Buffer buffer, int mode)
{
  BufferDesc *buf;

  Assert(BufferIsValid(buffer));
  if (BufferIsLocal(buffer))
  {
    return;                                 
  }

  buf = GetBufferDescriptor(buffer - 1);

  if (mode == BUFFER_LOCK_UNLOCK)
  {
    LWLockRelease(BufferDescriptorGetContentLock(buf));
  }
  else if (mode == BUFFER_LOCK_SHARE)
  {
    LWLockAcquire(BufferDescriptorGetContentLock(buf), LW_SHARED);
  }
  else if (mode == BUFFER_LOCK_EXCLUSIVE)
  {
    LWLockAcquire(BufferDescriptorGetContentLock(buf), LW_EXCLUSIVE);
  }
  else
  {
    elog(ERROR, "unrecognized buffer lock mode: %d", mode);
  }
}

   
                                                                               
   
                                                             
   
bool
ConditionalLockBuffer(Buffer buffer)
{
  BufferDesc *buf;

  Assert(BufferIsValid(buffer));
  if (BufferIsLocal(buffer))
  {
    return true;                              
  }

  buf = GetBufferDescriptor(buffer - 1);

  return LWLockConditionalAcquire(BufferDescriptorGetContentLock(buf), LW_EXCLUSIVE);
}

   
                                                                          
   
                                                                           
                                                                           
                                                                         
                                                                           
                                                                           
                                                                        
                                                                   
   
                                                                          
                                                                           
                                                                         
                                               
   
void
LockBufferForCleanup(Buffer buffer)
{
  BufferDesc *bufHdr;

  Assert(BufferIsValid(buffer));
  Assert(PinCountWaitBuf == NULL);

  if (BufferIsLocal(buffer))
  {
                                         
    if (LocalRefCount[-buffer - 1] != 1)
    {
      elog(ERROR, "incorrect local pin count: %d", LocalRefCount[-buffer - 1]);
    }
                                 
    return;
  }

                                             
  if (GetPrivateRefCount(buffer) != 1)
  {
    elog(ERROR, "incorrect local pin count: %d", GetPrivateRefCount(buffer));
  }

  bufHdr = GetBufferDescriptor(buffer - 1);

  for (;;)
  {
    uint32 buf_state;

                             
    LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
    buf_state = LockBufHdr(bufHdr);

    Assert(BUF_STATE_GET_REFCOUNT(buf_state) > 0);
    if (BUF_STATE_GET_REFCOUNT(buf_state) == 1)
    {
                                                                
      UnlockBufHdr(bufHdr, buf_state);
      return;
    }
                                                          
    if (buf_state & BM_PIN_COUNT_WAITER)
    {
      UnlockBufHdr(bufHdr, buf_state);
      LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
      elog(ERROR, "multiple backends attempting to wait for pincount 1");
    }
    bufHdr->wait_backend_pid = MyProcPid;
    PinCountWaitBuf = bufHdr;
    buf_state |= BM_PIN_COUNT_WAITER;
    UnlockBufHdr(bufHdr, buf_state);
    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

                                              
    if (InHotStandby)
    {
                                                           
      SetStartupBufferPinWaitBufId(buffer - 1);
                                                                   
      ResolveRecoveryConflictWithBufferPin();
                                     
      SetStartupBufferPinWaitBufId(-1);
    }
    else
    {
      ProcWaitForSignal(PG_WAIT_BUFFER_PIN);
    }

       
                                                                       
                                                                        
                                                                          
                                                                        
                                                                          
                       
       
    buf_state = LockBufHdr(bufHdr);
    if ((buf_state & BM_PIN_COUNT_WAITER) != 0 && bufHdr->wait_backend_pid == MyProcPid)
    {
      buf_state &= ~BM_PIN_COUNT_WAITER;
    }
    UnlockBufHdr(bufHdr, buf_state);

    PinCountWaitBuf = NULL;
                                 
  }
}

   
                                                                    
                                                                          
   
bool
HoldingBufferPinThatDelaysRecovery(void)
{
  int bufid = GetStartupBufferPinWaitBufId();

     
                                                                            
                                                                            
                                                                       
                                                                
     
  if (bufid < 0)
  {
    return false;
  }

  if (GetPrivateRefCount(bufid + 1) > 0)
  {
    return true;
  }

  return false;
}

   
                                                                              
   
                                                                         
                                        
   
bool
ConditionalLockBufferForCleanup(Buffer buffer)
{
  BufferDesc *bufHdr;
  uint32 buf_state, refcount;

  Assert(BufferIsValid(buffer));

  if (BufferIsLocal(buffer))
  {
    refcount = LocalRefCount[-buffer - 1];
                                         
    Assert(refcount > 0);
    if (refcount != 1)
    {
      return false;
    }
                                 
    return true;
  }

                                             
  refcount = GetPrivateRefCount(buffer);
  Assert(refcount);
  if (refcount != 1)
  {
    return false;
  }

                           
  if (!ConditionalLockBuffer(buffer))
  {
    return false;
  }

  bufHdr = GetBufferDescriptor(buffer - 1);
  buf_state = LockBufHdr(bufHdr);
  refcount = BUF_STATE_GET_REFCOUNT(buf_state);

  Assert(refcount > 0);
  if (refcount == 1)
  {
                                                              
    UnlockBufHdr(bufHdr, buf_state);
    return true;
  }

                                   
  UnlockBufHdr(bufHdr, buf_state);
  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
  return false;
}

   
                                                              
   
                                                                      
                                                                      
                                                                       
                                                                      
   
bool
IsBufferCleanupOK(Buffer buffer)
{
  BufferDesc *bufHdr;
  uint32 buf_state;

  Assert(BufferIsValid(buffer));

  if (BufferIsLocal(buffer))
  {
                                         
    if (LocalRefCount[-buffer - 1] != 1)
    {
      return false;
    }
                                 
    return true;
  }

                                             
  if (GetPrivateRefCount(buffer) != 1)
  {
    return false;
  }

  bufHdr = GetBufferDescriptor(buffer - 1);

                                                 
  Assert(LWLockHeldByMeInMode(BufferDescriptorGetContentLock(bufHdr), LW_EXCLUSIVE));

  buf_state = LockBufHdr(bufHdr);

  Assert(BUF_STATE_GET_REFCOUNT(buf_state) > 0);
  if (BUF_STATE_GET_REFCOUNT(buf_state) == 1)
  {
                         
    UnlockBufHdr(bufHdr, buf_state);
    return true;
  }

  UnlockBufHdr(bufHdr, buf_state);
  return false;
}

   
                                     
   
                                                        
                                                         
   
                                                                          
   

   
                                                                      
   
static void
WaitIO(BufferDesc *buf)
{
     
                                                            
     
                                                                          
                                                                
                    
     
  for (;;)
  {
    uint32 buf_state;

       
                                                                         
                                                                           
                     
       
    buf_state = LockBufHdr(buf);
    UnlockBufHdr(buf, buf_state);

    if (!(buf_state & BM_IO_IN_PROGRESS))
    {
      break;
    }
    LWLockAcquire(BufferDescriptorGetIOLock(buf), LW_SHARED);
    LWLockRelease(BufferDescriptorGetIOLock(buf));
  }
}

   
                                           
                 
                                 
                        
   
                                                                          
                                                                       
                                                                    
                                        
   
                                                                         
                                                                         
                                                      
   
                                                                  
                                               
   
static bool
StartBufferIO(BufferDesc *buf, bool forInput)
{
  uint32 buf_state;

  Assert(!InProgressBuf);

  for (;;)
  {
       
                                                                         
                             
       
    LWLockAcquire(BufferDescriptorGetIOLock(buf), LW_EXCLUSIVE);

    buf_state = LockBufHdr(buf);

    if (!(buf_state & BM_IO_IN_PROGRESS))
    {
      break;
    }

       
                                                                           
                                                                          
                                                                           
                            
       
    UnlockBufHdr(buf, buf_state);
    LWLockRelease(BufferDescriptorGetIOLock(buf));
    WaitIO(buf);
  }

                                                                          

  if (forInput ? (buf_state & BM_VALID) : !(buf_state & BM_DIRTY))
  {
                                          
    UnlockBufHdr(buf, buf_state);
    LWLockRelease(BufferDescriptorGetIOLock(buf));
    return false;
  }

  buf_state |= BM_IO_IN_PROGRESS;
  UnlockBufHdr(buf, buf_state);

  InProgressBuf = buf;
  IsForInput = forInput;

  return true;
}

   
                                                            
                 
                                             
                                               
                                            
                        
   
                                                                       
                                                                   
                                                                         
                                                                        
   
                                                                     
                                                                      
                                                              
   
static void
TerminateBufferIO(BufferDesc *buf, bool clear_dirty, uint32 set_flag_bits)
{
  uint32 buf_state;

  Assert(buf == InProgressBuf);

  buf_state = LockBufHdr(buf);

  Assert(buf_state & BM_IO_IN_PROGRESS);

  buf_state &= ~(BM_IO_IN_PROGRESS | BM_IO_ERROR);
  if (clear_dirty && !(buf_state & BM_JUST_DIRTIED))
  {
    buf_state &= ~(BM_DIRTY | BM_CHECKPOINT_NEEDED);
  }

  buf_state |= set_flag_bits;
  UnlockBufHdr(buf, buf_state);

  InProgressBuf = NULL;

  LWLockRelease(BufferDescriptorGetIOLock(buf));
}

   
                                                                 
   
                                                      
                                                                           
   
                                                                       
                                                           
   
void
AbortBufferIO(void)
{
  BufferDesc *buf = InProgressBuf;

  if (buf)
  {
    uint32 buf_state;

       
                                                                         
                                                                          
                                                                          
                                                                     
       
    LWLockAcquire(BufferDescriptorGetIOLock(buf), LW_EXCLUSIVE);

    buf_state = LockBufHdr(buf);
    Assert(buf_state & BM_IO_IN_PROGRESS);
    if (IsForInput)
    {
      Assert(!(buf_state & BM_DIRTY));

                                                     
      Assert(!(buf_state & BM_VALID));
      UnlockBufHdr(buf, buf_state);
    }
    else
    {
      Assert(buf_state & BM_DIRTY);
      UnlockBufHdr(buf, buf_state);
                                                            
      if (buf_state & BM_IO_ERROR)
      {
                                                                   
        char *path;

        path = relpathperm(buf->tag.rnode, buf->tag.forkNum);
        ereport(WARNING, (errcode(ERRCODE_IO_ERROR), errmsg("could not write block %u of %s", buf->tag.blockNum, path), errdetail("Multiple failures --- write error might be permanent.")));
        pfree(path);
      }
    }
    TerminateBufferIO(buf, false, BM_IO_ERROR);
  }
}

   
                                                                            
   
static void
shared_buffer_write_error_callback(void *arg)
{
  BufferDesc *bufHdr = (BufferDesc *)arg;

                                                                             
  if (bufHdr != NULL)
  {
    char *path = relpathperm(bufHdr->tag.rnode, bufHdr->tag.forkNum);

    errcontext("writing block %u of relation %s", bufHdr->tag.blockNum, path);
    pfree(path);
  }
}

   
                                                                           
   
static void
local_buffer_write_error_callback(void *arg)
{
  BufferDesc *bufHdr = (BufferDesc *)arg;

  if (bufHdr != NULL)
  {
    char *path = relpathbackend(bufHdr->tag.rnode, MyBackendId, bufHdr->tag.forkNum);

    errcontext("writing block %u of relation %s", bufHdr->tag.blockNum, path);
    pfree(path);
  }
}

   
                                                                
   
static int
rnode_comparator(const void *p1, const void *p2)
{
  RelFileNode n1 = *(const RelFileNode *)p1;
  RelFileNode n2 = *(const RelFileNode *)p2;

  if (n1.relNode < n2.relNode)
  {
    return -1;
  }
  else if (n1.relNode > n2.relNode)
  {
    return 1;
  }

  if (n1.dbNode < n2.dbNode)
  {
    return -1;
  }
  else if (n1.dbNode > n2.dbNode)
  {
    return 1;
  }

  if (n1.spcNode < n2.spcNode)
  {
    return -1;
  }
  else if (n1.spcNode > n2.spcNode)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

   
                                                       
   
uint32
LockBufHdr(BufferDesc *desc)
{
  SpinDelayStatus delayStatus;
  uint32 old_buf_state;

  init_local_spin_delay(&delayStatus);

  while (true)
  {
                            
    old_buf_state = pg_atomic_fetch_or_u32(&desc->state, BM_LOCKED);
                                          
    if (!(old_buf_state & BM_LOCKED))
    {
      break;
    }
    perform_spin_delay(&delayStatus);
  }
  finish_spin_delay(&delayStatus);
  return old_buf_state | BM_LOCKED;
}

   
                                                                           
                        
   
                                                                              
                                                
   
static uint32
WaitBufHdrUnlocked(BufferDesc *buf)
{
  SpinDelayStatus delayStatus;
  uint32 buf_state;

  init_local_spin_delay(&delayStatus);

  buf_state = pg_atomic_read_u32(&buf->state);

  while (buf_state & BM_LOCKED)
  {
    perform_spin_delay(&delayStatus);
    buf_state = pg_atomic_read_u32(&buf->state);
  }

  finish_spin_delay(&delayStatus);

  return buf_state;
}

   
                         
   
static int
buffertag_comparator(const void *a, const void *b)
{
  const BufferTag *ba = (const BufferTag *)a;
  const BufferTag *bb = (const BufferTag *)b;
  int ret;

  ret = rnode_comparator(&ba->rnode, &bb->rnode);

  if (ret != 0)
  {
    return ret;
  }

  if (ba->forkNum < bb->forkNum)
  {
    return -1;
  }
  if (ba->forkNum > bb->forkNum)
  {
    return 1;
  }

  if (ba->blockNum < bb->blockNum)
  {
    return -1;
  }
  if (ba->blockNum > bb->blockNum)
  {
    return 1;
  }

  return 0;
}

   
                                                              
   
                                                                            
                                            
   
static int
ckpt_buforder_comparator(const void *pa, const void *pb)
{
  const CkptSortItem *a = (const CkptSortItem *)pa;
  const CkptSortItem *b = (const CkptSortItem *)pb;

                          
  if (a->tsId < b->tsId)
  {
    return -1;
  }
  else if (a->tsId > b->tsId)
  {
    return 1;
  }
                        
  if (a->relNode < b->relNode)
  {
    return -1;
  }
  else if (a->relNode > b->relNode)
  {
    return 1;
  }
                    
  else if (a->forkNum < b->forkNum)
  {
    return -1;
  }
  else if (a->forkNum > b->forkNum)
  {
    return 1;
  }
                            
  else if (a->blockNum < b->blockNum)
  {
    return -1;
  }
  else if (a->blockNum > b->blockNum)
  {
    return 1;
  }
                                                       
  return 0;
}

   
                                                                           
             
   
static int
ts_ckpt_progress_comparator(Datum a, Datum b, void *arg)
{
  CkptTsStatus *sa = (CkptTsStatus *)a;
  CkptTsStatus *sb = (CkptTsStatus *)b;

                                                     
  if (sa->progress < sb->progress)
  {
    return 1;
  }
  else if (sa->progress == sb->progress)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}

   
                                                                        
   
                                                                            
                                                                            
                                                                           
                                        
   
void
WritebackContextInit(WritebackContext *context, int *max_pending)
{
  Assert(*max_pending <= WRITEBACK_MAX_PENDING_FLUSHES);

  context->max_pending = max_pending;
  context->nr_pending = 0;
}

   
                                                     
   
void
ScheduleBufferTagForWriteback(WritebackContext *context, BufferTag *tag)
{
  PendingWriteback *pending;

     
                                                                            
               
     
  if (*context->max_pending > 0)
  {
    Assert(*context->max_pending <= WRITEBACK_MAX_PENDING_FLUSHES);

    pending = &context->pending_writebacks[context->nr_pending++];

    pending->tag = *tag;
  }

     
                                                                      
                                                                            
                      
     
  if (context->nr_pending >= *context->max_pending)
  {
    IssuePendingWritebacks(context);
  }
}

   
                                                                   
                                             
   
                                                                              
                                 
   
void
IssuePendingWritebacks(WritebackContext *context)
{
  int i;

  if (context->nr_pending == 0)
  {
    return;
  }

     
                                                                             
                                                                            
     
  qsort(&context->pending_writebacks, context->nr_pending, sizeof(PendingWriteback), buffertag_comparator);

     
                                                                         
                                                                            
                                                  
     
  for (i = 0; i < context->nr_pending; i++)
  {
    PendingWriteback *cur;
    PendingWriteback *next;
    SMgrRelation reln;
    int ahead;
    BufferTag tag;
    Size nblocks = 1;

    cur = &context->pending_writebacks[i];
    tag = cur->tag;

       
                                                                         
                                         
       
    for (ahead = 0; i + ahead + 1 < context->nr_pending; ahead++)
    {
      next = &context->pending_writebacks[i + ahead + 1];

                                
      if (!RelFileNodeEquals(cur->tag.rnode, next->tag.rnode) || cur->tag.forkNum != next->tag.forkNum)
      {
        break;
      }

                                        
      if (cur->tag.blockNum == next->tag.blockNum)
      {
        continue;
      }

                                         
      if (cur->tag.blockNum + 1 != next->tag.blockNum)
      {
        break;
      }

      nblocks++;
      cur = next;
    }

    i += ahead;

                                                                  
    reln = smgropen(tag.rnode, InvalidBackendId);
    smgrwriteback(reln, tag.forkNum, tag.blockNum, nblocks);
  }

  context->nr_pending = 0;
}

   
                                                          
   
                                                                              
                 
   
void
TestForOldSnapshot_impl(Snapshot snapshot, Relation relation)
{
  if (RelationAllowsEarlyPruning(relation) && (snapshot)->whenTaken < GetOldSnapshotThresholdTimestamp())
  {
    ereport(ERROR, (errcode(ERRCODE_SNAPSHOT_TOO_OLD), errmsg("snapshot too old")));
  }
}
