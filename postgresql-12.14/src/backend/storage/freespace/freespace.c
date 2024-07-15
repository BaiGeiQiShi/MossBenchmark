                                                                            
   
               
                                                                         
   
   
                                                                         
                                                                        
   
                  
                                               
   
   
          
   
                                                                        
                                                                          
                                                                        
                                                                           
                     
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xlogutils.h"
#include "miscadmin.h"
#include "storage/freespace.h"
#include "storage/fsm_internals.h"
#include "storage/lmgr.h"
#include "storage/smgr.h"

   
                                                                           
                                                                      
                                                                          
                                                                          
                                                               
                                 
   
                                                                          
                                                                    
                              
   
   
                   
              
               
                   
                   
                   
                   
   
                                                                             
                                                                          
                                                                        
                                                                              
                                                                        
                                               
   
#define FSM_CATEGORIES 256
#define FSM_CAT_STEP (BLCKSZ / FSM_CATEGORIES)
#define MaxFSMRequestSize MaxHeapTupleSize

   
                                                                           
                                                                           
                                                                         
                                                                          
                                                            
   
#define FSM_TREE_DEPTH ((SlotsPerFSMPage >= 1626) ? 3 : 4)

#define FSM_ROOT_LEVEL (FSM_TREE_DEPTH - 1)
#define FSM_BOTTOM_LEVEL 0

   
                                                                       
                                                                         
   
typedef struct
{
  int level;                
  int logpageno;                                   
} FSMAddress;

                               
static const FSMAddress FSM_ROOT_ADDRESS = {FSM_ROOT_LEVEL, 0};

                                    
static FSMAddress
fsm_get_child(FSMAddress parent, uint16 slot);
static FSMAddress
fsm_get_parent(FSMAddress child, uint16 *slot);
static FSMAddress
fsm_get_location(BlockNumber heapblk, uint16 *slot);
static BlockNumber
fsm_get_heap_blk(FSMAddress addr, uint16 slot);
static BlockNumber
fsm_logical_to_physical(FSMAddress addr);

static Buffer
fsm_readbuf(Relation rel, FSMAddress addr, bool extend);
static void
fsm_extend(Relation rel, BlockNumber fsm_nblocks);

                                                                 
static uint8
fsm_space_avail_to_cat(Size avail);
static uint8
fsm_space_needed_to_cat(Size needed);
static Size
fsm_space_cat_to_avail(uint8 cat);

                                                
static int
fsm_set_and_search(Relation rel, FSMAddress addr, uint16 slot, uint8 newValue, uint8 minValue);
static BlockNumber
fsm_search(Relation rel, uint8 min_cat);
static uint8
fsm_vacuum_page(Relation rel, FSMAddress addr, BlockNumber start, BlockNumber end, bool *eof);

                              

   
                                                                        
                                                 
   
                                                                              
   
                                                                          
                                                                           
                                                                         
                                                                       
                                                                       
                        
   
BlockNumber
GetPageWithFreeSpace(Relation rel, Size spaceNeeded)
{
  uint8 min_cat = fsm_space_needed_to_cat(spaceNeeded);

  return fsm_search(rel, min_cat);
}

   
                                                                           
   
                                                                         
                                                                          
                                                                         
                                                                           
                                
   
BlockNumber
RecordAndGetPageWithFreeSpace(Relation rel, BlockNumber oldPage, Size oldSpaceAvail, Size spaceNeeded)
{
  int old_cat = fsm_space_avail_to_cat(oldSpaceAvail);
  int search_cat = fsm_space_needed_to_cat(spaceNeeded);
  FSMAddress addr;
  uint16 slot;
  int search_slot;

                                                                    
  addr = fsm_get_location(oldPage, &slot);

  search_slot = fsm_set_and_search(rel, addr, slot, old_cat, search_cat);

     
                                                                    
                                 
     
  if (search_slot != -1)
  {
    return fsm_get_heap_blk(addr, search_slot);
  }
  else
  {
    return fsm_search(rel, search_cat);
  }
}

   
                                                       
   
                                                                             
                                                                              
                                                                 
   
void
RecordPageWithFreeSpace(Relation rel, BlockNumber heapBlk, Size spaceAvail)
{
  int new_cat = fsm_space_avail_to_cat(spaceAvail);
  FSMAddress addr;
  uint16 slot;

                                                                    
  addr = fsm_get_location(heapBlk, &slot);

  fsm_set_and_search(rel, addr, slot, new_cat, 0);
}

   
                                                                          
               
   
void
XLogRecordPageWithFreeSpace(RelFileNode rnode, BlockNumber heapBlk, Size spaceAvail)
{
  int new_cat = fsm_space_avail_to_cat(spaceAvail);
  FSMAddress addr;
  uint16 slot;
  BlockNumber blkno;
  Buffer buf;
  Page page;

                                                                    
  addr = fsm_get_location(heapBlk, &slot);
  blkno = fsm_logical_to_physical(addr);

                                                 
  buf = XLogReadBufferExtended(rnode, FSM_FORKNUM, blkno, RBM_ZERO_ON_ERROR);
  LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

  page = BufferGetPage(buf);
  if (PageIsNew(page))
  {
    PageInit(page, BLCKSZ, 0);
  }

  if (fsm_set_avail(page, slot, new_cat))
  {
    MarkBufferDirtyHint(buf, false);
  }
  UnlockReleaseBuffer(buf);
}

   
                                                                               
                          
   
Size
GetRecordedFreeSpace(Relation rel, BlockNumber heapBlk)
{
  FSMAddress addr;
  uint16 slot;
  Buffer buf;
  uint8 cat;

                                                                    
  addr = fsm_get_location(heapBlk, &slot);

  buf = fsm_readbuf(rel, addr, false);
  if (!BufferIsValid(buf))
  {
    return 0;
  }
  cat = fsm_get_avail(BufferGetPage(buf), slot);
  ReleaseBuffer(buf);

  return fsm_space_cat_to_avail(cat);
}

   
                                                                  
   
                                                                            
                                                                               
                                     
   
                                        
   
void
FreeSpaceMapTruncateRel(Relation rel, BlockNumber nblocks)
{
  BlockNumber new_nfsmblocks;
  FSMAddress first_removed_address;
  uint16 first_removed_slot;
  Buffer buf;

     
                                                                          
               
     
  if (!smgrexists(RelationGetSmgr(rel), FSM_FORKNUM))
  {
    return;
  }

                                                                   
  first_removed_address = fsm_get_location(nblocks, &first_removed_slot);

     
                                                                   
                                                                             
                                                                             
                                         
     
  if (first_removed_slot > 0)
  {
    buf = fsm_readbuf(rel, first_removed_address, false);
    if (!BufferIsValid(buf))
    {
      return;                                                 
    }
    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

                                                             
    START_CRIT_SECTION();

    fsm_truncate_avail(BufferGetPage(buf), first_removed_slot);

       
                                                                        
                                                                           
                                                                          
                                                                           
                                                                          
                        
       
    MarkBufferDirty(buf);
    if (!InRecovery && RelationNeedsWAL(rel) && XLogHintBitIsNeeded())
    {
      log_newpage_buffer(buf, false);
    }

    END_CRIT_SECTION();

    UnlockReleaseBuffer(buf);

    new_nfsmblocks = fsm_logical_to_physical(first_removed_address) + 1;
  }
  else
  {
    new_nfsmblocks = fsm_logical_to_physical(first_removed_address);
    if (smgrnblocks(RelationGetSmgr(rel), FSM_FORKNUM) <= new_nfsmblocks)
    {
      return;                                                 
    }
  }

                                                                  
  smgrtruncate(RelationGetSmgr(rel), FSM_FORKNUM, new_nfsmblocks);

     
                                                                 
                                                                           
                                                                             
                                                                             
                 
     
  if (rel->rd_smgr)
  {
    rel->rd_smgr->smgr_fsm_nblocks = new_nfsmblocks;
  }

     
                                                                          
                                                                      
                                                     
     
  FreeSpaceMapVacuumRange(rel, nblocks, InvalidBlockNumber);
}

   
                                                                  
   
                                                                        
                               
   
void
FreeSpaceMapVacuum(Relation rel)
{
  bool dummy;

                                                       
  (void)fsm_vacuum_page(rel, FSM_ROOT_ADDRESS, (BlockNumber)0, InvalidBlockNumber, &dummy);
}

   
                                                                       
   
                                                                               
                                                                         
                                                                          
                                   
   
void
FreeSpaceMapVacuumRange(Relation rel, BlockNumber start, BlockNumber end)
{
  bool dummy;

                                                       
  if (end > start)
  {
    (void)fsm_vacuum_page(rel, FSM_ROOT_ADDRESS, start, end, &dummy);
  }
}

                                     

   
                                                       
   
static uint8
fsm_space_avail_to_cat(Size avail)
{
  int cat;

  Assert(avail < BLCKSZ);

  if (avail >= MaxFSMRequestSize)
  {
    return 255;
  }

  cat = avail / FSM_CAT_STEP;

     
                                                                           
           
     
  if (cat > 254)
  {
    cat = 254;
  }

  return (uint8)cat;
}

   
                                                                          
             
   
static Size
fsm_space_cat_to_avail(uint8 cat)
{
                                                                        
  if (cat == 255)
  {
    return MaxFSMRequestSize;
  }
  else
  {
    return cat * FSM_CAT_STEP;
  }
}

   
                                                                            
                                                                      
   
static uint8
fsm_space_needed_to_cat(Size needed)
{
  int cat;

                                                                     
  if (needed > MaxFSMRequestSize)
  {
    elog(ERROR, "invalid FSM request size %zu", needed);
  }

  if (needed == 0)
  {
    return 1;
  }

  cat = (needed + FSM_CAT_STEP - 1) / FSM_CAT_STEP;

  if (cat > 255)
  {
    cat = 255;
  }

  return (uint8)cat;
}

   
                                                   
   
static BlockNumber
fsm_logical_to_physical(FSMAddress addr)
{
  BlockNumber pages;
  int leafno;
  int l;

     
                                                                        
                 
     
  leafno = addr.logpageno;
  for (l = 0; l < addr.level; l++)
  {
    leafno *= SlotsPerFSMPage;
  }

                                                                 
  pages = 0;
  for (l = 0; l < FSM_TREE_DEPTH; l++)
  {
    pages += leafno + 1;
    leafno /= SlotsPerFSMPage;
  }

     
                                                                            
                                                    
     
  pages -= addr.level;

                                                     
  return pages - 1;
}

   
                                                              
   
static FSMAddress
fsm_get_location(BlockNumber heapblk, uint16 *slot)
{
  FSMAddress addr;

  addr.level = FSM_BOTTOM_LEVEL;
  addr.logpageno = heapblk / SlotsPerFSMPage;
  *slot = heapblk % SlotsPerFSMPage;

  return addr;
}

   
                                                                            
   
static BlockNumber
fsm_get_heap_blk(FSMAddress addr, uint16 slot)
{
  Assert(addr.level == FSM_BOTTOM_LEVEL);
  return ((unsigned int)addr.logpageno) * SlotsPerFSMPage + slot;
}

   
                                                                           
                                                                          
   
static FSMAddress
fsm_get_parent(FSMAddress child, uint16 *slot)
{
  FSMAddress parent;

  Assert(child.level < FSM_ROOT_LEVEL);

  parent.level = child.level + 1;
  parent.logpageno = child.logpageno / SlotsPerFSMPage;
  *slot = child.logpageno % SlotsPerFSMPage;

  return parent;
}

   
                                                                       
                                                    
   
static FSMAddress
fsm_get_child(FSMAddress parent, uint16 slot)
{
  FSMAddress child;

  Assert(parent.level > FSM_BOTTOM_LEVEL);

  child.level = parent.level - 1;
  child.logpageno = parent.logpageno * SlotsPerFSMPage + slot;

  return child;
}

   
                    
   
                                                                           
                                   
   
static Buffer
fsm_readbuf(Relation rel, FSMAddress addr, bool extend)
{
  BlockNumber blkno = fsm_logical_to_physical(addr);
  Buffer buf;
  SMgrRelation reln;

     
                                                                          
                                                                         
                                                   
     
  reln = RelationGetSmgr(rel);

     
                                                                         
                                                                           
                                                                            
                        
     
  if (reln->smgr_fsm_nblocks == InvalidBlockNumber || blkno >= reln->smgr_fsm_nblocks)
  {
    if (smgrexists(reln, FSM_FORKNUM))
    {
      reln->smgr_fsm_nblocks = smgrnblocks(reln, FSM_FORKNUM);
    }
    else
    {
      reln->smgr_fsm_nblocks = 0;
    }
  }

                                  
  if (blkno >= reln->smgr_fsm_nblocks)
  {
    if (extend)
    {
      fsm_extend(rel, blkno + 1);
    }
    else
    {
      return InvalidBuffer;
    }
  }

     
                                                                           
                                                                         
                                                                         
                                                                         
                           
     
                                                                            
                                                                       
                                                                           
                                                                       
                                                                       
                                                                         
                                                                             
                                                                             
                                                                           
                                                                       
                                                                         
                                                                           
                                                                            
     
  buf = ReadBufferExtended(rel, FSM_FORKNUM, blkno, RBM_ZERO_ON_ERROR, NULL);
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
fsm_extend(Relation rel, BlockNumber fsm_nblocks)
{
  BlockNumber fsm_nblocks_now;
  PGAlignedBlock pg;
  SMgrRelation reln;

  PageInit((Page)pg.data, BLCKSZ, 0);

     
                                                                             
                                                                         
                                                                           
                                                                          
         
     
                                                                           
                                  
     
  LockRelationForExtension(rel, ExclusiveLock);

     
                                                                          
                                                                         
                                                   
     
  reln = RelationGetSmgr(rel);

     
                                                                            
                                                                  
     
  if ((reln->smgr_fsm_nblocks == 0 || reln->smgr_fsm_nblocks == InvalidBlockNumber) && !smgrexists(reln, FSM_FORKNUM))
  {
    smgrcreate(reln, FSM_FORKNUM, false);
  }

  fsm_nblocks_now = smgrnblocks(reln, FSM_FORKNUM);

  while (fsm_nblocks_now < fsm_nblocks)
  {
    PageSetChecksumInplace((Page)pg.data, fsm_nblocks_now);

    smgrextend(reln, FSM_FORKNUM, fsm_nblocks_now, pg.data, false);
    fsm_nblocks_now++;
  }

                                                   
  reln->smgr_fsm_nblocks = fsm_nblocks_now;

  UnlockRelationForExtension(rel, ExclusiveLock);
}

   
                                         
   
                                                                         
                                                                     
                           
   
static int
fsm_set_and_search(Relation rel, FSMAddress addr, uint16 slot, uint8 newValue, uint8 minValue)
{
  Buffer buf;
  Page page;
  int newslot = -1;

  buf = fsm_readbuf(rel, addr, true);
  LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

  page = BufferGetPage(buf);

  if (fsm_set_avail(page, slot, newValue))
  {
    MarkBufferDirtyHint(buf, false);
  }

  if (minValue != 0)
  {
                                             
    newslot = fsm_search_avail(buf, minValue, addr.level == FSM_BOTTOM_LEVEL, true);
  }

  UnlockReleaseBuffer(buf);

  return newslot;
}

   
                                                                       
   
static BlockNumber
fsm_search(Relation rel, uint8 min_cat)
{
  int restarts = 0;
  FSMAddress addr = FSM_ROOT_ADDRESS;

  for (;;)
  {
    int slot;
    Buffer buf;
    uint8 max_avail = 0;

                            
    buf = fsm_readbuf(rel, addr, false);

                                
    if (BufferIsValid(buf))
    {
      LockBuffer(buf, BUFFER_LOCK_SHARE);
      slot = fsm_search_avail(buf, min_cat, (addr.level == FSM_BOTTOM_LEVEL), false);
      if (slot == -1)
      {
        max_avail = fsm_get_max_avail(BufferGetPage(buf));
      }
      UnlockReleaseBuffer(buf);
    }
    else
    {
      slot = -1;
    }

    if (slot != -1)
    {
         
                                                                     
                 
         
      if (addr.level == FSM_BOTTOM_LEVEL)
      {
        return fsm_get_heap_blk(addr, slot);
      }

      addr = fsm_get_child(addr, slot);
    }
    else if (addr.level == FSM_ROOT_LEVEL)
    {
         
                                                                     
                                    
         
      return InvalidBlockNumber;
    }
    else
    {
      uint16 parentslot;
      FSMAddress parent;

         
                                                                       
                                                                       
                                                                        
                     
         
                                                                        
                                                                         
                                                                        
                                                                     
                                                       
         
      parent = fsm_get_parent(addr, &parentslot);
      fsm_set_and_search(rel, parent, parentslot, max_avail, 0);

         
                                                                         
                                                                        
                                                                         
                                                                     
                
         
      if (restarts++ > 10000)
      {
        return InvalidBlockNumber;
      }

                                               
      addr = FSM_ROOT_ADDRESS;
    }
  }
}

   
                                        
   
                                                                             
                                                                          
                                                           
                                                    
   
                                                                        
   
                                                                     
                                                                            
   
static uint8
fsm_vacuum_page(Relation rel, FSMAddress addr, BlockNumber start, BlockNumber end, bool *eof_p)
{
  Buffer buf;
  Page page;
  uint8 max_avail;

                                                 
  buf = fsm_readbuf(rel, addr, false);
  if (!BufferIsValid(buf))
  {
    *eof_p = true;
    return 0;
  }
  else
  {
    *eof_p = false;
  }

  page = BufferGetPage(buf);

     
                                                                         
                                                  
     
  if (addr.level > FSM_BOTTOM_LEVEL)
  {
    FSMAddress fsm_start, fsm_end;
    uint16 fsm_start_slot, fsm_end_slot;
    int slot, start_slot, end_slot;
    bool eof = false;

       
                                                                        
                                                                          
                                                                          
                                                                          
                                                                          
       
    fsm_start = fsm_get_location(start, &fsm_start_slot);
    fsm_end = fsm_get_location(end - 1, &fsm_end_slot);

    while (fsm_start.level < addr.level)
    {
      fsm_start = fsm_get_parent(fsm_start, &fsm_start_slot);
      fsm_end = fsm_get_parent(fsm_end, &fsm_end_slot);
    }
    Assert(fsm_start.level == addr.level);

    if (fsm_start.logpageno == addr.logpageno)
    {
      start_slot = fsm_start_slot;
    }
    else if (fsm_start.logpageno > addr.logpageno)
    {
      start_slot = SlotsPerFSMPage;                            
    }
    else
    {
      start_slot = 0;
    }

    if (fsm_end.logpageno == addr.logpageno)
    {
      end_slot = fsm_end_slot;
    }
    else if (fsm_end.logpageno > addr.logpageno)
    {
      end_slot = SlotsPerFSMPage - 1;
    }
    else
    {
      end_slot = -1;                            
    }

    for (slot = start_slot; slot <= end_slot; slot++)
    {
      int child_avail;

      CHECK_FOR_INTERRUPTS();

                                                                      
      if (!eof)
      {
        child_avail = fsm_vacuum_page(rel, fsm_get_child(addr, slot), start, end, &eof);
      }
      else
      {
        child_avail = 0;
      }

                                              
      if (fsm_get_avail(page, slot) != child_avail)
      {
        LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
        fsm_set_avail(page, slot, child_avail);
        MarkBufferDirtyHint(buf, false);
        LockBuffer(buf, BUFFER_LOCK_UNLOCK);
      }
    }
  }

                                                                  
  max_avail = fsm_get_max_avail(page);

     
                                                                          
                                                                        
                                                                            
                                                            
     
  ((FSMPage)PageGetContents(page))->fp_next_slot = 0;

  ReleaseBuffer(buf);

  return max_avail;
}
