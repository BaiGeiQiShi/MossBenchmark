                                                                            
   
                   
                                                   
   
                                                                         
                                                                        
   
   
                  
                                             
   
                      
                                                                         
                                                          
                                                                            
                                                               
                                                  
                                                                      
                                                         
   
         
   
                                                                             
                                                                              
                                                                             
                                                                           
                                                                              
                                                                             
                                                                             
   
                                                                              
                                                                               
                      
   
                                                                           
                                                                            
                                             
   
                                                                              
                                                                             
                                                                           
                                                                              
                                                                               
                                                                                
                                                                               
                                                                            
                                                                            
                                                                            
                         
   
                                                                            
                                                                                
   
           
   
                                                                          
                                                                      
                                                                            
                                                                        
                                                                           
                                                                               
                                                                         
                                                                          
                                                                             
                                                                      
                                                                        
                                                                               
                                                                         
                                                                           
                                                                  
   
                                                                         
                                                                        
                                                                            
                                       
   
                                                                            
                                                                              
                                                                              
                                                                           
                                                                  
   
                                                                            
   
#include "postgres.h"

#include "access/heapam_xlog.h"
#include "access/visibilitymap.h"
#include "access/xlog.h"
#include "miscadmin.h"
#include "port/pg_bitutils.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"
#include "utils/inval.h"

                                

   
                                                                        
                                                                      
                        
   
#define MAPSIZE (BLCKSZ - MAXALIGN(SizeOfPageHeaderData))

                                                        
#define HEAPBLOCKS_PER_BYTE (BITS_PER_BYTE / BITS_PER_HEAPBLOCK)

                                                                        
#define HEAPBLOCKS_PER_PAGE (MAPSIZE * HEAPBLOCKS_PER_BYTE)

                                                                           
#define HEAPBLK_TO_MAPBLOCK(x) ((x) / HEAPBLOCKS_PER_PAGE)
#define HEAPBLK_TO_MAPBYTE(x) (((x) % HEAPBLOCKS_PER_PAGE) / HEAPBLOCKS_PER_BYTE)
#define HEAPBLK_TO_OFFSET(x) (((x) % HEAPBLOCKS_PER_BYTE) * BITS_PER_HEAPBLOCK)

                                                               
#define VISIBLE_MASK64                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  UINT64CONST(0x5555555555555555)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
                                                
#define FROZEN_MASK64                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  UINT64CONST(0xaaaaaaaaaaaaaaaa)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
                                                

                                      
static Buffer
vm_readbuf(Relation rel, BlockNumber blkno, bool extend);
static void
vm_extend(Relation rel, BlockNumber nvmblocks);

   
                                                                             
   
                                                                            
                                                                               
                                                                             
   
bool
visibilitymap_clear(Relation rel, BlockNumber heapBlk, Buffer buf, uint8 flags)
{
  BlockNumber mapBlock = HEAPBLK_TO_MAPBLOCK(heapBlk);
  int mapByte = HEAPBLK_TO_MAPBYTE(heapBlk);
  int mapOffset = HEAPBLK_TO_OFFSET(heapBlk);
  uint8 mask = flags << mapOffset;
  char *map;
  bool cleared = false;

  Assert(flags & VISIBILITYMAP_VALID_BITS);

#ifdef TRACE_VISIBILITYMAP
  elog(DEBUG1, "vm_clear %s %d", RelationGetRelationName(rel), heapBlk);
#endif

  if (!BufferIsValid(buf) || BufferGetBlockNumber(buf) != mapBlock)
  {
    elog(ERROR, "wrong buffer passed to visibilitymap_clear");
  }

  LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
  map = PageGetContents(BufferGetPage(buf));

  if (map[mapByte] & mask)
  {
    map[mapByte] &= ~mask;

    MarkBufferDirty(buf);
    cleared = true;
  }

  LockBuffer(buf, BUFFER_LOCK_UNLOCK);

  return cleared;
}

   
                                                        
   
                                                                             
                                                                            
                                                                         
                                                                       
                                              
   
                                                                        
                                                                                
                                                                            
                        
   
                                                                  
   
void
visibilitymap_pin(Relation rel, BlockNumber heapBlk, Buffer *buf)
{
  BlockNumber mapBlock = HEAPBLK_TO_MAPBLOCK(heapBlk);

                                               
  if (BufferIsValid(*buf))
  {
    if (BufferGetBlockNumber(*buf) == mapBlock)
    {
      return;
    }

    ReleaseBuffer(*buf);
  }
  *buf = vm_readbuf(rel, mapBlock, true);
}

   
                                                                      
   
                                                                       
                                                                                
                                                                       
                  
   
bool
visibilitymap_pin_ok(BlockNumber heapBlk, Buffer buf)
{
  BlockNumber mapBlock = HEAPBLK_TO_MAPBLOCK(heapBlk);

  return BufferIsValid(buf) && BufferGetBlockNumber(buf) == mapBlock;
}

   
                                                              
   
                                                                               
                                                                            
                                                                              
                                                                             
                                                                
                                                                            
                                                                            
                      
   
                                                                               
                                                                       
                                                                             
                                                                   
   
                                                                            
                                                                               
            
   
void
visibilitymap_set(Relation rel, BlockNumber heapBlk, Buffer heapBuf, XLogRecPtr recptr, Buffer vmBuf, TransactionId cutoff_xid, uint8 flags)
{
  BlockNumber mapBlock = HEAPBLK_TO_MAPBLOCK(heapBlk);
  uint32 mapByte = HEAPBLK_TO_MAPBYTE(heapBlk);
  uint8 mapOffset = HEAPBLK_TO_OFFSET(heapBlk);
  Page page;
  uint8 *map;

#ifdef TRACE_VISIBILITYMAP
  elog(DEBUG1, "vm_set %s %d", RelationGetRelationName(rel), heapBlk);
#endif

  Assert(InRecovery || XLogRecPtrIsInvalid(recptr));
  Assert(InRecovery || BufferIsValid(heapBuf));
  Assert(flags & VISIBILITYMAP_VALID_BITS);

                                                                 
  if (BufferIsValid(heapBuf) && BufferGetBlockNumber(heapBuf) != heapBlk)
  {
    elog(ERROR, "wrong heap buffer passed to visibilitymap_set");
  }

                                                   
  if (!BufferIsValid(vmBuf) || BufferGetBlockNumber(vmBuf) != mapBlock)
  {
    elog(ERROR, "wrong VM buffer passed to visibilitymap_set");
  }

  page = BufferGetPage(vmBuf);
  map = (uint8 *)PageGetContents(page);
  LockBuffer(vmBuf, BUFFER_LOCK_EXCLUSIVE);

  if (flags != (map[mapByte] >> mapOffset & VISIBILITYMAP_VALID_BITS))
  {
    START_CRIT_SECTION();

    map[mapByte] |= (flags << mapOffset);
    MarkBufferDirty(vmBuf);

    if (RelationNeedsWAL(rel))
    {
      if (XLogRecPtrIsInvalid(recptr))
      {
        Assert(!InRecovery);
        recptr = log_heap_visible(rel->rd_node, heapBuf, vmBuf, cutoff_xid, flags);

           
                                                                   
                                                          
           
        if (XLogHintBitIsNeeded())
        {
          Page heapPage = BufferGetPage(heapBuf);

                                                              
          Assert(PageIsAllVisible(heapPage));
          PageSetLSN(heapPage, recptr);
        }
      }
      PageSetLSN(page, recptr);
    }

    END_CRIT_SECTION();
  }

  LockBuffer(vmBuf, BUFFER_LOCK_UNLOCK);
}

   
                                                 
   
                                                                            
                          
   
                                                                           
                                                                             
                                                                            
                                                                        
                                                            
   
                                                                            
                                                                             
                                                                               
                                                                              
                                                                               
                           
   
uint8
visibilitymap_get_status(Relation rel, BlockNumber heapBlk, Buffer *buf)
{
  BlockNumber mapBlock = HEAPBLK_TO_MAPBLOCK(heapBlk);
  uint32 mapByte = HEAPBLK_TO_MAPBYTE(heapBlk);
  uint8 mapOffset = HEAPBLK_TO_OFFSET(heapBlk);
  char *map;
  uint8 result;

#ifdef TRACE_VISIBILITYMAP
  elog(DEBUG1, "vm_get_status %s %d", RelationGetRelationName(rel), heapBlk);
#endif

                                               
  if (BufferIsValid(*buf))
  {
    if (BufferGetBlockNumber(*buf) != mapBlock)
    {
      ReleaseBuffer(*buf);
      *buf = InvalidBuffer;
    }
  }

  if (!BufferIsValid(*buf))
  {
    *buf = vm_readbuf(rel, mapBlock, false);
    if (!BufferIsValid(*buf))
    {
      return false;
    }
  }

  map = PageGetContents(BufferGetPage(*buf));

     
                                                                           
                                                                            
                 
     
  result = ((map[mapByte] >> mapOffset) & VISIBILITYMAP_VALID_BITS);
  return result;
}

   
                                                                     
   
                                                                              
                                                                             
                                                                                  
   
void
visibilitymap_count(Relation rel, BlockNumber *all_visible, BlockNumber *all_frozen)
{
  BlockNumber mapBlock;
  BlockNumber nvisible = 0;
  BlockNumber nfrozen = 0;

                                     
  Assert(all_visible);

  for (mapBlock = 0;; mapBlock++)
  {
    Buffer mapBuffer;
    uint64 *map;
    int i;

       
                                                                           
                                                                       
                            
       
    mapBuffer = vm_readbuf(rel, mapBlock, false);
    if (!BufferIsValid(mapBuffer))
    {
      break;
    }

       
                                                                       
                                                                     
                                                                    
       
    map = (uint64 *)PageGetContents(BufferGetPage(mapBuffer));

    StaticAssertStmt(MAPSIZE % sizeof(uint64) == 0, "unsupported MAPSIZE");
    if (all_frozen == NULL)
    {
      for (i = 0; i < MAPSIZE / sizeof(uint64); i++)
      {
        nvisible += pg_popcount64(map[i] & VISIBLE_MASK64);
      }
    }
    else
    {
      for (i = 0; i < MAPSIZE / sizeof(uint64); i++)
      {
        nvisible += pg_popcount64(map[i] & VISIBLE_MASK64);
        nfrozen += pg_popcount64(map[i] & FROZEN_MASK64);
      }
    }

    ReleaseBuffer(mapBuffer);
  }

  *all_visible = nvisible;
  if (all_frozen)
  {
    *all_frozen = nfrozen;
  }
}

   
                                                        
   
                                                                            
                                                                               
                                    
   
                                            
   
void
visibilitymap_truncate(Relation rel, BlockNumber nheapblocks)
{
  BlockNumber newnblocks;

                                           
  BlockNumber truncBlock = HEAPBLK_TO_MAPBLOCK(nheapblocks);
  uint32 truncByte = HEAPBLK_TO_MAPBYTE(nheapblocks);
  uint8 truncOffset = HEAPBLK_TO_OFFSET(nheapblocks);

#ifdef TRACE_VISIBILITYMAP
  elog(DEBUG1, "vm_truncate %s %d", RelationGetRelationName(rel), nheapblocks);
#endif

     
                                                                          
                          
     
  if (!smgrexists(RelationGetSmgr(rel), VISIBILITYMAP_FORKNUM))
  {
    return;
  }

     
                                                                           
                                                                           
                                                                           
                                                                             
            
     
  if (truncByte != 0 || truncOffset != 0)
  {
    Buffer mapBuffer;
    Page page;
    char *map;

    newnblocks = truncBlock + 1;

    mapBuffer = vm_readbuf(rel, truncBlock, false);
    if (!BufferIsValid(mapBuffer))
    {
                                                       
      return;
    }

    page = BufferGetPage(mapBuffer);
    map = PageGetContents(page);

    LockBuffer(mapBuffer, BUFFER_LOCK_EXCLUSIVE);

                                                             
    START_CRIT_SECTION();

                                       
    MemSet(&map[truncByte + 1], 0, MAPSIZE - (truncByte + 1));

           
                                                              
       
                                 
                                 
           
                                 
                                 
           
       
    map[truncByte] &= (1 << truncOffset) - 1;

       
                                                                        
                                                                           
                                                                          
                                                                           
                                                                          
                        
       
    MarkBufferDirty(mapBuffer);
    if (!InRecovery && RelationNeedsWAL(rel) && XLogHintBitIsNeeded())
    {
      log_newpage_buffer(mapBuffer, false);
    }

    END_CRIT_SECTION();

    UnlockReleaseBuffer(mapBuffer);
  }
  else
  {
    newnblocks = truncBlock;
  }

  if (smgrnblocks(RelationGetSmgr(rel), VISIBILITYMAP_FORKNUM) <= newnblocks)
  {
                                                                         
    return;
  }

                                                                 
  smgrtruncate(RelationGetSmgr(rel), VISIBILITYMAP_FORKNUM, newnblocks);

     
                                                                             
                                                                          
                                                                            
                                                                             
     
  if (rel->rd_smgr)
  {
    rel->rd_smgr->smgr_vm_nblocks = newnblocks;
  }
}

   
                               
   
                                                                           
                                              
   
static Buffer
vm_readbuf(Relation rel, BlockNumber blkno, bool extend)
{
  Buffer buf;
  SMgrRelation reln;

     
                                                                          
                                                                         
                                                   
     
  reln = RelationGetSmgr(rel);

     
                                                                            
            
     
  if (reln->smgr_vm_nblocks == InvalidBlockNumber)
  {
    if (smgrexists(reln, VISIBILITYMAP_FORKNUM))
    {
      reln->smgr_vm_nblocks = smgrnblocks(reln, VISIBILITYMAP_FORKNUM);
    }
    else
    {
      reln->smgr_vm_nblocks = 0;
    }
  }

                                  
  if (blkno >= reln->smgr_vm_nblocks)
  {
    if (extend)
    {
      vm_extend(rel, blkno + 1);
    }
    else
    {
      return InvalidBuffer;
    }
  }

     
                                                                        
                                                                           
                
     
                                                                            
                                                                       
                                                                           
                                                                       
                                                                       
                                                                         
                                                                             
                                                                             
                                                                           
                                                                       
                                                                         
                                                                           
                                                                            
     
  buf = ReadBufferExtended(rel, VISIBILITYMAP_FORKNUM, blkno, RBM_ZERO_ON_ERROR, NULL);
  if (PageIsNew(BufferGetPage(buf)))
  {
    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
    if (PageIsNew(BufferGetPage(buf)))
    {
      PageInit(BufferGetPage(buf), BLCKSZ, 0);
    }
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);
  }
  return buf;
}

   
                                                                              
                                      
   
static void
vm_extend(Relation rel, BlockNumber vm_nblocks)
{
  BlockNumber vm_nblocks_now;
  PGAlignedBlock pg;
  SMgrRelation reln;

  PageInit((Page)pg.data, BLCKSZ, 0);

     
                                                                             
                                                                             
                                                                       
                                                                     
                                    
     
                                                                           
                                  
     
  LockRelationForExtension(rel, ExclusiveLock);

     
                                                                          
                                                                         
                                                   
     
  reln = RelationGetSmgr(rel);

     
                                                                       
                                                                  
     
  if ((reln->smgr_vm_nblocks == 0 || reln->smgr_vm_nblocks == InvalidBlockNumber) && !smgrexists(reln, VISIBILITYMAP_FORKNUM))
  {
    smgrcreate(reln, VISIBILITYMAP_FORKNUM, false);
  }

  vm_nblocks_now = smgrnblocks(reln, VISIBILITYMAP_FORKNUM);

                           
  while (vm_nblocks_now < vm_nblocks)
  {
    PageSetChecksumInplace((Page)pg.data, vm_nblocks_now);

    smgrextend(reln, VISIBILITYMAP_FORKNUM, vm_nblocks_now, pg.data, false);
    vm_nblocks_now++;
  }

     
                                                                           
                                                                          
                                                                             
                                                                           
                   
     
  CacheInvalidateSmgr(reln->smgr_rnode);

                                                   
  reln->smgr_vm_nblocks = vm_nblocks_now;

  UnlockRelationForExtension(rel, ExclusiveLock);
}
