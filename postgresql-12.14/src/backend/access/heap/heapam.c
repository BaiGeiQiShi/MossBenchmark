                                                                            
   
            
                             
   
                                                                         
                                                                        
   
   
                  
                                      
   
   
                      
                                         
                                           
                                     
                                               
                                                
                                                
                                                               
                                                  
                                                                    
                                                             
   
         
                                                           
                                                           
                
   
                                                                            
   
#include "postgres.h"

#include "access/bufmask.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/hio.h"
#include "access/multixact.h"
#include "access/parallel.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/valid.h"
#include "access/visibilitymap.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "port/atomics.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "storage/spin.h"
#include "storage/standby.h"
#include "utils/datum.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"
#include "utils/spccache.h"

static HeapTuple
heap_prepare_insert(Relation relation, HeapTuple tup, TransactionId xid, CommandId cid, int options);
static XLogRecPtr
log_heap_update(Relation reln, Buffer oldbuf, Buffer newbuf, HeapTuple oldtup, HeapTuple newtup, HeapTuple old_key_tup, bool all_visible_cleared, bool new_all_visible_cleared);
static Bitmapset *
HeapDetermineColumnsInfo(Relation relation, Bitmapset *interesting_cols, Bitmapset *external_cols, HeapTuple oldtup, HeapTuple newtup, bool *has_external);
static bool
heap_acquire_tuplock(Relation relation, ItemPointer tid, LockTupleMode mode, LockWaitPolicy wait_policy, bool *have_tuple_lock);
static void
compute_new_xmax_infomask(TransactionId xmax, uint16 old_infomask, uint16 old_infomask2, TransactionId add_to_xmax, LockTupleMode mode, bool is_update, TransactionId *result_xmax, uint16 *result_infomask, uint16 *result_infomask2);
static TM_Result
heap_lock_updated_tuple(Relation rel, HeapTuple tuple, ItemPointer ctid, TransactionId xid, LockTupleMode mode);
static void
GetMultiXactIdHintBits(MultiXactId multi, uint16 *new_infomask, uint16 *new_infomask2);
static TransactionId
MultiXactIdGetUpdateXid(TransactionId xmax, uint16 t_infomask);
static bool
DoesMultiXactIdConflict(MultiXactId multi, uint16 infomask, LockTupleMode lockmode, bool *current_is_member);
static void
MultiXactIdWait(MultiXactId multi, MultiXactStatus status, uint16 infomask, Relation rel, ItemPointer ctid, XLTW_Oper oper, int *remaining);
static bool
ConditionalMultiXactIdWait(MultiXactId multi, MultiXactStatus status, uint16 infomask, Relation rel, int *remaining);
static XLogRecPtr
log_heap_new_cid(Relation relation, HeapTuple tup);
static HeapTuple
ExtractReplicaIdentity(Relation rel, HeapTuple tup, bool key_required, bool *copy);

   
                                                                             
                                                                              
                                                                           
                                                                              
                        
   
                                                                               
            
   
static const struct
{
  LOCKMODE hwlock;
  int lockstatus;
  int updstatus;
}

tupleLockExtraInfo[MaxLockTupleMode + 1] = {{
                                                                       
                                                AccessShareLock, MultiXactStatusForKeyShare, -1                                              
                                            },
    {
                            
        RowShareLock, MultiXactStatusForShare, -1                                           
    },
    {                             
        ExclusiveLock, MultiXactStatusForNoKeyUpdate, MultiXactStatusNoKeyUpdate},
    {                        
        AccessExclusiveLock, MultiXactStatusForUpdate, MultiXactStatusUpdate}};

                                                  
#define LOCKMODE_from_mxstatus(status) (tupleLockExtraInfo[TUPLOCK_from_mxstatus((status))].hwlock)

   
                                                                              
                                                                           
             
   
#define LockTupleTuplock(rel, tup, mode) LockTuple((rel), (tup), tupleLockExtraInfo[mode].hwlock)
#define UnlockTupleTuplock(rel, tup, mode) UnlockTuple((rel), (tup), tupleLockExtraInfo[mode].hwlock)
#define ConditionalLockTupleTuplock(rel, tup, mode) ConditionalLockTuple((rel), (tup), tupleLockExtraInfo[mode].hwlock)

#ifdef USE_PREFETCH
   
                                                                           
                                                      
   
typedef struct
{
  BlockNumber cur_hblkno;
  int next_item;
  int nitems;
  ItemPointerData *tids;
} XidHorizonPrefetchState;
#endif

   
                                                                  
                          
   
static const int MultiXactStatusLock[MaxMultiXactStatus + 1] = {
    LockTupleKeyShare,                        
    LockTupleShare,                        
    LockTupleNoKeyExclusive,                     
    LockTupleExclusive,                     
    LockTupleNoKeyExclusive,                  
    LockTupleExclusive                   
};

                                                       
#define TUPLOCK_from_mxstatus(status) (MultiXactStatusLock[(status)])

                                                                    
                               
                                                                    
   

                    
                                                                  
                    
   
static void
initscan(HeapScanDesc scan, ScanKey key, bool keep_startblock)
{
  ParallelBlockTableScanDesc bpscan = NULL;
  bool allow_strat;
  bool allow_sync;

     
                                                     
     
                                                                            
                                                                            
                                                                             
                                                                          
                                                                      
                                                                             
                                                               
     
  if (scan->rs_base.rs_parallel != NULL)
  {
    bpscan = (ParallelBlockTableScanDesc)scan->rs_base.rs_parallel;
    scan->rs_nblocks = bpscan->phs_nblocks;
  }
  else
  {
    scan->rs_nblocks = RelationGetNumberOfBlocks(scan->rs_base.rs_rd);
  }

     
                                                                        
                                                                           
                                                                            
                                                                         
                                                                            
                                                                            
                                                       
     
                                                                            
                                                          
     
  if (!RelationUsesLocalBuffers(scan->rs_base.rs_rd) && scan->rs_nblocks > NBuffers / 4)
  {
    allow_strat = (scan->rs_base.rs_flags & SO_ALLOW_STRAT) != 0;
    allow_sync = (scan->rs_base.rs_flags & SO_ALLOW_SYNC) != 0;
  }
  else
  {
    allow_strat = allow_sync = false;
  }

  if (allow_strat)
  {
                                                             
    if (scan->rs_strategy == NULL)
    {
      scan->rs_strategy = GetAccessStrategy(BAS_BULKREAD);
    }
  }
  else
  {
    if (scan->rs_strategy != NULL)
    {
      FreeAccessStrategy(scan->rs_strategy);
    }
    scan->rs_strategy = NULL;
  }

  if (scan->rs_base.rs_parallel != NULL)
  {
                                                                         
    if (scan->rs_base.rs_parallel->phs_syncscan)
    {
      scan->rs_base.rs_flags |= SO_ALLOW_SYNC;
    }
    else
    {
      scan->rs_base.rs_flags &= ~SO_ALLOW_SYNC;
    }
  }
  else if (keep_startblock)
  {
       
                                                                         
                                                                       
                                                  
       
    if (allow_sync && synchronize_seqscans)
    {
      scan->rs_base.rs_flags |= SO_ALLOW_SYNC;
    }
    else
    {
      scan->rs_base.rs_flags &= ~SO_ALLOW_SYNC;
    }
  }
  else if (allow_sync && synchronize_seqscans)
  {
    scan->rs_base.rs_flags |= SO_ALLOW_SYNC;
    scan->rs_startblock = ss_get_location(scan->rs_base.rs_rd, scan->rs_nblocks);
  }
  else
  {
    scan->rs_base.rs_flags &= ~SO_ALLOW_SYNC;
    scan->rs_startblock = 0;
  }

  scan->rs_numblocks = InvalidBlockNumber;
  scan->rs_inited = false;
  scan->rs_ctup.t_data = NULL;
  ItemPointerSetInvalid(&scan->rs_ctup.t_self);
  scan->rs_cbuf = InvalidBuffer;
  scan->rs_cblock = InvalidBlockNumber;

                                                                   

     
                                       
     
  if (key != NULL && scan->rs_base.rs_nkeys > 0)
  {
    memcpy(scan->rs_base.rs_key, key, scan->rs_base.rs_nkeys * sizeof(ScanKeyData));
  }

     
                                                                            
                                                                             
                                                              
     
  if (scan->rs_base.rs_flags & SO_TYPE_SEQSCAN)
  {
    pgstat_count_heap_scan(scan->rs_base.rs_rd);
  }
}

   
                                                     
   
                                    
                                                                       
   
void
heap_setscanlimits(TableScanDesc sscan, BlockNumber startBlk, BlockNumber numBlks)
{
  HeapScanDesc scan = (HeapScanDesc)sscan;

  Assert(!scan->rs_inited);                              
                                         
  Assert(!(scan->rs_base.rs_flags & SO_ALLOW_SYNC));

                                                                  
  Assert(startBlk == 0 || startBlk < scan->rs_nblocks);

  scan->rs_startblock = startBlk;
  scan->rs_numblocks = numBlks;
}

   
                                             
   
                                                                   
                                                                          
                                         
   
void
heapgetpage(TableScanDesc sscan, BlockNumber page)
{
  HeapScanDesc scan = (HeapScanDesc)sscan;
  Buffer buffer;
  Snapshot snapshot;
  Page dp;
  int lines;
  int ntup;
  OffsetNumber lineoff;
  ItemId lpp;
  bool all_visible;

  Assert(page < scan->rs_nblocks);

                                            
  if (BufferIsValid(scan->rs_cbuf))
  {
    ReleaseBuffer(scan->rs_cbuf);
    scan->rs_cbuf = InvalidBuffer;
  }

     
                                                                        
                                                                             
                                              
     
  CHECK_FOR_INTERRUPTS();

                                         
  scan->rs_cbuf = ReadBufferExtended(scan->rs_base.rs_rd, MAIN_FORKNUM, page, RBM_NORMAL, scan->rs_strategy);
  scan->rs_cblock = page;

  if (!(scan->rs_base.rs_flags & SO_ALLOW_PAGEMODE))
  {
    return;
  }

  buffer = scan->rs_cbuf;
  snapshot = scan->rs_base.rs_snapshot;

     
                                                                     
     
  heap_page_prune_opt(scan->rs_base.rs_rd, buffer);

     
                                                                         
                                                                      
                                                                    
     
  LockBuffer(buffer, BUFFER_LOCK_SHARE);

  dp = BufferGetPage(buffer);
  TestForOldSnapshot(snapshot, scan->rs_base.rs_rd, dp);
  lines = PageGetMaxOffsetNumber(dp);
  ntup = 0;

     
                                                                       
                                                                      
     
                                                                 
                                                                        
                                                                           
                                                                           
                                                                           
                                                                   
                                                                             
                                                                           
                                                                    
                                                                           
                                                                          
                                                                          
                                                                            
                                                                             
                                        
     
  all_visible = PageIsAllVisible(dp) && !snapshot->takenDuringRecovery;

  for (lineoff = FirstOffsetNumber, lpp = PageGetItemId(dp, lineoff); lineoff <= lines; lineoff++, lpp++)
  {
    if (ItemIdIsNormal(lpp))
    {
      HeapTupleData loctup;
      bool valid;

      loctup.t_tableOid = RelationGetRelid(scan->rs_base.rs_rd);
      loctup.t_data = (HeapTupleHeader)PageGetItem((Page)dp, lpp);
      loctup.t_len = ItemIdGetLength(lpp);
      ItemPointerSet(&(loctup.t_self), page, lineoff);

      if (all_visible)
      {
        valid = true;
      }
      else
      {
        valid = HeapTupleSatisfiesVisibility(&loctup, snapshot, buffer);
      }

      CheckForSerializableConflictOut(valid, scan->rs_base.rs_rd, &loctup, buffer, snapshot);

      if (valid)
      {
        scan->rs_vistuples[ntup++] = lineoff;
      }
    }
  }

  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

  Assert(ntup <= MaxHeapTuplesPerPage);
  scan->rs_ntuples = ntup;
}

                    
                                       
   
                                                                      
                                                                         
                                                          
   
                                                                      
                      
   
                                                                          
                                                                            
                 
   
                                                                      
                                                                     
                                                                   
                                                                    
                                                                          
                                                                     
                              
                    
   
static void
heapgettup(HeapScanDesc scan, ScanDirection dir, int nkeys, ScanKey key)
{
  HeapTuple tuple = &(scan->rs_ctup);
  Snapshot snapshot = scan->rs_base.rs_snapshot;
  bool backward = ScanDirectionIsBackward(dir);
  BlockNumber page;
  bool finished;
  Page dp;
  int lines;
  OffsetNumber lineoff;
  int linesleft;
  ItemId lpp;

     
                                                           
     
  if (ScanDirectionIsForward(dir))
  {
    if (!scan->rs_inited)
    {
         
                                                      
         
      if (scan->rs_nblocks == 0 || scan->rs_numblocks == 0)
      {
        Assert(!BufferIsValid(scan->rs_cbuf));
        tuple->t_data = NULL;
        return;
      }
      if (scan->rs_base.rs_parallel != NULL)
      {
        ParallelBlockTableScanDesc pbscan = (ParallelBlockTableScanDesc)scan->rs_base.rs_parallel;

        table_block_parallelscan_startblock_init(scan->rs_base.rs_rd, pbscan);

        page = table_block_parallelscan_nextpage(scan->rs_base.rs_rd, pbscan);

                                                                   
        if (page == InvalidBlockNumber)
        {
          Assert(!BufferIsValid(scan->rs_cbuf));
          tuple->t_data = NULL;
          return;
        }
      }
      else
      {
        page = scan->rs_startblock;                 
      }
      heapgetpage((TableScanDesc)scan, page);
      lineoff = FirstOffsetNumber;                   
      scan->rs_inited = true;
    }
    else
    {
                                                        
      page = scan->rs_cblock;                   
      lineoff =                                
          OffsetNumberNext(ItemPointerGetOffsetNumber(&(tuple->t_self)));
    }

    LockBuffer(scan->rs_cbuf, BUFFER_LOCK_SHARE);

    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(snapshot, scan->rs_base.rs_rd, dp);
    lines = PageGetMaxOffsetNumber(dp);
                                                                

    linesleft = lines - lineoff + 1;
  }
  else if (backward)
  {
                                              
    Assert(scan->rs_base.rs_parallel == NULL);

    if (!scan->rs_inited)
    {
         
                                                      
         
      if (scan->rs_nblocks == 0 || scan->rs_numblocks == 0)
      {
        Assert(!BufferIsValid(scan->rs_cbuf));
        tuple->t_data = NULL;
        return;
      }

         
                                                                       
                                                                         
                                                                      
                           
         
      scan->rs_base.rs_flags &= ~SO_ALLOW_SYNC;

         
                                                                        
                                                                     
         
      if (scan->rs_numblocks != InvalidBlockNumber)
      {
        page = (scan->rs_startblock + scan->rs_numblocks - 1) % scan->rs_nblocks;
      }
      else if (scan->rs_startblock > 0)
      {
        page = scan->rs_startblock - 1;
      }
      else
      {
        page = scan->rs_nblocks - 1;
      }
      heapgetpage((TableScanDesc)scan, page);
    }
    else
    {
                                                        
      page = scan->rs_cblock;                   
    }

    LockBuffer(scan->rs_cbuf, BUFFER_LOCK_SHARE);

    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(snapshot, scan->rs_base.rs_rd, dp);
    lines = PageGetMaxOffsetNumber(dp);

    if (!scan->rs_inited)
    {
      lineoff = lines;                   
      scan->rs_inited = true;
    }
    else
    {
      lineoff =                      
          OffsetNumberPrev(ItemPointerGetOffsetNumber(&(tuple->t_self)));
    }
                                                                    

    linesleft = lineoff;
  }
  else
  {
       
                                                           
       
    if (!scan->rs_inited)
    {
      Assert(!BufferIsValid(scan->rs_cbuf));
      tuple->t_data = NULL;
      return;
    }

    page = ItemPointerGetBlockNumber(&(tuple->t_self));
    if (page != scan->rs_cblock)
    {
      heapgetpage((TableScanDesc)scan, page);
    }

                                                                        
    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(snapshot, scan->rs_base.rs_rd, dp);
    lineoff = ItemPointerGetOffsetNumber(&(tuple->t_self));
    lpp = PageGetItemId(dp, lineoff);
    Assert(ItemIdIsNormal(lpp));

    tuple->t_data = (HeapTupleHeader)PageGetItem((Page)dp, lpp);
    tuple->t_len = ItemIdGetLength(lpp);

    return;
  }

     
                                                                           
             
     
  lpp = PageGetItemId(dp, lineoff);
  for (;;)
  {
    while (linesleft > 0)
    {
      if (ItemIdIsNormal(lpp))
      {
        bool valid;

        tuple->t_data = (HeapTupleHeader)PageGetItem((Page)dp, lpp);
        tuple->t_len = ItemIdGetLength(lpp);
        ItemPointerSet(&(tuple->t_self), page, lineoff);

           
                                                  
           
        valid = HeapTupleSatisfiesVisibility(tuple, snapshot, scan->rs_cbuf);

        CheckForSerializableConflictOut(valid, scan->rs_base.rs_rd, tuple, scan->rs_cbuf, snapshot);

        if (valid && key != NULL)
        {
          HeapKeyTest(tuple, RelationGetDescr(scan->rs_base.rs_rd), nkeys, key, valid);
        }

        if (valid)
        {
          LockBuffer(scan->rs_cbuf, BUFFER_LOCK_UNLOCK);
          return;
        }
      }

         
                                                     
         
      --linesleft;
      if (backward)
      {
        --lpp;                                            
        --lineoff;
      }
      else
      {
        ++lpp;                                               
        ++lineoff;
      }
    }

       
                                                                           
                                      
       
    LockBuffer(scan->rs_cbuf, BUFFER_LOCK_UNLOCK);

       
                                                         
       
    if (backward)
    {
      finished = (page == scan->rs_startblock) || (scan->rs_numblocks != InvalidBlockNumber ? --scan->rs_numblocks == 0 : false);
      if (page == 0)
      {
        page = scan->rs_nblocks;
      }
      page--;
    }
    else if (scan->rs_base.rs_parallel != NULL)
    {
      ParallelBlockTableScanDesc pbscan = (ParallelBlockTableScanDesc)scan->rs_base.rs_parallel;

      page = table_block_parallelscan_nextpage(scan->rs_base.rs_rd, pbscan);
      finished = (page == InvalidBlockNumber);
    }
    else
    {
      page++;
      if (page >= scan->rs_nblocks)
      {
        page = 0;
      }
      finished = (page == scan->rs_startblock) || (scan->rs_numblocks != InvalidBlockNumber ? --scan->rs_numblocks == 0 : false);

         
                                                                       
                                                                       
                                                    
         
                                                                      
                                                                      
                                                                         
                                                                         
                                                                         
                                                                      
         
      if (scan->rs_base.rs_flags & SO_ALLOW_SYNC)
      {
        ss_report_location(scan->rs_base.rs_rd, page);
      }
    }

       
                                                    
       
    if (finished)
    {
      if (BufferIsValid(scan->rs_cbuf))
      {
        ReleaseBuffer(scan->rs_cbuf);
      }
      scan->rs_cbuf = InvalidBuffer;
      scan->rs_cblock = InvalidBlockNumber;
      tuple->t_data = NULL;
      scan->rs_inited = false;
      return;
    }

    heapgetpage((TableScanDesc)scan, page);

    LockBuffer(scan->rs_cbuf, BUFFER_LOCK_SHARE);

    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(snapshot, scan->rs_base.rs_rd, dp);
    lines = PageGetMaxOffsetNumber((Page)dp);
    linesleft = lines;
    if (backward)
    {
      lineoff = lines;
      lpp = PageGetItemId(dp, lines);
    }
    else
    {
      lineoff = FirstOffsetNumber;
      lpp = PageGetItemId(dp, FirstOffsetNumber);
    }
  }
}

                    
                                                                       
   
                                                            
   
                                                                               
                                                                           
                                                                             
                                                                      
                                                                          
                          
                    
   
static void
heapgettup_pagemode(HeapScanDesc scan, ScanDirection dir, int nkeys, ScanKey key)
{
  HeapTuple tuple = &(scan->rs_ctup);
  bool backward = ScanDirectionIsBackward(dir);
  BlockNumber page;
  bool finished;
  Page dp;
  int lines;
  int lineindex;
  OffsetNumber lineoff;
  int linesleft;
  ItemId lpp;

     
                                                             
     
  if (ScanDirectionIsForward(dir))
  {
    if (!scan->rs_inited)
    {
         
                                                      
         
      if (scan->rs_nblocks == 0 || scan->rs_numblocks == 0)
      {
        Assert(!BufferIsValid(scan->rs_cbuf));
        tuple->t_data = NULL;
        return;
      }
      if (scan->rs_base.rs_parallel != NULL)
      {
        ParallelBlockTableScanDesc pbscan = (ParallelBlockTableScanDesc)scan->rs_base.rs_parallel;

        table_block_parallelscan_startblock_init(scan->rs_base.rs_rd, pbscan);

        page = table_block_parallelscan_nextpage(scan->rs_base.rs_rd, pbscan);

                                                                   
        if (page == InvalidBlockNumber)
        {
          Assert(!BufferIsValid(scan->rs_cbuf));
          tuple->t_data = NULL;
          return;
        }
      }
      else
      {
        page = scan->rs_startblock;                 
      }
      heapgetpage((TableScanDesc)scan, page);
      lineindex = 0;
      scan->rs_inited = true;
    }
    else
    {
                                                        
      page = scan->rs_cblock;                   
      lineindex = scan->rs_cindex + 1;
    }

    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(scan->rs_base.rs_snapshot, scan->rs_base.rs_rd, dp);
    lines = scan->rs_ntuples;
                                                               

    linesleft = lines - lineindex;
  }
  else if (backward)
  {
                                              
    Assert(scan->rs_base.rs_parallel == NULL);

    if (!scan->rs_inited)
    {
         
                                                      
         
      if (scan->rs_nblocks == 0 || scan->rs_numblocks == 0)
      {
        Assert(!BufferIsValid(scan->rs_cbuf));
        tuple->t_data = NULL;
        return;
      }

         
                                                                       
                                                                         
                                                                      
                           
         
      scan->rs_base.rs_flags &= ~SO_ALLOW_SYNC;

         
                                                                        
                                                                     
         
      if (scan->rs_numblocks != InvalidBlockNumber)
      {
        page = (scan->rs_startblock + scan->rs_numblocks - 1) % scan->rs_nblocks;
      }
      else if (scan->rs_startblock > 0)
      {
        page = scan->rs_startblock - 1;
      }
      else
      {
        page = scan->rs_nblocks - 1;
      }
      heapgetpage((TableScanDesc)scan, page);
    }
    else
    {
                                                        
      page = scan->rs_cblock;                   
    }

    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(scan->rs_base.rs_snapshot, scan->rs_base.rs_rd, dp);
    lines = scan->rs_ntuples;

    if (!scan->rs_inited)
    {
      lineindex = lines - 1;
      scan->rs_inited = true;
    }
    else
    {
      lineindex = scan->rs_cindex - 1;
    }
                                                                   

    linesleft = lineindex + 1;
  }
  else
  {
       
                                                           
       
    if (!scan->rs_inited)
    {
      Assert(!BufferIsValid(scan->rs_cbuf));
      tuple->t_data = NULL;
      return;
    }

    page = ItemPointerGetBlockNumber(&(tuple->t_self));
    if (page != scan->rs_cblock)
    {
      heapgetpage((TableScanDesc)scan, page);
    }

                                                                        
    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(scan->rs_base.rs_snapshot, scan->rs_base.rs_rd, dp);
    lineoff = ItemPointerGetOffsetNumber(&(tuple->t_self));
    lpp = PageGetItemId(dp, lineoff);
    Assert(ItemIdIsNormal(lpp));

    tuple->t_data = (HeapTupleHeader)PageGetItem((Page)dp, lpp);
    tuple->t_len = ItemIdGetLength(lpp);

                                         
    Assert(scan->rs_cindex < scan->rs_ntuples);
    Assert(lineoff == scan->rs_vistuples[scan->rs_cindex]);

    return;
  }

     
                                                                           
             
     
  for (;;)
  {
    while (linesleft > 0)
    {
      lineoff = scan->rs_vistuples[lineindex];
      lpp = PageGetItemId(dp, lineoff);
      Assert(ItemIdIsNormal(lpp));

      tuple->t_data = (HeapTupleHeader)PageGetItem((Page)dp, lpp);
      tuple->t_len = ItemIdGetLength(lpp);
      ItemPointerSet(&(tuple->t_self), page, lineoff);

         
                                                
         
      if (key != NULL)
      {
        bool valid;

        HeapKeyTest(tuple, RelationGetDescr(scan->rs_base.rs_rd), nkeys, key, valid);
        if (valid)
        {
          scan->rs_cindex = lineindex;
          return;
        }
      }
      else
      {
        scan->rs_cindex = lineindex;
        return;
      }

         
                                                     
         
      --linesleft;
      if (backward)
      {
        --lineindex;
      }
      else
      {
        ++lineindex;
      }
    }

       
                                                                           
                                      
       
    if (backward)
    {
      finished = (page == scan->rs_startblock) || (scan->rs_numblocks != InvalidBlockNumber ? --scan->rs_numblocks == 0 : false);
      if (page == 0)
      {
        page = scan->rs_nblocks;
      }
      page--;
    }
    else if (scan->rs_base.rs_parallel != NULL)
    {
      ParallelBlockTableScanDesc pbscan = (ParallelBlockTableScanDesc)scan->rs_base.rs_parallel;

      page = table_block_parallelscan_nextpage(scan->rs_base.rs_rd, pbscan);
      finished = (page == InvalidBlockNumber);
    }
    else
    {
      page++;
      if (page >= scan->rs_nblocks)
      {
        page = 0;
      }
      finished = (page == scan->rs_startblock) || (scan->rs_numblocks != InvalidBlockNumber ? --scan->rs_numblocks == 0 : false);

         
                                                                       
                                                                       
                                                    
         
                                                                      
                                                                      
                                                                         
                                                                         
                                                                         
                                                                      
         
      if (scan->rs_base.rs_flags & SO_ALLOW_SYNC)
      {
        ss_report_location(scan->rs_base.rs_rd, page);
      }
    }

       
                                                    
       
    if (finished)
    {
      if (BufferIsValid(scan->rs_cbuf))
      {
        ReleaseBuffer(scan->rs_cbuf);
      }
      scan->rs_cbuf = InvalidBuffer;
      scan->rs_cblock = InvalidBlockNumber;
      tuple->t_data = NULL;
      scan->rs_inited = false;
      return;
    }

    heapgetpage((TableScanDesc)scan, page);

    dp = BufferGetPage(scan->rs_cbuf);
    TestForOldSnapshot(scan->rs_base.rs_snapshot, scan->rs_base.rs_rd, dp);
    lines = scan->rs_ntuples;
    linesleft = lines;
    if (backward)
    {
      lineindex = lines - 1;
    }
    else
    {
      lineindex = 0;
    }
  }
}

#if defined(DISABLE_COMPLEX_MACRO)
   
                                                                      
                                                      
   
Datum
fastgetattr(HeapTuple tup, int attnum, TupleDesc tupleDesc, bool *isnull)
{
  return ((attnum) > 0 ? ((*(isnull) = false), HeapTupleNoNulls(tup) ? (TupleDescAttr((tupleDesc), (attnum)-1)->attcacheoff >= 0 ? (fetchatt(TupleDescAttr((tupleDesc), (attnum)-1), (char *)(tup)->t_data + (tup)->t_data->t_hoff + TupleDescAttr((tupleDesc), (attnum)-1)->attcacheoff)) : nocachegetattr((tup), (attnum), (tupleDesc))) : (att_isnull((attnum)-1, (tup)->t_data->t_bits) ? ((*(isnull) = true), (Datum)NULL) : (nocachegetattr((tup), (attnum), (tupleDesc))))) : ((Datum)NULL));
}
#endif                                     

                                                                    
                                     
                                                                    
   

TableScanDesc
heap_beginscan(Relation relation, Snapshot snapshot, int nkeys, ScanKey key, ParallelTableScanDesc parallel_scan, uint32 flags)
{
  HeapScanDesc scan;

     
                                                          
     
                                                                             
                                                                          
                                                             
     
  RelationIncrementReferenceCount(relation);

     
                                             
     
  scan = (HeapScanDesc)palloc(sizeof(HeapScanDescData));

  scan->rs_base.rs_rd = relation;
  scan->rs_base.rs_snapshot = snapshot;
  scan->rs_base.rs_nkeys = nkeys;
  scan->rs_base.rs_flags = flags;
  scan->rs_base.rs_parallel = parallel_scan;
  scan->rs_strategy = NULL;                      

     
                                                                   
     
  if (!(snapshot && IsMVCCSnapshot(snapshot)))
  {
    scan->rs_base.rs_flags &= ~SO_ALLOW_PAGEMODE;
  }

     
                                                                           
                                                                         
                                                                            
                                                                            
                                                                             
                                                                        
                                                                         
                                                                          
                                                                             
                                                                           
                       
     
  if (scan->rs_base.rs_flags & (SO_TYPE_SEQSCAN | SO_TYPE_SAMPLESCAN))
  {
       
                                                                  
                                                                   
                                                
       
    Assert(snapshot);
    PredicateLockRelation(relation, snapshot);
  }

                                        
  scan->rs_ctup.t_tableOid = RelationGetRelid(relation);

     
                                                                             
                                                           
     
  if (nkeys > 0)
  {
    scan->rs_base.rs_key = (ScanKey)palloc(sizeof(ScanKeyData) * nkeys);
  }
  else
  {
    scan->rs_base.rs_key = NULL;
  }

  initscan(scan, key, false);

  return (TableScanDesc)scan;
}

void
heap_rescan(TableScanDesc sscan, ScanKey key, bool set_params, bool allow_strat, bool allow_sync, bool allow_pagemode)
{
  HeapScanDesc scan = (HeapScanDesc)sscan;

  if (set_params)
  {
    if (allow_strat)
    {
      scan->rs_base.rs_flags |= SO_ALLOW_STRAT;
    }
    else
    {
      scan->rs_base.rs_flags &= ~SO_ALLOW_STRAT;
    }

    if (allow_sync)
    {
      scan->rs_base.rs_flags |= SO_ALLOW_SYNC;
    }
    else
    {
      scan->rs_base.rs_flags &= ~SO_ALLOW_SYNC;
    }

    if (allow_pagemode && scan->rs_base.rs_snapshot && IsMVCCSnapshot(scan->rs_base.rs_snapshot))
    {
      scan->rs_base.rs_flags |= SO_ALLOW_PAGEMODE;
    }
    else
    {
      scan->rs_base.rs_flags &= ~SO_ALLOW_PAGEMODE;
    }
  }

     
                        
     
  if (BufferIsValid(scan->rs_cbuf))
  {
    ReleaseBuffer(scan->rs_cbuf);
  }

     
                                  
     
  initscan(scan, key, true);
}

void
heap_endscan(TableScanDesc sscan)
{
  HeapScanDesc scan = (HeapScanDesc)sscan;

                                             

     
                        
     
  if (BufferIsValid(scan->rs_cbuf))
  {
    ReleaseBuffer(scan->rs_cbuf);
  }

     
                                                                         
     
  RelationDecrementReferenceCount(scan->rs_base.rs_rd);

  if (scan->rs_base.rs_key)
  {
    pfree(scan->rs_base.rs_key);
  }

  if (scan->rs_strategy != NULL)
  {
    FreeAccessStrategy(scan->rs_strategy);
  }

  if (scan->rs_base.rs_flags & SO_TEMP_SNAPSHOT)
  {
    UnregisterSnapshot(scan->rs_base.rs_snapshot);
  }

  pfree(scan);
}

#ifdef HEAPDEBUGALL
#define HEAPDEBUG_1 elog(DEBUG2, "heap_getnext([%s,nkeys=%d],dir=%d) called", RelationGetRelationName(scan->rs_rd), scan->rs_nkeys, (int)direction)
#define HEAPDEBUG_2 elog(DEBUG2, "heap_getnext returning EOS")
#define HEAPDEBUG_3 elog(DEBUG2, "heap_getnext returning tuple")
#else
#define HEAPDEBUG_1
#define HEAPDEBUG_2
#define HEAPDEBUG_3
#endif                             

HeapTuple
heap_getnext(TableScanDesc sscan, ScanDirection direction)
{
  HeapScanDesc scan = (HeapScanDesc)sscan;

     
                                                                            
                                                                     
                                                                          
                                                                           
                                                      
     
  if (unlikely(sscan->rs_rd->rd_tableam != GetHeapamTableAmRoutine()))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg_internal("only heap AM is supported")));
  }

                                             

  HEAPDEBUG_1;                           

  if (scan->rs_base.rs_flags & SO_ALLOW_PAGEMODE)
  {
    heapgettup_pagemode(scan, direction, scan->rs_base.rs_nkeys, scan->rs_base.rs_key);
  }
  else
  {
    heapgettup(scan, direction, scan->rs_base.rs_nkeys, scan->rs_base.rs_key);
  }

  if (scan->rs_ctup.t_data == NULL)
  {
    HEAPDEBUG_2;                                 
    return NULL;
  }

     
                                                                           
                                                    
     
  HEAPDEBUG_3;                                   

  pgstat_count_heap_getnext(scan->rs_base.rs_rd);

  return &scan->rs_ctup;
}

#ifdef HEAPAMSLOTDEBUGALL
#define HEAPAMSLOTDEBUG_1 elog(DEBUG2, "heapam_getnextslot([%s,nkeys=%d],dir=%d) called", RelationGetRelationName(scan->rs_base.rs_rd), scan->rs_base.rs_nkeys, (int)direction)
#define HEAPAMSLOTDEBUG_2 elog(DEBUG2, "heapam_getnextslot returning EOS")
#define HEAPAMSLOTDEBUG_3 elog(DEBUG2, "heapam_getnextslot returning tuple")
#else
#define HEAPAMSLOTDEBUG_1
#define HEAPAMSLOTDEBUG_2
#define HEAPAMSLOTDEBUG_3
#endif

bool
heap_getnextslot(TableScanDesc sscan, ScanDirection direction, TupleTableSlot *slot)
{
  HeapScanDesc scan = (HeapScanDesc)sscan;

                                             

  HEAPAMSLOTDEBUG_1;                               

  if (sscan->rs_flags & SO_ALLOW_PAGEMODE)
  {
    heapgettup_pagemode(scan, direction, sscan->rs_nkeys, sscan->rs_key);
  }
  else
  {
    heapgettup(scan, direction, sscan->rs_nkeys, sscan->rs_key);
  }

  if (scan->rs_ctup.t_data == NULL)
  {
    HEAPAMSLOTDEBUG_2;                                     
    ExecClearTuple(slot);
    return false;
  }

     
                                                                           
                                                    
     
  HEAPAMSLOTDEBUG_3;                                       

  pgstat_count_heap_getnext(scan->rs_base.rs_rd);

  ExecStoreBufferHeapTuple(&scan->rs_ctup, slot, scan->rs_cbuf);
  return true;
}

   
                                               
   
                                                                           
                                                                          
                                   
   
                                                                            
                                                                            
                                                   
   
                                                                          
                                                                        
                          
   
                                                                               
                                                                            
                                                                          
                                                                  
   
                                                                            
               
   
                                                                             
                                                                               
                                                                           
                                                                       
                                                                            
                                                                              
                                                                           
                                                             
   
bool
heap_fetch(Relation relation, Snapshot snapshot, HeapTuple tuple, Buffer *userbuf)
{
  return heap_fetch_extended(relation, snapshot, tuple, userbuf, false);
}

   
                                                                     
   
                                                                          
                                                                            
                                                                           
                                                                 
                                                                       
   
bool
heap_fetch_extended(Relation relation, Snapshot snapshot, HeapTuple tuple, Buffer *userbuf, bool keep_buf)
{
  ItemPointer tid = &(tuple->t_self);
  ItemId lp;
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  bool valid;

     
                                                         
     
  buffer = ReadBuffer(relation, ItemPointerGetBlockNumber(tid));

     
                                                               
     
  LockBuffer(buffer, BUFFER_LOCK_SHARE);
  page = BufferGetPage(buffer);
  TestForOldSnapshot(snapshot, relation, page);

     
                                                                           
                       
     
  offnum = ItemPointerGetOffsetNumber(tid);
  if (offnum < FirstOffsetNumber || offnum > PageGetMaxOffsetNumber(page))
  {
    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
    ReleaseBuffer(buffer);
    *userbuf = InvalidBuffer;
    tuple->t_data = NULL;
    return false;
  }

     
                                                                  
     
  lp = PageGetItemId(page, offnum);

     
                                   
     
  if (!ItemIdIsNormal(lp))
  {
    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
    ReleaseBuffer(buffer);
    *userbuf = InvalidBuffer;
    tuple->t_data = NULL;
    return false;
  }

     
                           
     
  tuple->t_data = (HeapTupleHeader)PageGetItem(page, lp);
  tuple->t_len = ItemIdGetLength(lp);
  tuple->t_tableOid = RelationGetRelid(relation);

     
                                               
     
  valid = HeapTupleSatisfiesVisibility(tuple, snapshot, buffer);

  if (valid)
  {
    PredicateLockTuple(relation, tuple, snapshot);
  }

  CheckForSerializableConflictOut(valid, relation, tuple, buffer, snapshot);

  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

  if (valid)
  {
       
                                                                      
                                             
       
    *userbuf = buffer;

    return true;
  }

                                                                        
  if (keep_buf)
  {
    *userbuf = buffer;
  }
  else
  {
    ReleaseBuffer(buffer);
    *userbuf = InvalidBuffer;
  }

  return false;
}

   
                                                                           
   
                                                                            
                                                                            
                                                                        
                                                                      
                                                                   
   
                                                                            
                                                                            
                                                    
   
                                                                           
                                                                        
                                 
   
                                                                            
                                                          
   
bool
heap_hot_search_buffer(ItemPointer tid, Relation relation, Buffer buffer, Snapshot snapshot, HeapTuple heapTuple, bool *all_dead, bool first_call)
{
  Page dp = (Page)BufferGetPage(buffer);
  TransactionId prev_xmax = InvalidTransactionId;
  BlockNumber blkno;
  OffsetNumber offnum;
  bool at_chain_start;
  bool valid;
  bool skip;

                                                                             
  if (all_dead)
  {
    *all_dead = first_call;
  }

  blkno = ItemPointerGetBlockNumber(tid);
  offnum = ItemPointerGetOffsetNumber(tid);
  at_chain_start = first_call;
  skip = !first_call;

  Assert(TransactionIdIsValid(RecentGlobalXmin));
  Assert(BufferGetBlockNumber(buffer) == blkno);

                                                           
  for (;;)
  {
    ItemId lp;

                             
    if (offnum < FirstOffsetNumber || offnum > PageGetMaxOffsetNumber(dp))
    {
      break;
    }

    lp = PageGetItemId(dp, offnum);

                                                     
    if (!ItemIdIsNormal(lp))
    {
                                                           
      if (ItemIdIsRedirected(lp) && at_chain_start)
      {
                                 
        offnum = ItemIdGetRedirect(lp);
        at_chain_start = false;
        continue;
      }
                                     
      break;
    }

       
                                                                       
                                                                         
                                                                        
                                                                           
       
    heapTuple->t_data = (HeapTupleHeader)PageGetItem(dp, lp);
    heapTuple->t_len = ItemIdGetLength(lp);
    heapTuple->t_tableOid = RelationGetRelid(relation);
    ItemPointerSet(&heapTuple->t_self, blkno, offnum);

       
                                                       
       
    if (at_chain_start && HeapTupleIsHeapOnly(heapTuple))
    {
      break;
    }

       
                                                                    
               
       
    if (TransactionIdIsValid(prev_xmax) && !TransactionIdEquals(prev_xmax, HeapTupleHeaderGetXmin(heapTuple->t_data)))
    {
      break;
    }

       
                                                                         
                                                                       
                                                                      
                                                                          
                                                     
       
    if (!skip)
    {
                                                               
      valid = HeapTupleSatisfiesVisibility(heapTuple, snapshot, buffer);
      CheckForSerializableConflictOut(valid, relation, heapTuple, buffer, snapshot);

      if (valid)
      {
        ItemPointerSetOffsetNumber(tid, offnum);
        PredicateLockTuple(relation, heapTuple, snapshot);
        if (all_dead)
        {
          *all_dead = false;
        }
        return true;
      }
    }
    skip = false;

       
                                                                    
                                                                
                     
       
                                                                          
                                                                
       
    if (all_dead && *all_dead && !HeapTupleIsSurelyDead(heapTuple, RecentGlobalXmin))
    {
      *all_dead = false;
    }

       
                                                                        
                                        
       
    if (HeapTupleIsHotUpdated(heapTuple))
    {
      Assert(ItemPointerGetBlockNumber(&heapTuple->t_data->t_ctid) == blkno);
      offnum = ItemPointerGetOffsetNumber(&heapTuple->t_data->t_ctid);
      at_chain_start = false;
      prev_xmax = HeapTupleHeaderGetUpdateXid(heapTuple->t_data);
    }
    else
    {
      break;                   
    }
  }

  return false;
}

   
                                                                  
   
                                                                           
                                                                               
                                 
   
                                                                   
                                                                         
                                                      
   
void
heap_get_latest_tid(TableScanDesc sscan, ItemPointer tid)
{
  Relation relation = sscan->rs_rd;
  Snapshot snapshot = sscan->rs_snapshot;
  ItemPointerData ctid;
  TransactionId priorXmax;

     
                                                                            
                                                                             
                        
     
  Assert(ItemPointerIsValid(tid));

     
                                                                            
                                                                           
                  
     
                                                                        
                                                                          
                                                                
     
  ctid = *tid;
  priorXmax = InvalidTransactionId;                              
  for (;;)
  {
    Buffer buffer;
    Page page;
    OffsetNumber offnum;
    ItemId lp;
    HeapTupleData tp;
    bool valid;

       
                                     
       
    buffer = ReadBuffer(relation, ItemPointerGetBlockNumber(&ctid));
    LockBuffer(buffer, BUFFER_LOCK_SHARE);
    page = BufferGetPage(buffer);
    TestForOldSnapshot(snapshot, relation, page);

       
                                                                     
                                                                         
                                                                     
       
    offnum = ItemPointerGetOffsetNumber(&ctid);
    if (offnum < FirstOffsetNumber || offnum > PageGetMaxOffsetNumber(page))
    {
      UnlockReleaseBuffer(buffer);
      break;
    }
    lp = PageGetItemId(page, offnum);
    if (!ItemIdIsNormal(lp))
    {
      UnlockReleaseBuffer(buffer);
      break;
    }

                                
    tp.t_self = ctid;
    tp.t_data = (HeapTupleHeader)PageGetItem(page, lp);
    tp.t_len = ItemIdGetLength(lp);
    tp.t_tableOid = RelationGetRelid(relation);

       
                                                                      
                                     
       
    if (TransactionIdIsValid(priorXmax) && !TransactionIdEquals(priorXmax, HeapTupleHeaderGetXmin(tp.t_data)))
    {
      UnlockReleaseBuffer(buffer);
      break;
    }

       
                                                                    
                  
       
    valid = HeapTupleSatisfiesVisibility(&tp, snapshot, buffer);
    CheckForSerializableConflictOut(valid, relation, &tp, buffer, snapshot);
    if (valid)
    {
      *tid = ctid;
    }

       
                                                                   
       
    if ((tp.t_data->t_infomask & HEAP_XMAX_INVALID) || HeapTupleHeaderIsOnlyLocked(tp.t_data) || HeapTupleHeaderIndicatesMovedPartitions(tp.t_data) || ItemPointerEquals(&tp.t_self, &tp.t_data->t_ctid))
    {
      UnlockReleaseBuffer(buffer);
      break;
    }

    ctid = tp.t_data->t_ctid;
    priorXmax = HeapTupleHeaderGetUpdateXid(tp.t_data);
    UnlockReleaseBuffer(buffer);
  }                  
}

   
                                                                           
   
                                                                              
                                                                           
                                                                            
                                                                          
                                                
   
                                                                            
                       
   
                                                   
   
                                                                  
   
static void
UpdateXmaxHintBits(HeapTupleHeader tuple, Buffer buffer, TransactionId xid)
{
  Assert(TransactionIdEquals(HeapTupleHeaderGetRawXmax(tuple), xid));
  Assert(!(tuple->t_infomask & HEAP_XMAX_IS_MULTI));

  if (!(tuple->t_infomask & (HEAP_XMAX_COMMITTED | HEAP_XMAX_INVALID)))
  {
    if (!HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask) && TransactionIdDidCommit(xid))
    {
      HeapTupleSetHintBits(tuple, buffer, HEAP_XMAX_COMMITTED, xid);
    }
    else
    {
      HeapTupleSetHintBits(tuple, buffer, HEAP_XMAX_INVALID, InvalidTransactionId);
    }
  }
}

   
                                                                
   
BulkInsertState
GetBulkInsertState(void)
{
  BulkInsertState bistate;

  bistate = (BulkInsertState)palloc(sizeof(BulkInsertStateData));
  bistate->strategy = GetAccessStrategy(BAS_BULKWRITE);
  bistate->current_buf = InvalidBuffer;
  return bistate;
}

   
                                                                
   
void
FreeBulkInsertState(BulkInsertState bistate)
{
  if (bistate->current_buf != InvalidBuffer)
  {
    ReleaseBuffer(bistate->current_buf);
  }
  FreeAccessStrategy(bistate->strategy);
  pfree(bistate);
}

   
                                                                          
   
void
ReleaseBulkInsertStatePin(BulkInsertState bistate)
{
  if (bistate->current_buf != InvalidBuffer)
  {
    ReleaseBuffer(bistate->current_buf);
  }
  bistate->current_buf = InvalidBuffer;
}

   
                                           
   
                                                                          
               
   
                                                                             
                                                                
   
                                                                       
                                                                               
                                               
   
                                                                              
                                                                         
                                                                              
                        
   
void
heap_insert(Relation relation, HeapTuple tup, CommandId cid, int options, BulkInsertState bistate)
{
  TransactionId xid = GetCurrentTransactionId();
  HeapTuple heaptup;
  Buffer buffer;
  Buffer vmbuffer = InvalidBuffer;
  bool all_visible_cleared = false;

                                                                         
  Assert(HeapTupleHeaderGetNatts(tup->t_data) <= RelationGetNumberOfAttributes(relation));

     
                                                                   
     
                                                                             
                                                                     
     
  heaptup = heap_prepare_insert(relation, tup, xid, cid, options);

     
                                                                         
                                                           
     
  buffer = RelationGetBufferForTuple(relation, heaptup->t_len, InvalidBuffer, options, bistate, &vmbuffer, NULL);

     
                                                                             
                                                              
     
                                                                          
                                                                         
                                                                          
                                                                           
     
                                                                             
                                                                           
                                                                           
                                                                        
                                                                  
     
  CheckForSerializableConflictIn(relation, NULL, InvalidBuffer);

                                                           
  START_CRIT_SECTION();

  RelationPutHeapTuple(relation, buffer, heaptup, (options & HEAP_INSERT_SPECULATIVE) != 0);

  if (PageIsAllVisible(BufferGetPage(buffer)))
  {
    all_visible_cleared = true;
    PageClearAllVisible(BufferGetPage(buffer));
    visibilitymap_clear(relation, ItemPointerGetBlockNumber(&(heaptup->t_self)), vmbuffer, VISIBILITYMAP_VALID_BITS);
  }

     
                                                      
     
                                                                           
                                                                            
                                                                        
                                                                        
     
                                                                         
     

  MarkBufferDirty(buffer);

                  
  if (!(options & HEAP_INSERT_SKIP_WAL) && RelationNeedsWAL(relation))
  {
    xl_heap_insert xlrec;
    xl_heap_header xlhdr;
    XLogRecPtr recptr;
    Page page = BufferGetPage(buffer);
    uint8 info = XLOG_HEAP_INSERT;
    int bufflags = 0;

       
                                                                       
                                    
       
    if (RelationIsAccessibleInLogicalDecoding(relation))
    {
      log_heap_new_cid(relation, heaptup);
    }

       
                                                                        
                                                                      
                                          
       
    if (ItemPointerGetOffsetNumber(&(heaptup->t_self)) == FirstOffsetNumber && PageGetMaxOffsetNumber(page) == FirstOffsetNumber)
    {
      info |= XLOG_HEAP_INIT_PAGE;
      bufflags |= REGBUF_WILL_INIT;
    }

    xlrec.offnum = ItemPointerGetOffsetNumber(&heaptup->t_self);
    xlrec.flags = 0;
    if (all_visible_cleared)
    {
      xlrec.flags |= XLH_INSERT_ALL_VISIBLE_CLEARED;
    }
    if (options & HEAP_INSERT_SPECULATIVE)
    {
      xlrec.flags |= XLH_INSERT_IS_SPECULATIVE;
    }
    Assert(ItemPointerGetBlockNumber(&heaptup->t_self) == BufferGetBlockNumber(buffer));

       
                                                                          
                                                                          
                                                                         
       
    if (RelationIsLogicallyLogged(relation) && !(options & HEAP_INSERT_NO_LOGICAL))
    {
      xlrec.flags |= XLH_INSERT_CONTAINS_NEW_TUPLE;
      bufflags |= REGBUF_KEEP_DATA;
    }

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHeapInsert);

    xlhdr.t_infomask2 = heaptup->t_data->t_infomask2;
    xlhdr.t_infomask = heaptup->t_data->t_infomask;
    xlhdr.t_hoff = heaptup->t_data->t_hoff;

       
                                                                           
                                                                
                                   
       
    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD | bufflags);
    XLogRegisterBufData(0, (char *)&xlhdr, SizeOfHeapHeader);
                                                             
    XLogRegisterBufData(0, (char *)heaptup->t_data + SizeofHeapTupleHeader, heaptup->t_len - SizeofHeapTupleHeader);

                                                                   
    XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

    recptr = XLogInsert(RM_HEAP_ID, info);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  UnlockReleaseBuffer(buffer);
  if (vmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(vmbuffer);
  }

     
                                                                            
                                                                             
                                                                          
             
     
  CacheInvalidateHeapTuple(relation, heaptup, NULL);

                                                                           
  pgstat_count_heap_insert(relation, 1);

     
                                                                            
                                      
     
  if (heaptup != tup)
  {
    tup->t_self = heaptup->t_self;
    heap_freetuple(heaptup);
  }
}

   
                                                                               
                                                                             
                                                                              
                                                                           
   
static HeapTuple
heap_prepare_insert(Relation relation, HeapTuple tup, TransactionId xid, CommandId cid, int options)
{
     
                                                                             
                                                                      
                                                                          
                                                                            
                                                                             
                                                                             
     
  if (IsParallelWorker())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot insert tuples in a parallel worker")));
  }

  tup->t_data->t_infomask &= ~(HEAP_XACT_MASK);
  tup->t_data->t_infomask2 &= ~(HEAP2_XACT_MASK);
  tup->t_data->t_infomask |= HEAP_XMAX_INVALID;
  HeapTupleHeaderSetXmin(tup->t_data, xid);
  if (options & HEAP_INSERT_FROZEN)
  {
    HeapTupleHeaderSetXminFrozen(tup->t_data);
  }

  HeapTupleHeaderSetCmin(tup->t_data, cid);
  HeapTupleHeaderSetXmax(tup->t_data, 0);                      
  tup->t_tableOid = RelationGetRelid(relation);

     
                                                                         
                                                                          
     
  if (relation->rd_rel->relkind != RELKIND_RELATION && relation->rd_rel->relkind != RELKIND_MATVIEW)
  {
                                                                 
    Assert(!HeapTupleHasExternal(tup));
    return tup;
  }
  else if (HeapTupleHasExternal(tup) || tup->t_len > TOAST_TUPLE_THRESHOLD)
  {
    return toast_insert_or_update(relation, tup, NULL, options);
  }
  else
  {
    return tup;
  }
}

   
                                                         
   
                                                                             
                                                                             
                                                                           
                                                                            
   
                                                                             
                                                               
   
void
heap_multi_insert(Relation relation, TupleTableSlot **slots, int ntuples, CommandId cid, int options, BulkInsertState bistate)
{
  TransactionId xid = GetCurrentTransactionId();
  HeapTuple *heaptuples;
  int i;
  int ndone;
  PGAlignedBlock scratch;
  Page page;
  bool needwal;
  Size saveFreeSpace;
  bool need_tuple_data = RelationIsLogicallyLogged(relation);
  bool need_cids = RelationIsAccessibleInLogicalDecoding(relation);

                                                                       
  AssertArg(!(options & HEAP_INSERT_NO_LOGICAL));

  needwal = !(options & HEAP_INSERT_SKIP_WAL) && RelationNeedsWAL(relation);
  saveFreeSpace = RelationGetTargetPageFreeSpace(relation, HEAP_DEFAULT_FILLFACTOR);

                                                  
  heaptuples = palloc(ntuples * sizeof(HeapTuple));
  for (i = 0; i < ntuples; i++)
  {
    HeapTuple tuple;

    tuple = ExecFetchSlotHeapTuple(slots[i], true, NULL);
    slots[i]->tts_tableOid = RelationGetRelid(relation);
    tuple->t_tableOid = slots[i]->tts_tableOid;
    heaptuples[i] = heap_prepare_insert(relation, tuple, xid, cid, options);
  }

     
                                                                           
                                                                        
           
     
                                                                         
                                                                     
                                                                          
                                                                        
                                                                      
                                                                           
                                                                            
                                                    
     
                                                                        
                                          
     
                                                                            
                                                                            
                                                                           
                                                                        
                                                                  
     
  CheckForSerializableConflictIn(relation, NULL, InvalidBuffer);

  ndone = 0;
  while (ndone < ntuples)
  {
    Buffer buffer;
    Buffer vmbuffer = InvalidBuffer;
    bool all_visible_cleared = false;
    int nthispage;

    CHECK_FOR_INTERRUPTS();

       
                                                                           
                                                                          
       
    buffer = RelationGetBufferForTuple(relation, heaptuples[ndone]->t_len, InvalidBuffer, options, bistate, &vmbuffer, NULL);
    page = BufferGetPage(buffer);

                                                             
    START_CRIT_SECTION();

       
                                                                        
                                                                   
       
    RelationPutHeapTuple(relation, buffer, heaptuples[ndone], false);
    for (nthispage = 1; ndone + nthispage < ntuples; nthispage++)
    {
      HeapTuple heaptup = heaptuples[ndone + nthispage];

      if (PageGetHeapFreeSpace(page) < MAXALIGN(heaptup->t_len) + saveFreeSpace)
      {
        break;
      }

      RelationPutHeapTuple(relation, buffer, heaptup, false);

         
                                                                    
                               
         
      if (needwal && need_cids)
      {
        log_heap_new_cid(relation, heaptup);
      }
    }

    if (PageIsAllVisible(page))
    {
      all_visible_cleared = true;
      PageClearAllVisible(page);
      visibilitymap_clear(relation, BufferGetBlockNumber(buffer), vmbuffer, VISIBILITYMAP_VALID_BITS);
    }

       
                                                                          
       

    MarkBufferDirty(buffer);

                    
    if (needwal)
    {
      XLogRecPtr recptr;
      xl_heap_multi_insert *xlrec;
      uint8 info = XLOG_HEAP2_MULTI_INSERT;
      char *tupledata;
      int totaldatalen;
      char *scratchptr = scratch.data;
      bool init;
      int bufflags = 0;

         
                                                                  
                                               
         
      init = (ItemPointerGetOffsetNumber(&(heaptuples[ndone]->t_self)) == FirstOffsetNumber && PageGetMaxOffsetNumber(page) == FirstOffsetNumber + nthispage - 1);

                                                                      
      xlrec = (xl_heap_multi_insert *)scratchptr;
      scratchptr += SizeOfHeapMultiInsert;

         
                                                                       
                                                                 
                                                                  
                     
         
      if (!init)
      {
        scratchptr += nthispage * sizeof(OffsetNumber);
      }

                                                                
      tupledata = scratchptr;

      xlrec->flags = all_visible_cleared ? XLH_INSERT_ALL_VISIBLE_CLEARED : 0;
      xlrec->ntuples = nthispage;

         
                                                                      
                         
         
      for (i = 0; i < nthispage; i++)
      {
        HeapTuple heaptup = heaptuples[ndone + i];
        xl_multi_insert_tuple *tuphdr;
        int datalen;

        if (!init)
        {
          xlrec->offsets[i] = ItemPointerGetOffsetNumber(&heaptup->t_self);
        }
                                                             
        tuphdr = (xl_multi_insert_tuple *)SHORTALIGN(scratchptr);
        scratchptr = ((char *)tuphdr) + SizeOfMultiInsertTuple;

        tuphdr->t_infomask2 = heaptup->t_data->t_infomask2;
        tuphdr->t_infomask = heaptup->t_data->t_infomask;
        tuphdr->t_hoff = heaptup->t_data->t_hoff;

                                                     
        datalen = heaptup->t_len - SizeofHeapTupleHeader;
        memcpy(scratchptr, (char *)heaptup->t_data + SizeofHeapTupleHeader, datalen);
        tuphdr->datalen = datalen;
        scratchptr += datalen;
      }
      totaldatalen = scratchptr - tupledata;
      Assert((scratchptr - scratch.data) < BLCKSZ);

      if (need_tuple_data)
      {
        xlrec->flags |= XLH_INSERT_CONTAINS_NEW_TUPLE;
      }

         
                                                                  
                                                                         
                                                              
         
      if (ndone + nthispage == ntuples)
      {
        xlrec->flags |= XLH_INSERT_LAST_IN_MULTI;
      }

      if (init)
      {
        info |= XLOG_HEAP_INIT_PAGE;
        bufflags |= REGBUF_WILL_INIT;
      }

         
                                                                     
                                                        
         
      if (need_tuple_data)
      {
        bufflags |= REGBUF_KEEP_DATA;
      }

      XLogBeginInsert();
      XLogRegisterData((char *)xlrec, tupledata - scratch.data);
      XLogRegisterBuffer(0, buffer, REGBUF_STANDARD | bufflags);

      XLogRegisterBufData(0, tupledata, totaldatalen);

                                                                     
      XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

      recptr = XLogInsert(RM_HEAP2_ID, info);

      PageSetLSN(page, recptr);
    }

    END_CRIT_SECTION();

    UnlockReleaseBuffer(buffer);
    if (vmbuffer != InvalidBuffer)
    {
      ReleaseBuffer(vmbuffer);
    }

    ndone += nthispage;
  }

     
                                                                        
                                                                             
                                                                         
                                                                           
                                                                            
                                                      
     
                                                                            
                                                                            
                                                                           
                                                                        
                                  
     
  CheckForSerializableConflictIn(relation, NULL, InvalidBuffer);

     
                                                                           
                                                                          
                                                                          
                        
     
  if (IsCatalogRelation(relation))
  {
    for (i = 0; i < ntuples; i++)
    {
      CacheInvalidateHeapTuple(relation, heaptuples[i], NULL);
    }
  }

                                                     
  for (i = 0; i < ntuples; i++)
  {
    slots[i]->tts_tid = heaptuples[i]->t_self;
  }

  pgstat_count_heap_insert(relation, ntuples);
}

   
                                       
   
                                                                      
                                                                        
   
                                                                             
                                           
   
void
simple_heap_insert(Relation relation, HeapTuple tup)
{
  heap_insert(relation, tup, GetCurrentCommandId(true), 0, NULL);
}

   
                                                                        
                                                                     
                                     
   
                                   
   
static uint8
compute_infobits(uint16 infomask, uint16 infomask2)
{
  return ((infomask & HEAP_XMAX_IS_MULTI) != 0 ? XLHL_XMAX_IS_MULTI : 0) | ((infomask & HEAP_XMAX_LOCK_ONLY) != 0 ? XLHL_XMAX_LOCK_ONLY : 0) | ((infomask & HEAP_XMAX_EXCL_LOCK) != 0 ? XLHL_XMAX_EXCL_LOCK : 0) |
                                                     
         ((infomask & HEAP_XMAX_KEYSHR_LOCK) != 0 ? XLHL_XMAX_KEYSHR_LOCK : 0) | ((infomask2 & HEAP_KEYS_UPDATED) != 0 ? XLHL_KEYS_UPDATED : 0);
}

   
                                                                           
                                                                             
                                                                                
                                                                           
                
   
                                                           
   
static inline bool
xmax_infomask_changed(uint16 new_infomask, uint16 old_infomask)
{
  const uint16 interesting = HEAP_XMAX_IS_MULTI | HEAP_XMAX_LOCK_ONLY | HEAP_LOCK_MASK;

  if ((new_infomask & interesting) != (old_infomask & interesting))
  {
    return true;
  }

  return false;
}

   
                                
   
                                                                              
                                                           
   
                                                                          
                                                                               
                                                                         
                                      
   
TM_Result
heap_delete(Relation relation, ItemPointer tid, CommandId cid, Snapshot crosscheck, bool wait, TM_FailureData *tmfd, bool changingPart)
{
  TM_Result result;
  TransactionId xid = GetCurrentTransactionId();
  ItemId lp;
  HeapTupleData tp;
  Page page;
  BlockNumber block;
  Buffer buffer;
  Buffer vmbuffer = InvalidBuffer;
  TransactionId new_xmax;
  uint16 new_infomask, new_infomask2;
  bool have_tuple_lock = false;
  bool iscombo;
  bool all_visible_cleared = false;
  HeapTuple old_key_tuple = NULL;                                    
  bool old_key_copied = false;

  Assert(ItemPointerIsValid(tid));

     
                                                                           
                                                                          
                                                    
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot delete tuples during a parallel operation")));
  }

  block = ItemPointerGetBlockNumber(tid);
  buffer = ReadBuffer(relation, block);
  page = BufferGetPage(buffer);

     
                                                                             
                                                                             
                                                                            
               
     
  if (PageIsAllVisible(page))
  {
    visibilitymap_pin(relation, block, &vmbuffer);
  }

  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

  lp = PageGetItemId(page, ItemPointerGetOffsetNumber(tid));
  Assert(ItemIdIsNormal(lp));

  tp.t_tableOid = RelationGetRelid(relation);
  tp.t_data = (HeapTupleHeader)PageGetItem(page, lp);
  tp.t_len = ItemIdGetLength(lp);
  tp.t_self = *tid;

l1:
     
                                                                          
                                                                             
                                                                            
                                                        
     
  if (vmbuffer == InvalidBuffer && PageIsAllVisible(page))
  {
    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
    visibilitymap_pin(relation, block, &vmbuffer);
    LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
  }

  result = HeapTupleSatisfiesUpdate(&tp, cid, buffer);

  if (result == TM_Invisible)
  {
    UnlockReleaseBuffer(buffer);
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("attempted to delete invisible tuple")));
  }
  else if (result == TM_BeingModified && wait)
  {
    TransactionId xwait;
    uint16 infomask;

                                                      
    xwait = HeapTupleHeaderGetRawXmax(tp.t_data);
    infomask = tp.t_data->t_infomask;

       
                                                                        
                                                                       
                                                                          
       
                                                                       
                                                                     
                                                          
       
                                                                       
                                                                           
                    
       
    if (infomask & HEAP_XMAX_IS_MULTI)
    {
      bool current_is_member = false;

      if (DoesMultiXactIdConflict((MultiXactId)xwait, infomask, LockTupleExclusive, &current_is_member))
      {
        LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

           
                                                                  
                                                                     
           
        if (!current_is_member)
        {
          heap_acquire_tuplock(relation, &(tp.t_self), LockTupleExclusive, LockWaitBlock, &have_tuple_lock);
        }

                                
        MultiXactIdWait((MultiXactId)xwait, MultiXactStatusUpdate, infomask, relation, &(tp.t_self), XLTW_Delete, NULL);
        LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

           
                                                                   
                                                                       
                                                  
           
                                                                     
                                            
           
        if ((vmbuffer == InvalidBuffer && PageIsAllVisible(page)) || xmax_infomask_changed(tp.t_data->t_infomask, infomask) || !TransactionIdEquals(HeapTupleHeaderGetRawXmax(tp.t_data), xwait))
        {
          goto l1;
        }
      }

         
                                                                         
                                                                     
                                                                       
                                                               
                                                                        
                                                                     
                                                              
         
    }
    else if (!TransactionIdIsCurrentTransactionId(xwait))
    {
         
                                                                       
               
         
      LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
      heap_acquire_tuplock(relation, &(tp.t_self), LockTupleExclusive, LockWaitBlock, &have_tuple_lock);
      XactLockTableWait(xwait, relation, &(tp.t_self), XLTW_Delete);
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

         
                                                                         
                                                                         
                                                      
         
                                                                       
                                      
         
      if ((vmbuffer == InvalidBuffer && PageIsAllVisible(page)) || xmax_infomask_changed(tp.t_data->t_infomask, infomask) || !TransactionIdEquals(HeapTupleHeaderGetRawXmax(tp.t_data), xwait))
      {
        goto l1;
      }

                                                      
      UpdateXmaxHintBits(tp.t_data, buffer, xwait);
    }

       
                                                                         
                                                  
       
    if ((tp.t_data->t_infomask & HEAP_XMAX_INVALID) || HEAP_XMAX_IS_LOCKED_ONLY(tp.t_data->t_infomask) || HeapTupleHeaderIsOnlyLocked(tp.t_data))
    {
      result = TM_Ok;
    }
    else if (!ItemPointerEquals(&tp.t_self, &tp.t_data->t_ctid) || HeapTupleHeaderIndicatesMovedPartitions(tp.t_data))
    {
      result = TM_Updated;
    }
    else
    {
      result = TM_Deleted;
    }
  }

  if (crosscheck != InvalidSnapshot && result == TM_Ok)
  {
                                                                           
    if (!HeapTupleSatisfiesVisibility(&tp, crosscheck, buffer))
    {
      result = TM_Updated;
    }
  }

  if (result != TM_Ok)
  {
    Assert(result == TM_SelfModified || result == TM_Updated || result == TM_Deleted || result == TM_BeingModified);
    Assert(!(tp.t_data->t_infomask & HEAP_XMAX_INVALID));
    Assert(result != TM_Updated || !ItemPointerEquals(&tp.t_self, &tp.t_data->t_ctid));
    tmfd->ctid = tp.t_data->t_ctid;
    tmfd->xmax = HeapTupleHeaderGetUpdateXid(tp.t_data);
    if (result == TM_SelfModified)
    {
      tmfd->cmax = HeapTupleHeaderGetCmax(tp.t_data);
    }
    else
    {
      tmfd->cmax = InvalidCommandId;
    }
    UnlockReleaseBuffer(buffer);
    if (have_tuple_lock)
    {
      UnlockTupleTuplock(relation, &(tp.t_self), LockTupleExclusive);
    }
    if (vmbuffer != InvalidBuffer)
    {
      ReleaseBuffer(vmbuffer);
    }
    return result;
  }

     
                                                                         
                                                              
     
                                                                          
                                                                         
                                                                          
                                                                           
     
  CheckForSerializableConflictIn(relation, &tp, buffer);

                                                 
  HeapTupleHeaderAdjustCmax(tp.t_data, &cid, &iscombo);

     
                                                                            
                                                      
     
  old_key_tuple = ExtractReplicaIdentity(relation, &tp, true, &old_key_copied);

     
                                                                           
                                                                            
                                                                          
                                                                         
                                                                       
                                                                   
     
  MultiXactIdSetOldestMember();

  compute_new_xmax_infomask(HeapTupleHeaderGetRawXmax(tp.t_data), tp.t_data->t_infomask, tp.t_data->t_infomask2, xid, LockTupleExclusive, true, &new_xmax, &new_infomask, &new_infomask2);

  START_CRIT_SECTION();

     
                                                                       
                                                                             
                                                                             
                                                                      
              
     
  PageSetPrunable(page, xid);

  if (PageIsAllVisible(page))
  {
    all_visible_cleared = true;
    PageClearAllVisible(page);
    visibilitymap_clear(relation, BufferGetBlockNumber(buffer), vmbuffer, VISIBILITYMAP_VALID_BITS);
  }

                                                                
  tp.t_data->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
  tp.t_data->t_infomask2 &= ~HEAP_KEYS_UPDATED;
  tp.t_data->t_infomask |= new_infomask;
  tp.t_data->t_infomask2 |= new_infomask2;
  HeapTupleHeaderClearHotUpdated(tp.t_data);
  HeapTupleHeaderSetXmax(tp.t_data, new_xmax);
  HeapTupleHeaderSetCmax(tp.t_data, cid, iscombo);
                                                          
  tp.t_data->t_ctid = tp.t_self;

                                                                  
  if (changingPart)
  {
    HeapTupleHeaderSetMovedPartitions(tp.t_data);
  }

  MarkBufferDirty(buffer);

     
                
     
                                                                       
               
     
  if (RelationNeedsWAL(relation))
  {
    xl_heap_delete xlrec;
    xl_heap_header xlhdr;
    XLogRecPtr recptr;

                                                                             
    if (RelationIsAccessibleInLogicalDecoding(relation))
    {
      log_heap_new_cid(relation, &tp);
    }

    xlrec.flags = 0;
    if (all_visible_cleared)
    {
      xlrec.flags |= XLH_DELETE_ALL_VISIBLE_CLEARED;
    }
    if (changingPart)
    {
      xlrec.flags |= XLH_DELETE_IS_PARTITION_MOVE;
    }
    xlrec.infobits_set = compute_infobits(tp.t_data->t_infomask, tp.t_data->t_infomask2);
    xlrec.offnum = ItemPointerGetOffsetNumber(&tp.t_self);
    xlrec.xmax = new_xmax;

    if (old_key_tuple != NULL)
    {
      if (relation->rd_rel->relreplident == REPLICA_IDENTITY_FULL)
      {
        xlrec.flags |= XLH_DELETE_CONTAINS_OLD_TUPLE;
      }
      else
      {
        xlrec.flags |= XLH_DELETE_CONTAINS_OLD_KEY;
      }
    }

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHeapDelete);

    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

       
                                                                 
       
    if (old_key_tuple != NULL)
    {
      xlhdr.t_infomask2 = old_key_tuple->t_data->t_infomask2;
      xlhdr.t_infomask = old_key_tuple->t_data->t_infomask;
      xlhdr.t_hoff = old_key_tuple->t_data->t_hoff;

      XLogRegisterData((char *)&xlhdr, SizeOfHeapHeader);
      XLogRegisterData((char *)old_key_tuple->t_data + SizeofHeapTupleHeader, old_key_tuple->t_len - SizeofHeapTupleHeader);
    }

                                                                   
    XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

    recptr = XLogInsert(RM_HEAP_ID, XLOG_HEAP_DELETE);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

  if (vmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(vmbuffer);
  }

     
                                                                        
                                                                      
                                                                          
                                                   
     
  if (relation->rd_rel->relkind != RELKIND_RELATION && relation->rd_rel->relkind != RELKIND_MATVIEW)
  {
                                                                 
    Assert(!HeapTupleHasExternal(&tp));
  }
  else if (HeapTupleHasExternal(&tp))
  {
    toast_delete(relation, &tp, false);
  }

     
                                                                    
                                                                         
                                                
     
  CacheInvalidateHeapTuple(relation, &tp, NULL);

                                     
  ReleaseBuffer(buffer);

     
                                                
     
  if (have_tuple_lock)
  {
    UnlockTupleTuplock(relation, &(tp.t_self), LockTupleExclusive);
  }

  pgstat_count_heap_delete(relation);

  if (old_key_tuple != NULL && old_key_copied)
  {
    heap_freetuple(old_key_tuple);
  }

  return TM_Ok;
}

   
                                       
   
                                                                         
                                                                          
                                                                        
                  
   
void
simple_heap_delete(Relation relation, ItemPointer tid)
{
  TM_Result result;
  TM_FailureData tmfd;

  result = heap_delete(relation, tid, GetCurrentCommandId(true), InvalidSnapshot, true                      , &tmfd, false /* changingPart */);
  switch (result)
  {
  case TM_SelfModified:
                                                       
    elog(ERROR, "tuple already updated by self");
    break;

  case TM_Ok:
                           
    break;

  case TM_Updated:
    elog(ERROR, "tuple concurrently updated");
    break;

  case TM_Deleted:
    elog(ERROR, "tuple concurrently deleted");
    break;

  default:
    elog(ERROR, "unrecognized heap_delete status: %u", result);
    break;
  }
}

   
                                 
   
                                                                              
                                                           
   
                                                                          
                                                                               
                                                                         
                                      
   
TM_Result
heap_update(Relation relation, ItemPointer otid, HeapTuple newtup, CommandId cid, Snapshot crosscheck, bool wait, TM_FailureData *tmfd, LockTupleMode *lockmode)
{
  TM_Result result;
  TransactionId xid = GetCurrentTransactionId();
  Bitmapset *hot_attrs;
  Bitmapset *key_attrs;
  Bitmapset *id_attrs;
  Bitmapset *interesting_attrs;
  Bitmapset *modified_attrs;
  ItemId lp;
  HeapTupleData oldtup;
  HeapTuple heaptup;
  HeapTuple old_key_tuple = NULL;
  bool old_key_copied = false;
  Page page;
  BlockNumber block;
  MultiXactStatus mxact_status;
  Buffer buffer, newbuf, vmbuffer = InvalidBuffer, vmbuffer_new = InvalidBuffer;
  bool need_toast;
  Size newtupsize, pagefree;
  bool have_tuple_lock = false;
  bool iscombo;
  bool use_hot_update = false;
  bool hot_attrs_checked = false;
  bool key_intact;
  bool all_visible_cleared = false;
  bool all_visible_cleared_new = false;
  bool checked_lockers;
  bool locker_remains;
  bool id_has_external = false;
  TransactionId xmax_new_tuple, xmax_old_tuple;
  uint16 infomask_old_tuple, infomask2_old_tuple, infomask_new_tuple, infomask2_new_tuple;

  Assert(ItemPointerIsValid(otid));

                                                                         
  Assert(HeapTupleHeaderGetNatts(newtup->t_data) <= RelationGetNumberOfAttributes(relation));

     
                                                                           
                                                                          
                                                    
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot update tuples during a parallel operation")));
  }

     
                                                                        
     
                                                                           
                                                                             
                                                                        
                                                                      
                                                                            
                                                      
     
                                                                            
                                                
     
                                                                        
                                              
     
  hot_attrs = RelationGetIndexAttrBitmap(relation, INDEX_ATTR_BITMAP_ALL);
  key_attrs = RelationGetIndexAttrBitmap(relation, INDEX_ATTR_BITMAP_KEY);
  id_attrs = RelationGetIndexAttrBitmap(relation, INDEX_ATTR_BITMAP_IDENTITY_KEY);

  block = ItemPointerGetBlockNumber(otid);
  buffer = ReadBuffer(relation, block);
  page = BufferGetPage(buffer);

  interesting_attrs = NULL;

     
                                                                            
                                                                        
                                                                          
                                                                          
                                                                         
                                                                   
                                                                             
                                
     
  if (!PageIsFull(page))
  {
    interesting_attrs = bms_add_members(interesting_attrs, hot_attrs);
    hot_attrs_checked = true;
  }
  interesting_attrs = bms_add_members(interesting_attrs, key_attrs);
  interesting_attrs = bms_add_members(interesting_attrs, id_attrs);

     
                                                                             
                                                                             
                                                                            
               
     
  if (PageIsAllVisible(page))
  {
    visibilitymap_pin(relation, block, &vmbuffer);
  }

  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

  lp = PageGetItemId(page, ItemPointerGetOffsetNumber(otid));
  Assert(ItemIdIsNormal(lp));

     
                                                                        
               
     
  oldtup.t_tableOid = RelationGetRelid(relation);
  oldtup.t_data = (HeapTupleHeader)PageGetItem(page, lp);
  oldtup.t_len = ItemIdGetLength(lp);
  oldtup.t_self = *otid;

                                                
  newtup->t_tableOid = RelationGetRelid(relation);

     
                                                                       
                                                                          
                                                                          
                                                                            
                                                                        
                             
     
  modified_attrs = HeapDetermineColumnsInfo(relation, interesting_attrs, id_attrs, &oldtup, newtup, &id_has_external);

     
                                                                             
                                                                         
                              
     
                                                                          
                                                                           
                                                                            
                                                                  
                                                     
     
  if (!bms_overlap(modified_attrs, key_attrs))
  {
    *lockmode = LockTupleNoKeyExclusive;
    mxact_status = MultiXactStatusNoKeyUpdate;
    key_intact = true;

       
                                                                     
                                                                   
                                                                           
                                                                        
                                                                       
                                                                     
                                
       
    MultiXactIdSetOldestMember();
  }
  else
  {
    *lockmode = LockTupleExclusive;
    mxact_status = MultiXactStatusUpdate;
    key_intact = false;
  }

     
                                                                         
                                                                         
                                                                             
                       
     

l2:
  checked_lockers = false;
  locker_remains = false;
  result = HeapTupleSatisfiesUpdate(&oldtup, cid, buffer);

                                          
  Assert(result != TM_BeingModified || wait);

  if (result == TM_Invisible)
  {
    UnlockReleaseBuffer(buffer);
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("attempted to update invisible tuple")));
  }
  else if (result == TM_BeingModified && wait)
  {
    TransactionId xwait;
    uint16 infomask;
    bool can_continue = false;

       
                                                                      
                                                                          
                                                                    
                                                                       
                                                                           
                                                                   
                  
       
                                                                
                             
       

                                                      
    xwait = HeapTupleHeaderGetRawXmax(oldtup.t_data);
    infomask = oldtup.t_data->t_infomask;

       
                                                                         
                                                                        
                                                                          
                                                   
       
                                                                     
                                                                       
                                                                          
                         
       
                                                                       
                                                                     
                                                                        
                                                                         
                                                                        
                                                 
       
                                                                       
                                                                           
                    
       
    if (infomask & HEAP_XMAX_IS_MULTI)
    {
      TransactionId update_xact;
      int remain;
      bool current_is_member = false;

      if (DoesMultiXactIdConflict((MultiXactId)xwait, infomask, *lockmode, &current_is_member))
      {
        LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

           
                                                                  
                                                                     
           
        if (!current_is_member)
        {
          heap_acquire_tuplock(relation, &(oldtup.t_self), *lockmode, LockWaitBlock, &have_tuple_lock);
        }

                                
        MultiXactIdWait((MultiXactId)xwait, mxact_status, infomask, relation, &oldtup.t_self, XLTW_Update, &remain);
        checked_lockers = true;
        locker_remains = remain != 0;
        LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

           
                                                                   
                                                                       
                                                  
           
        if (xmax_infomask_changed(oldtup.t_data->t_infomask, infomask) || !TransactionIdEquals(HeapTupleHeaderGetRawXmax(oldtup.t_data), xwait))
        {
          goto l2;
        }
      }

         
                                                                        
                                                                   
                                                                        
                                                         
                                                                      
                                                                         
               
         
                                                               
                                                                        
                                                                   
                                                                      
                                             
         
                                                                       
                                                                       
                                                                   
                         
         
      if (!HEAP_XMAX_IS_LOCKED_ONLY(oldtup.t_data->t_infomask))
      {
        update_xact = HeapTupleGetUpdateXid(oldtup.t_data);
      }
      else
      {
        update_xact = InvalidTransactionId;
      }

         
                                                                 
                                                                       
                                  
         
      if (!TransactionIdIsValid(update_xact) || TransactionIdDidAbort(update_xact))
      {
        can_continue = true;
      }
    }
    else if (TransactionIdIsCurrentTransactionId(xwait))
    {
         
                                                                       
                                                               
         
      checked_lockers = true;
      locker_remains = true;
      can_continue = true;
    }
    else if (HEAP_XMAX_IS_KEYSHR_LOCKED(infomask) && key_intact)
    {
         
                                                                         
                                                                      
                                
         
      checked_lockers = true;
      locker_remains = true;
      can_continue = true;
    }
    else
    {
         
                                                                       
               
         
      LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
      heap_acquire_tuplock(relation, &(oldtup.t_self), *lockmode, LockWaitBlock, &have_tuple_lock);
      XactLockTableWait(xwait, relation, &oldtup.t_self, XLTW_Update);
      checked_lockers = true;
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

         
                                                                         
                                                                         
                                                      
         
      if (xmax_infomask_changed(oldtup.t_data->t_infomask, infomask) || !TransactionIdEquals(xwait, HeapTupleHeaderGetRawXmax(oldtup.t_data)))
      {
        goto l2;
      }

                                                      
      UpdateXmaxHintBits(oldtup.t_data, buffer, xwait);
      if (oldtup.t_data->t_infomask & HEAP_XMAX_INVALID)
      {
        can_continue = true;
      }
    }

    if (can_continue)
    {
      result = TM_Ok;
    }
    else if (!ItemPointerEquals(&oldtup.t_self, &oldtup.t_data->t_ctid) || HeapTupleHeaderIndicatesMovedPartitions(oldtup.t_data))
    {
      result = TM_Updated;
    }
    else
    {
      result = TM_Deleted;
    }
  }

  if (crosscheck != InvalidSnapshot && result == TM_Ok)
  {
                                                                           
    if (!HeapTupleSatisfiesVisibility(&oldtup, crosscheck, buffer))
    {
      result = TM_Updated;
      Assert(!ItemPointerEquals(&oldtup.t_self, &oldtup.t_data->t_ctid));
    }
  }

  if (result != TM_Ok)
  {
    Assert(result == TM_SelfModified || result == TM_Updated || result == TM_Deleted || result == TM_BeingModified);
    Assert(!(oldtup.t_data->t_infomask & HEAP_XMAX_INVALID));
    Assert(result != TM_Updated || !ItemPointerEquals(&oldtup.t_self, &oldtup.t_data->t_ctid));
    tmfd->ctid = oldtup.t_data->t_ctid;
    tmfd->xmax = HeapTupleHeaderGetUpdateXid(oldtup.t_data);
    if (result == TM_SelfModified)
    {
      tmfd->cmax = HeapTupleHeaderGetCmax(oldtup.t_data);
    }
    else
    {
      tmfd->cmax = InvalidCommandId;
    }
    UnlockReleaseBuffer(buffer);
    if (have_tuple_lock)
    {
      UnlockTupleTuplock(relation, &(oldtup.t_self), *lockmode);
    }
    if (vmbuffer != InvalidBuffer)
    {
      ReleaseBuffer(vmbuffer);
    }
    bms_free(hot_attrs);
    bms_free(key_attrs);
    bms_free(id_attrs);
    bms_free(modified_attrs);
    bms_free(interesting_attrs);
    return result;
  }

     
                                                                          
                                                                   
                                                                             
                                                                            
                                                                             
                                                                       
                        
     
  if (vmbuffer == InvalidBuffer && PageIsAllVisible(page))
  {
    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
    visibilitymap_pin(relation, block, &vmbuffer);
    LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
    goto l2;
  }

                                       

     
                                                                            
                                                                       
     
  compute_new_xmax_infomask(HeapTupleHeaderGetRawXmax(oldtup.t_data), oldtup.t_data->t_infomask, oldtup.t_data->t_infomask2, xid, *lockmode, true, &xmax_old_tuple, &infomask_old_tuple, &infomask2_old_tuple);

     
                                                                             
                                                                            
                                                                           
                                                                   
                                              
     
  if ((oldtup.t_data->t_infomask & HEAP_XMAX_INVALID) || HEAP_LOCKED_UPGRADED(oldtup.t_data->t_infomask) || (checked_lockers && !locker_remains))
  {
    xmax_new_tuple = InvalidTransactionId;
  }
  else
  {
    xmax_new_tuple = HeapTupleHeaderGetRawXmax(oldtup.t_data);
  }

  if (!TransactionIdIsValid(xmax_new_tuple))
  {
    infomask_new_tuple = HEAP_XMAX_INVALID;
    infomask2_new_tuple = 0;
  }
  else
  {
       
                                                                          
                                                                        
                                                                           
                                           
       
    if (oldtup.t_data->t_infomask & HEAP_XMAX_IS_MULTI)
    {
      GetMultiXactIdHintBits(xmax_new_tuple, &infomask_new_tuple, &infomask2_new_tuple);
    }
    else
    {
      infomask_new_tuple = HEAP_XMAX_KEYSHR_LOCK | HEAP_XMAX_LOCK_ONLY;
      infomask2_new_tuple = 0;
    }
  }

     
                                                                           
                                                               
     
  newtup->t_data->t_infomask &= ~(HEAP_XACT_MASK);
  newtup->t_data->t_infomask2 &= ~(HEAP2_XACT_MASK);
  HeapTupleHeaderSetXmin(newtup->t_data, xid);
  HeapTupleHeaderSetCmin(newtup->t_data, cid);
  newtup->t_data->t_infomask |= HEAP_UPDATED | infomask_new_tuple;
  newtup->t_data->t_infomask2 |= infomask2_new_tuple;
  HeapTupleHeaderSetXmax(newtup->t_data, xmax_new_tuple);

     
                                                                          
                                       
     
  HeapTupleHeaderAdjustCmax(oldtup.t_data, &cid, &iscombo);

     
                                                                            
                                                                           
                                                                         
                                                                            
                                                                      
                 
     
                                                                        
                                                                    
     
  if (relation->rd_rel->relkind != RELKIND_RELATION && relation->rd_rel->relkind != RELKIND_MATVIEW)
  {
                                                                 
    Assert(!HeapTupleHasExternal(&oldtup));
    Assert(!HeapTupleHasExternal(newtup));
    need_toast = false;
  }
  else
  {
    need_toast = (HeapTupleHasExternal(&oldtup) || HeapTupleHasExternal(newtup) || newtup->t_len > TOAST_TUPLE_THRESHOLD);
  }

  pagefree = PageGetHeapFreeSpace(page);

  newtupsize = MAXALIGN(newtup->t_len);

  if (need_toast || newtupsize > pagefree)
  {
    TransactionId xmax_lock_old_tuple;
    uint16 infomask_lock_old_tuple, infomask2_lock_old_tuple;
    bool cleared_all_frozen = false;

       
                                                                          
                                                                         
       
                                                                          
                                                                  
                                                                   
                                                                     
                                                                       
                                  
       

       
                                                                           
                                                                        
                                                                           
                 
       
    compute_new_xmax_infomask(HeapTupleHeaderGetRawXmax(oldtup.t_data), oldtup.t_data->t_infomask, oldtup.t_data->t_infomask2, xid, *lockmode, false, &xmax_lock_old_tuple, &infomask_lock_old_tuple, &infomask2_lock_old_tuple);

    Assert(HEAP_XMAX_IS_LOCKED_ONLY(infomask_lock_old_tuple));

    START_CRIT_SECTION();

                                             
    oldtup.t_data->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
    oldtup.t_data->t_infomask2 &= ~HEAP_KEYS_UPDATED;
    HeapTupleClearHotUpdated(&oldtup);
                                                                  
    Assert(TransactionIdIsValid(xmax_lock_old_tuple));
    HeapTupleHeaderSetXmax(oldtup.t_data, xmax_lock_old_tuple);
    oldtup.t_data->t_infomask |= infomask_lock_old_tuple;
    oldtup.t_data->t_infomask2 |= infomask2_lock_old_tuple;
    HeapTupleHeaderSetCmax(oldtup.t_data, cid, iscombo);

                                                          
    oldtup.t_data->t_ctid = oldtup.t_self;

       
                                                                  
                                                                     
                                                                  
                   
       
    if (PageIsAllVisible(page) && visibilitymap_clear(relation, block, vmbuffer, VISIBILITYMAP_ALL_FROZEN))
    {
      cleared_all_frozen = true;
    }

    MarkBufferDirty(buffer);

    if (RelationNeedsWAL(relation))
    {
      xl_heap_lock xlrec;
      XLogRecPtr recptr;

      XLogBeginInsert();
      XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

      xlrec.offnum = ItemPointerGetOffsetNumber(&oldtup.t_self);
      xlrec.locking_xid = xmax_lock_old_tuple;
      xlrec.infobits_set = compute_infobits(oldtup.t_data->t_infomask, oldtup.t_data->t_infomask2);
      xlrec.flags = cleared_all_frozen ? XLH_LOCK_ALL_FROZEN_CLEARED : 0;
      XLogRegisterData((char *)&xlrec, SizeOfHeapLock);
      recptr = XLogInsert(RM_HEAP_ID, XLOG_HEAP_LOCK);
      PageSetLSN(page, recptr);
    }

    END_CRIT_SECTION();

    LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

       
                                                
       
                                                                         
                                                                          
             
       
    if (need_toast)
    {
                                                         
      heaptup = toast_insert_or_update(relation, newtup, &oldtup, 0);
      newtupsize = MAXALIGN(heaptup->t_len);
    }
    else
    {
      heaptup = newtup;
    }

       
                                                                        
                                                                           
                                                                         
                                                                        
                                                                      
                                          
       
                                                                          
                                                                          
                                                                        
                                                                         
                                                                     
                                                                        
                                                                          
                                                            
       
                                                                           
                                                                         
                                                                         
                                                                          
               
       
    for (;;)
    {
      if (newtupsize > pagefree)
      {
                                                                 
        newbuf = RelationGetBufferForTuple(relation, heaptup->t_len, buffer, 0, NULL, &vmbuffer_new, &vmbuffer);
                             
        break;
      }
                                                               
      if (vmbuffer == InvalidBuffer && PageIsAllVisible(page))
      {
        visibilitymap_pin(relation, block, &vmbuffer);
      }
                                                        
      LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
                                                    
      pagefree = PageGetHeapFreeSpace(page);
      if (newtupsize > pagefree || (vmbuffer == InvalidBuffer && PageIsAllVisible(page)))
      {
           
                                                                      
                                                                   
                                                                     
           
        LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
      }
      else
      {
                             
        newbuf = buffer;
        break;
      }
    }
  }
  else
  {
                                                          
    newbuf = buffer;
    heaptup = newtup;
  }

     
                                                                         
                                                              
     
                                                                          
                                                                          
                                                                           
                                                                           
     
                                                                           
                                                                         
                                                                        
                                       
     
  CheckForSerializableConflictIn(relation, &oldtup, buffer);

     
                                                                            
                                                                            
                      
     

  if (newbuf == buffer)
  {
       
                                                                         
                                                                        
                                                                           
                                                          
       
    if (hot_attrs_checked && !bms_overlap(modified_attrs, hot_attrs))
    {
      use_hot_update = true;
    }
  }
  else
  {
                                                             
    PageSetFull(page);
  }

     
                                                                            
                                                      
                                                                      
                                                                             
                                                   
     
  old_key_tuple = ExtractReplicaIdentity(relation, &oldtup, bms_overlap(modified_attrs, id_attrs) || id_has_external, &old_key_copied);

                                                           
  START_CRIT_SECTION();

     
                                                                           
                                                                             
                                                                             
                                                                      
              
     
                                                                           
                                                                          
                                                                             
                                    
     
  PageSetPrunable(page, xid);

  if (use_hot_update)
  {
                                           
    HeapTupleSetHotUpdated(&oldtup);
                                             
    HeapTupleSetHeapOnly(heaptup);
                                                                    
    HeapTupleSetHeapOnly(newtup);
  }
  else
  {
                                                          
    HeapTupleClearHotUpdated(&oldtup);
    HeapTupleClearHeapOnly(heaptup);
    HeapTupleClearHeapOnly(newtup);
  }

  RelationPutHeapTuple(relation, newbuf, heaptup, false);                       

                                                                           
  oldtup.t_data->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
  oldtup.t_data->t_infomask2 &= ~HEAP_KEYS_UPDATED;
                                                                
  Assert(TransactionIdIsValid(xmax_old_tuple));
  HeapTupleHeaderSetXmax(oldtup.t_data, xmax_old_tuple);
  oldtup.t_data->t_infomask |= infomask_old_tuple;
  oldtup.t_data->t_infomask2 |= infomask2_old_tuple;
  HeapTupleHeaderSetCmax(oldtup.t_data, cid, iscombo);

                                                        
  oldtup.t_data->t_ctid = heaptup->t_self;

                                                                
  if (PageIsAllVisible(BufferGetPage(buffer)))
  {
    all_visible_cleared = true;
    PageClearAllVisible(BufferGetPage(buffer));
    visibilitymap_clear(relation, BufferGetBlockNumber(buffer), vmbuffer, VISIBILITYMAP_VALID_BITS);
  }
  if (newbuf != buffer && PageIsAllVisible(BufferGetPage(newbuf)))
  {
    all_visible_cleared_new = true;
    PageClearAllVisible(BufferGetPage(newbuf));
    visibilitymap_clear(relation, BufferGetBlockNumber(newbuf), vmbuffer_new, VISIBILITYMAP_VALID_BITS);
  }

  if (newbuf != buffer)
  {
    MarkBufferDirty(newbuf);
  }
  MarkBufferDirty(buffer);

                  
  if (RelationNeedsWAL(relation))
  {
    XLogRecPtr recptr;

       
                                                                     
                
       
    if (RelationIsAccessibleInLogicalDecoding(relation))
    {
      log_heap_new_cid(relation, &oldtup);
      log_heap_new_cid(relation, heaptup);
    }

    recptr = log_heap_update(relation, buffer, newbuf, &oldtup, heaptup, old_key_tuple, all_visible_cleared, all_visible_cleared_new);
    if (newbuf != buffer)
    {
      PageSetLSN(BufferGetPage(newbuf), recptr);
    }
    PageSetLSN(BufferGetPage(buffer), recptr);
  }

  END_CRIT_SECTION();

  if (newbuf != buffer)
  {
    LockBuffer(newbuf, BUFFER_LOCK_UNLOCK);
  }
  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

     
                                                                        
                                                                            
                                                                          
                                                                             
                                                                          
                       
     
  CacheInvalidateHeapTuple(relation, &oldtup, heaptup);

                                        
  if (newbuf != buffer)
  {
    ReleaseBuffer(newbuf);
  }
  ReleaseBuffer(buffer);
  if (BufferIsValid(vmbuffer_new))
  {
    ReleaseBuffer(vmbuffer_new);
  }
  if (BufferIsValid(vmbuffer))
  {
    ReleaseBuffer(vmbuffer);
  }

     
                                                
     
  if (have_tuple_lock)
  {
    UnlockTupleTuplock(relation, &(oldtup.t_self), *lockmode);
  }

  pgstat_count_heap_update(relation, use_hot_update);

     
                                                                            
                                      
     
  if (heaptup != newtup)
  {
    newtup->t_self = heaptup->t_self;
    heap_freetuple(heaptup);
  }

  if (old_key_tuple != NULL && old_key_copied)
  {
    heap_freetuple(old_key_tuple);
  }

  bms_free(hot_attrs);
  bms_free(key_attrs);
  bms_free(id_attrs);
  bms_free(modified_attrs);
  bms_free(interesting_attrs);

  return TM_Ok;
}

   
                                                                           
                             
   
static bool
heap_attr_equals(TupleDesc tupdesc, int attrnum, Datum value1, Datum value2, bool isnull1, bool isnull2)
{
  Form_pg_attribute att;

     
                                                                        
           
     
  if (isnull1 != isnull2)
  {
    return false;
  }

     
                                                     
     
  if (isnull1)
  {
    return true;
  }

     
                                                                           
                                                                         
                                                                            
                                                                          
                                                                       
                                                                          
                                          
     
  if (attrnum <= 0)
  {
                                                              
    return (DatumGetObjectId(value1) == DatumGetObjectId(value2));
  }
  else
  {
    Assert(attrnum <= tupdesc->natts);
    att = TupleDescAttr(tupdesc, attrnum - 1);
    return datumIsEqual(value1, value2, att->attbyval, att->attlen);
  }
}

   
                                          
   
                                                                             
                                                                      
   
                                                                          
                                                                               
                      
   
                                                                              
                                                      
   
static Bitmapset *
HeapDetermineColumnsInfo(Relation relation, Bitmapset *interesting_cols, Bitmapset *external_cols, HeapTuple oldtup, HeapTuple newtup, bool *has_external)
{
  int attrnum;
  Bitmapset *modified = NULL;
  TupleDesc tupdesc = RelationGetDescr(relation);

  while ((attrnum = bms_first_member(interesting_cols)) >= 0)
  {
    Datum value1, value2;
    bool isnull1, isnull2;

    attrnum += FirstLowInvalidHeapAttributeNumber;

       
                                                                          
                                                                       
                                                                  
       
    if (attrnum == 0)
    {
      modified = bms_add_member(modified, attrnum - FirstLowInvalidHeapAttributeNumber);
      continue;
    }

       
                                                                        
                                                                         
                                                                    
       
    if (attrnum < 0)
    {
      if (attrnum != TableOidAttributeNumber)
      {
        modified = bms_add_member(modified, attrnum - FirstLowInvalidHeapAttributeNumber);
        continue;
      }
    }

       
                                                                         
                                                                 
                                                                       
                                   
       
    value1 = heap_getattr(oldtup, attrnum, tupdesc, &isnull1);
    value2 = heap_getattr(newtup, attrnum, tupdesc, &isnull2);

    if (!heap_attr_equals(tupdesc, attrnum, value1, value2, isnull1, isnull2))
    {
      modified = bms_add_member(modified, attrnum - FirstLowInvalidHeapAttributeNumber);
      continue;
    }

       
                                                                         
                                                          
       
    if (attrnum < 0 || isnull1 || TupleDescAttr(tupdesc, attrnum - 1)->attlen != -1)
    {
      continue;
    }

       
                                                                        
                                
       
    if (VARATT_IS_EXTERNAL((struct varlena *)DatumGetPointer(value1)) && bms_is_member(attrnum - FirstLowInvalidHeapAttributeNumber, external_cols))
    {
      *has_external = true;
    }
  }

  return modified;
}

   
                                        
   
                                                                         
                                                                          
                                                                        
                  
   
void
simple_heap_update(Relation relation, ItemPointer otid, HeapTuple tup)
{
  TM_Result result;
  TM_FailureData tmfd;
  LockTupleMode lockmode;

  result = heap_update(relation, otid, tup, GetCurrentCommandId(true), InvalidSnapshot, true                      , &tmfd, &lockmode);
  switch (result)
  {
  case TM_SelfModified:
                                                       
    elog(ERROR, "tuple already updated by self");
    break;

  case TM_Ok:
                           
    break;

  case TM_Updated:
    elog(ERROR, "tuple concurrently updated");
    break;

  case TM_Deleted:
    elog(ERROR, "tuple concurrently deleted");
    break;

  default:
    elog(ERROR, "unrecognized heap_update status: %u", result);
    break;
  }
}

   
                                                                          
   
static MultiXactStatus
get_mxact_status_for_lock(LockTupleMode mode, bool is_update)
{
  int retval;

  if (is_update)
  {
    retval = tupleLockExtraInfo[mode].updstatus;
  }
  else
  {
    retval = tupleLockExtraInfo[mode].lockstatus;
  }

  if (retval == -1)
  {
    elog(ERROR, "invalid lock tuple mode %d/%s", mode, is_update ? "true" : "false");
  }

  return (MultiXactStatus)retval;
}

   
                                                              
   
                                                                        
   
                     
                                                                        
                             
                                                                      
                                        
                                                                
                                                          
                                                                            
            
   
                      
                                
                                                                        
                                              
   
                                                                     
   
                                                                   
                                                                          
                                                                 
                                                                    
                 
                                                               
   
                                                                    
   
TM_Result
heap_lock_tuple(Relation relation, HeapTuple tuple, CommandId cid, LockTupleMode mode, LockWaitPolicy wait_policy, bool follow_updates, Buffer *buffer, TM_FailureData *tmfd)
{
  TM_Result result;
  ItemPointer tid = &(tuple->t_self);
  ItemId lp;
  Page page;
  Buffer vmbuffer = InvalidBuffer;
  BlockNumber block;
  TransactionId xid, xmax;
  uint16 old_infomask, new_infomask, new_infomask2;
  bool first_time = true;
  bool skip_tuple_lock = false;
  bool have_tuple_lock = false;
  bool cleared_all_frozen = false;

  *buffer = ReadBuffer(relation, ItemPointerGetBlockNumber(tid));
  block = ItemPointerGetBlockNumber(tid);

     
                                                                             
                                                                             
                                                                            
               
     
  if (PageIsAllVisible(BufferGetPage(*buffer)))
  {
    visibilitymap_pin(relation, block, &vmbuffer);
  }

  LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);

  page = BufferGetPage(*buffer);
  lp = PageGetItemId(page, ItemPointerGetOffsetNumber(tid));
  Assert(ItemIdIsNormal(lp));

  tuple->t_data = (HeapTupleHeader)PageGetItem(page, lp);
  tuple->t_len = ItemIdGetLength(lp);
  tuple->t_tableOid = RelationGetRelid(relation);

l3:
  result = HeapTupleSatisfiesUpdate(tuple, cid, *buffer);

  if (result == TM_Invisible)
  {
       
                                                                       
                                                                           
                                                                        
              
       
    result = TM_Invisible;
    goto out_locked;
  }
  else if (result == TM_BeingModified || result == TM_Updated || result == TM_Deleted)
  {
    TransactionId xwait;
    uint16 infomask;
    uint16 infomask2;
    bool require_sleep;
    ItemPointerData t_ctid;

                                                      
    xwait = HeapTupleHeaderGetRawXmax(tuple->t_data);
    infomask = tuple->t_data->t_infomask;
    infomask2 = tuple->t_data->t_infomask2;
    ItemPointerCopy(&tuple->t_data->t_ctid, &t_ctid);

    LockBuffer(*buffer, BUFFER_LOCK_UNLOCK);

       
                                                                          
                                                                      
                                                                     
                                                                    
                                                          
       
                                                                       
                                                                  
                                                                           
                                  
       
    if (first_time)
    {
      first_time = false;

      if (infomask & HEAP_XMAX_IS_MULTI)
      {
        int i;
        int nmembers;
        MultiXactMember *members;

           
                                                                   
                                                                       
                                                 
           
        nmembers = GetMultiXactIdMembers(xwait, &members, false, HEAP_XMAX_IS_LOCKED_ONLY(infomask));

        for (i = 0; i < nmembers; i++)
        {
                                                            
          if (!TransactionIdIsCurrentTransactionId(members[i].xid))
          {
            continue;
          }

          if (TUPLOCK_from_mxstatus(members[i].status) >= mode)
          {
            pfree(members);
            result = TM_Ok;
            goto out_unlocked;
          }
          else
          {
               
                                                                  
                                                                 
                                                                  
                                                             
                                      
               
                                                                
                                                             
                                  
               
            skip_tuple_lock = true;
          }
        }

        if (members)
        {
          pfree(members);
        }
      }
      else if (TransactionIdIsCurrentTransactionId(xwait))
      {
        switch (mode)
        {
        case LockTupleKeyShare:
          Assert(HEAP_XMAX_IS_KEYSHR_LOCKED(infomask) || HEAP_XMAX_IS_SHR_LOCKED(infomask) || HEAP_XMAX_IS_EXCL_LOCKED(infomask));
          result = TM_Ok;
          goto out_unlocked;
        case LockTupleShare:
          if (HEAP_XMAX_IS_SHR_LOCKED(infomask) || HEAP_XMAX_IS_EXCL_LOCKED(infomask))
          {
            result = TM_Ok;
            goto out_unlocked;
          }
          break;
        case LockTupleNoKeyExclusive:
          if (HEAP_XMAX_IS_EXCL_LOCKED(infomask))
          {
            result = TM_Ok;
            goto out_unlocked;
          }
          break;
        case LockTupleExclusive:
          if (HEAP_XMAX_IS_EXCL_LOCKED(infomask) && infomask2 & HEAP_KEYS_UPDATED)
          {
            result = TM_Ok;
            goto out_unlocked;
          }
          break;
        }
      }
    }

       
                                                                  
                                                                        
                               
       
    require_sleep = true;
    if (mode == LockTupleKeyShare)
    {
         
                                                                         
                                                                       
                                                   
         
                                                                         
                                                                       
                                                                  
                                                                      
                                                                 
                                                                       
                                                                        
                                                                       
                             
         
                                                                        
                                                                    
                                                                       
                                                                       
                     
         
      if (!(infomask2 & HEAP_KEYS_UPDATED))
      {
        bool updated;

        updated = !HEAP_XMAX_IS_LOCKED_ONLY(infomask);

           
                                                                      
                                
           
        if (follow_updates && updated)
        {
          TM_Result res;

          res = heap_lock_updated_tuple(relation, tuple, &t_ctid, GetCurrentTransactionId(), mode);
          if (res != TM_Ok)
          {
            result = res;
                                                                
            LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
            goto failed;
          }
        }

        LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);

           
                                                                      
                                                                       
                                                                    
                                                               
                     
           
        if (!HeapTupleHeaderIsOnlyLocked(tuple->t_data) && ((tuple->t_data->t_infomask2 & HEAP_KEYS_UPDATED) || !updated))
        {
          goto l3;
        }

                                                       
        require_sleep = false;

           
                                                                     
                                                                     
                                                                       
                                                                      
                         
           
      }
    }
    else if (mode == LockTupleShare)
    {
         
                                                                       
                                                          
         
      if (HEAP_XMAX_IS_LOCKED_ONLY(infomask) && !HEAP_XMAX_IS_EXCL_LOCKED(infomask))
      {
        LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);

           
                                                                      
                                                    
           
        if (!HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_data->t_infomask) || HEAP_XMAX_IS_EXCL_LOCKED(tuple->t_data->t_infomask))
        {
          goto l3;
        }
        require_sleep = false;
      }
    }
    else if (mode == LockTupleNoKeyExclusive)
    {
         
                                                                      
                                                                    
                           
         
      if (infomask & HEAP_XMAX_IS_MULTI)
      {
        if (!DoesMultiXactIdConflict((MultiXactId)xwait, infomask, mode, NULL))
        {
             
                                                                  
                                   
             
          LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
          if (xmax_infomask_changed(tuple->t_data->t_infomask, infomask) || !TransactionIdEquals(HeapTupleHeaderGetRawXmax(tuple->t_data), xwait))
          {
            goto l3;
          }

                                     
          require_sleep = false;
        }
      }
      else if (HEAP_XMAX_IS_KEYSHR_LOCKED(infomask))
      {
        LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);

                                                             
        if (xmax_infomask_changed(tuple->t_data->t_infomask, infomask) || !TransactionIdEquals(HeapTupleHeaderGetRawXmax(tuple->t_data), xwait))
        {
          goto l3;
        }
                                   
        require_sleep = false;
      }
    }

       
                                                                           
                                                                         
                                                                         
                                                                           
                                                                         
                                                                       
                                    
       
                                                                           
                                                                
       
    if (require_sleep && !(infomask & HEAP_XMAX_IS_MULTI) && TransactionIdIsCurrentTransactionId(xwait))
    {
                                                                   
      LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
      if (xmax_infomask_changed(tuple->t_data->t_infomask, infomask) || !TransactionIdEquals(HeapTupleHeaderGetRawXmax(tuple->t_data), xwait))
      {
        goto l3;
      }
      Assert(HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_data->t_infomask));
      require_sleep = false;
    }

       
                                                                       
       
                                                                   
                                                                      
                                                             
       
                                                                           
                                                                          
                                                           
       
    if (require_sleep && (result == TM_Updated || result == TM_Deleted))
    {
      LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
      goto failed;
    }
    else if (require_sleep)
    {
         
                                                                        
                                                                         
                                                                       
                                                                
         
                                                                         
                                                                  
                                 
         
      if (!skip_tuple_lock && !heap_acquire_tuplock(relation, tid, mode, wait_policy, &have_tuple_lock))
      {
           
                                                                    
                                 
           
        result = TM_WouldBlock;
                                                            
        LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
        goto failed;
      }

      if (infomask & HEAP_XMAX_IS_MULTI)
      {
        MultiXactStatus status = get_mxact_status_for_lock(mode, false);

                                                         
        if (status >= MultiXactStatusNoKeyUpdate)
        {
          elog(ERROR, "invalid lock mode in heap_lock_tuple");
        }

                                                       
        switch (wait_policy)
        {
        case LockWaitBlock:
          MultiXactIdWait((MultiXactId)xwait, status, infomask, relation, &tuple->t_self, XLTW_Lock, NULL);
          break;
        case LockWaitSkip:
          if (!ConditionalMultiXactIdWait((MultiXactId)xwait, status, infomask, relation, NULL))
          {
            result = TM_WouldBlock;
                                                                
            LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
            goto failed;
          }
          break;
        case LockWaitError:
          if (!ConditionalMultiXactIdWait((MultiXactId)xwait, status, infomask, relation, NULL))
          {
            ereport(ERROR, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on row in relation \"%s\"", RelationGetRelationName(relation))));
          }

          break;
        }

           
                                                                     
                                                                       
                                                                     
                                                                   
                                                                   
                                                                       
                       
           
      }
      else
      {
                                                                
        switch (wait_policy)
        {
        case LockWaitBlock:
          XactLockTableWait(xwait, relation, &tuple->t_self, XLTW_Lock);
          break;
        case LockWaitSkip:
          if (!ConditionalXactLockTableWait(xwait))
          {
            result = TM_WouldBlock;
                                                                
            LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
            goto failed;
          }
          break;
        case LockWaitError:
          if (!ConditionalXactLockTableWait(xwait))
          {
            ereport(ERROR, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on row in relation \"%s\"", RelationGetRelationName(relation))));
          }
          break;
        }
      }

                                                         
      if (follow_updates && !HEAP_XMAX_IS_LOCKED_ONLY(infomask))
      {
        TM_Result res;

        res = heap_lock_updated_tuple(relation, tuple, &t_ctid, GetCurrentTransactionId(), mode);
        if (res != TM_Ok)
        {
          result = res;
                                                              
          LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
          goto failed;
        }
      }

      LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);

         
                                                                         
                                                                         
                                                      
         
      if (xmax_infomask_changed(tuple->t_data->t_infomask, infomask) || !TransactionIdEquals(HeapTupleHeaderGetRawXmax(tuple->t_data), xwait))
      {
        goto l3;
      }

      if (!(infomask & HEAP_XMAX_IS_MULTI))
      {
           
                                                                       
                                                                       
                                                                     
                                                                  
                                                                    
                                                       
           
        UpdateXmaxHintBits(tuple->t_data, *buffer, xwait);
      }
    }

                                                                         

       
                                                                         
                                                                          
                                   
       
    if (!require_sleep || (tuple->t_data->t_infomask & HEAP_XMAX_INVALID) || HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_data->t_infomask) || HeapTupleHeaderIsOnlyLocked(tuple->t_data))
    {
      result = TM_Ok;
    }
    else if (!ItemPointerEquals(&tuple->t_self, &tuple->t_data->t_ctid) || HeapTupleHeaderIndicatesMovedPartitions(tuple->t_data))
    {
      result = TM_Updated;
    }
    else
    {
      result = TM_Deleted;
    }
  }

failed:
  if (result != TM_Ok)
  {
    Assert(result == TM_SelfModified || result == TM_Updated || result == TM_Deleted || result == TM_WouldBlock);

       
                                                                          
                                                                         
                                                                       
                                                                    
                                    
       
    Assert((result == TM_WouldBlock) || !(tuple->t_data->t_infomask & HEAP_XMAX_INVALID));
    Assert(result != TM_Updated || !ItemPointerEquals(&tuple->t_self, &tuple->t_data->t_ctid));
    tmfd->ctid = tuple->t_data->t_ctid;
    tmfd->xmax = HeapTupleHeaderGetUpdateXid(tuple->t_data);
    if (result == TM_SelfModified)
    {
      tmfd->cmax = HeapTupleHeaderGetCmax(tuple->t_data);
    }
    else
    {
      tmfd->cmax = InvalidCommandId;
    }
    goto out_locked;
  }

     
                                                                          
                                                                   
                                                                             
                                                                             
                                                                         
                                                                       
                        
     
  if (vmbuffer == InvalidBuffer && PageIsAllVisible(page))
  {
    LockBuffer(*buffer, BUFFER_LOCK_UNLOCK);
    visibilitymap_pin(relation, block, &vmbuffer);
    LockBuffer(*buffer, BUFFER_LOCK_EXCLUSIVE);
    goto l3;
  }

  xmax = HeapTupleHeaderGetRawXmax(tuple->t_data);
  old_infomask = tuple->t_data->t_infomask;

     
                                                                           
                                                                            
                                                                          
                                                                         
                                                                       
                                                                   
     
  MultiXactIdSetOldestMember();

     
                                                                            
                                                                             
                                 
     
  compute_new_xmax_infomask(xmax, old_infomask, tuple->t_data->t_infomask2, GetCurrentTransactionId(), mode, false, &xid, &new_infomask, &new_infomask2);

  START_CRIT_SECTION();

     
                                                              
     
                                                                             
                                                                            
                                                                    
     
                                                                             
                                   
     
  tuple->t_data->t_infomask &= ~HEAP_XMAX_BITS;
  tuple->t_data->t_infomask2 &= ~HEAP_KEYS_UPDATED;
  tuple->t_data->t_infomask |= new_infomask;
  tuple->t_data->t_infomask2 |= new_infomask2;
  if (HEAP_XMAX_IS_LOCKED_ONLY(new_infomask))
  {
    HeapTupleHeaderClearHotUpdated(tuple->t_data);
  }
  HeapTupleHeaderSetXmax(tuple->t_data, xid);

     
                                                                           
                                                                           
                                                                         
                                                                             
                        
     
  if (HEAP_XMAX_IS_LOCKED_ONLY(new_infomask))
  {
    tuple->t_data->t_ctid = *tid;
  }

                                                                 
  if (PageIsAllVisible(page) && visibilitymap_clear(relation, block, vmbuffer, VISIBILITYMAP_ALL_FROZEN))
  {
    cleared_all_frozen = true;
  }

  MarkBufferDirty(*buffer);

     
                                                                            
                                                                           
                                                                     
                                                                           
                                                                         
                                                                      
                                                                            
                                                                        
                                                                            
                                     
     
  if (RelationNeedsWAL(relation))
  {
    xl_heap_lock xlrec;
    XLogRecPtr recptr;

    XLogBeginInsert();
    XLogRegisterBuffer(0, *buffer, REGBUF_STANDARD);

    xlrec.offnum = ItemPointerGetOffsetNumber(&tuple->t_self);
    xlrec.locking_xid = xid;
    xlrec.infobits_set = compute_infobits(new_infomask, tuple->t_data->t_infomask2);
    xlrec.flags = cleared_all_frozen ? XLH_LOCK_ALL_FROZEN_CLEARED : 0;
    XLogRegisterData((char *)&xlrec, SizeOfHeapLock);

                                                                     

    recptr = XLogInsert(RM_HEAP_ID, XLOG_HEAP_LOCK);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  result = TM_Ok;

out_locked:
  LockBuffer(*buffer, BUFFER_LOCK_UNLOCK);

out_unlocked:
  if (BufferIsValid(vmbuffer))
  {
    ReleaseBuffer(vmbuffer);
  }

     
                                                                          
                      
     

     
                                                                      
                                                
     
  if (have_tuple_lock)
  {
    UnlockTupleTuplock(relation, tid, mode);
  }

  return result;
}

   
                                                                             
                                      
   
                                                                            
                                                                         
                                                                             
                             
   
                                                                              
                        
   
static bool
heap_acquire_tuplock(Relation relation, ItemPointer tid, LockTupleMode mode, LockWaitPolicy wait_policy, bool *have_tuple_lock)
{
  if (*have_tuple_lock)
  {
    return true;
  }

  switch (wait_policy)
  {
  case LockWaitBlock:
    LockTupleTuplock(relation, tid, mode);
    break;

  case LockWaitSkip:
    if (!ConditionalLockTupleTuplock(relation, tid, mode))
    {
      return false;
    }
    break;

  case LockWaitError:
    if (!ConditionalLockTupleTuplock(relation, tid, mode))
    {
      ereport(ERROR, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on row in relation \"%s\"", RelationGetRelationName(relation))));
    }
    break;
  }
  *have_tuple_lock = true;

  return true;
}

   
                                                                                
                                                                            
                                                
   
                                                                              
   
                                                                                
                                                                              
                                                                                
                                                                               
                                             
   
                                                                              
                                                                              
                                                                     
                                               
   
static void
compute_new_xmax_infomask(TransactionId xmax, uint16 old_infomask, uint16 old_infomask2, TransactionId add_to_xmax, LockTupleMode mode, bool is_update, TransactionId *result_xmax, uint16 *result_infomask, uint16 *result_infomask2)
{
  TransactionId new_xmax;
  uint16 new_infomask, new_infomask2;

  Assert(TransactionIdIsCurrentTransactionId(add_to_xmax));

l5:
  new_infomask = 0;
  new_infomask2 = 0;
  if (old_infomask & HEAP_XMAX_INVALID)
  {
       
                                                                 
       
                                                                        
                                                                         
                                                                      
                                                                  
       
    if (is_update)
    {
      new_xmax = add_to_xmax;
      if (mode == LockTupleExclusive)
      {
        new_infomask2 |= HEAP_KEYS_UPDATED;
      }
    }
    else
    {
      new_infomask |= HEAP_XMAX_LOCK_ONLY;
      switch (mode)
      {
      case LockTupleKeyShare:
        new_xmax = add_to_xmax;
        new_infomask |= HEAP_XMAX_KEYSHR_LOCK;
        break;
      case LockTupleShare:
        new_xmax = add_to_xmax;
        new_infomask |= HEAP_XMAX_SHR_LOCK;
        break;
      case LockTupleNoKeyExclusive:
        new_xmax = add_to_xmax;
        new_infomask |= HEAP_XMAX_EXCL_LOCK;
        break;
      case LockTupleExclusive:
        new_xmax = add_to_xmax;
        new_infomask |= HEAP_XMAX_EXCL_LOCK;
        new_infomask2 |= HEAP_KEYS_UPDATED;
        break;
      default:
        new_xmax = InvalidTransactionId;                       
        elog(ERROR, "invalid lock mode");
      }
    }
  }
  else if (old_infomask & HEAP_XMAX_IS_MULTI)
  {
    MultiXactStatus new_status;

       
                                                                        
                    
       
    Assert(!(old_infomask & HEAP_XMAX_COMMITTED));

       
                                                                        
                                                                          
                                                                  
                                                                          
                                          
       
    if (HEAP_LOCKED_UPGRADED(old_infomask))
    {
      old_infomask &= ~HEAP_XMAX_IS_MULTI;
      old_infomask |= HEAP_XMAX_INVALID;
      goto l5;
    }

       
                                                                          
                                                                        
                                                                   
                                                                        
                                                                     
              
       
                                                                
                                                                        
                                    
       
    if (!MultiXactIdIsRunning(xmax, HEAP_XMAX_IS_LOCKED_ONLY(old_infomask)))
    {
      if (HEAP_XMAX_IS_LOCKED_ONLY(old_infomask) || !TransactionIdDidCommit(MultiXactIdGetUpdateXid(xmax, old_infomask)))
      {
           
                                                                   
                                     
           
        old_infomask &= ~HEAP_XMAX_IS_MULTI;
        old_infomask |= HEAP_XMAX_INVALID;
        goto l5;
      }
    }

    new_status = get_mxact_status_for_lock(mode, is_update);

    new_xmax = MultiXactIdExpand((MultiXactId)xmax, add_to_xmax, new_status);
    GetMultiXactIdHintBits(new_xmax, &new_infomask, &new_infomask2);
  }
  else if (old_infomask & HEAP_XMAX_COMMITTED)
  {
       
                                                                         
                  
       
    MultiXactStatus status;
    MultiXactStatus new_status;

    if (old_infomask2 & HEAP_KEYS_UPDATED)
    {
      status = MultiXactStatusUpdate;
    }
    else
    {
      status = MultiXactStatusNoKeyUpdate;
    }

    new_status = get_mxact_status_for_lock(mode, is_update);

       
                                                                     
                                                                        
                                                  
       
    new_xmax = MultiXactIdCreate(xmax, status, add_to_xmax, new_status);
    GetMultiXactIdHintBits(new_xmax, &new_infomask, &new_infomask2);
  }
  else if (TransactionIdIsInProgress(xmax))
  {
       
                                                                          
                                                                     
                                          
       
    MultiXactStatus new_status;
    MultiXactStatus old_status;
    LockTupleMode old_mode;

    if (HEAP_XMAX_IS_LOCKED_ONLY(old_infomask))
    {
      if (HEAP_XMAX_IS_KEYSHR_LOCKED(old_infomask))
      {
        old_status = MultiXactStatusForKeyShare;
      }
      else if (HEAP_XMAX_IS_SHR_LOCKED(old_infomask))
      {
        old_status = MultiXactStatusForShare;
      }
      else if (HEAP_XMAX_IS_EXCL_LOCKED(old_infomask))
      {
        if (old_infomask2 & HEAP_KEYS_UPDATED)
        {
          old_status = MultiXactStatusForUpdate;
        }
        else
        {
          old_status = MultiXactStatusForNoKeyUpdate;
        }
      }
      else
      {
           
                                                                    
                                                      
                                                                       
                                                      
           
        elog(WARNING, "LOCK_ONLY found for Xid in progress %u", xmax);
        old_infomask |= HEAP_XMAX_INVALID;
        old_infomask &= ~HEAP_XMAX_LOCK_ONLY;
        goto l5;
      }
    }
    else
    {
                                           
      if (old_infomask2 & HEAP_KEYS_UPDATED)
      {
        old_status = MultiXactStatusUpdate;
      }
      else
      {
        old_status = MultiXactStatusNoKeyUpdate;
      }
    }

    old_mode = TUPLOCK_from_mxstatus(old_status);

       
                                                                       
                                                                          
                                                                     
       
    if (xmax == add_to_xmax)
    {
         
                                                                  
                                                                        
                                                                     
                                                                        
                                                                      
                                                                      
                  
         
      Assert(HEAP_XMAX_IS_LOCKED_ONLY(old_infomask));

                                         
      if (mode < old_mode)
      {
        mode = old_mode;
      }
                                   

      old_infomask |= HEAP_XMAX_INVALID;
      goto l5;
    }

                                                               
    new_status = get_mxact_status_for_lock(mode, is_update);
    new_xmax = MultiXactIdCreate(xmax, old_status, add_to_xmax, new_status);
    GetMultiXactIdHintBits(new_xmax, &new_infomask, &new_infomask2);
  }
  else if (!HEAP_XMAX_IS_LOCKED_ONLY(old_infomask) && TransactionIdDidCommit(xmax))
  {
       
                                                                           
              
       
    MultiXactStatus status;
    MultiXactStatus new_status;

    if (old_infomask2 & HEAP_KEYS_UPDATED)
    {
      status = MultiXactStatusUpdate;
    }
    else
    {
      status = MultiXactStatusNoKeyUpdate;
    }

    new_status = get_mxact_status_for_lock(mode, is_update);

       
                                                                     
                                                                        
                                                  
       
    new_xmax = MultiXactIdCreate(xmax, status, add_to_xmax, new_status);
    GetMultiXactIdHintBits(new_xmax, &new_infomask, &new_infomask2);
  }
  else
  {
       
                                                                          
                                                                      
                                                                           
                                            
       
    old_infomask |= HEAP_XMAX_INVALID;
    goto l5;
  }

  *result_infomask = new_infomask;
  *result_infomask2 = new_infomask2;
  *result_xmax = new_xmax;
}

   
                                               
   
                                                                            
                                                                               
                                                                             
                                                                             
                                                                             
                                                                       
                                                               
   
                                                                               
                                                                              
                              
   
static TM_Result
test_lockmode_for_conflict(MultiXactStatus status, TransactionId xid, LockTupleMode mode, HeapTuple tup, bool *needwait)
{
  MultiXactStatus wantedstatus;

  *needwait = false;
  wantedstatus = get_mxact_status_for_lock(mode, false);

     
                                                            
                                                                             
                         
     
  if (TransactionIdIsCurrentTransactionId(xid))
  {
       
                                                                          
                                                                       
                                                  
       
    return TM_SelfModified;
  }
  else if (TransactionIdIsInProgress(xid))
  {
       
                                                                    
                                                                          
                                                                      
                                
       
    if (DoLockModesConflict(LOCKMODE_from_mxstatus(status), LOCKMODE_from_mxstatus(wantedstatus)))
    {
      *needwait = true;
    }

       
                                                                 
                                                                          
       
    return TM_Ok;
  }
  else if (TransactionIdDidAbort(xid))
  {
    return TM_Ok;
  }
  else if (TransactionIdDidCommit(xid))
  {
       
                                                                           
                                                                        
                                                                      
                                                                       
                                                                           
                                      
       
                                                                           
                                                                       
                                                                     
                                                                     
                                                                           
                          
       
    if (!ISUPDATE_from_mxstatus(status))
    {
      return TM_Ok;
    }

    if (DoLockModesConflict(LOCKMODE_from_mxstatus(status), LOCKMODE_from_mxstatus(wantedstatus)))
    {
                  
      if (!ItemPointerEquals(&tup->t_self, &tup->t_data->t_ctid) || HeapTupleHeaderIndicatesMovedPartitions(tup->t_data))
      {
        return TM_Updated;
      }
      else
      {
        return TM_Deleted;
      }
    }

    return TM_Ok;
  }

                                                                        
  return TM_Ok;
}

   
                                             
   
                                                                                
                                                                              
                    
   
static TM_Result
heap_lock_updated_tuple_rec(Relation rel, ItemPointer tid, TransactionId xid, LockTupleMode mode)
{
  TM_Result result;
  ItemPointerData tupid;
  HeapTupleData mytup;
  Buffer buf;
  uint16 new_infomask, new_infomask2, old_infomask, old_infomask2;
  TransactionId xmax, new_xmax;
  TransactionId priorXmax = InvalidTransactionId;
  bool cleared_all_frozen = false;
  bool pinned_desired_page;
  Buffer vmbuffer = InvalidBuffer;
  BlockNumber block;

  ItemPointerCopy(tid, &tupid);

  for (;;)
  {
    new_infomask = 0;
    new_xmax = InvalidTransactionId;
    block = ItemPointerGetBlockNumber(&tupid);
    ItemPointerCopy(&tupid, &(mytup.t_self));

    if (!heap_fetch(rel, SnapshotAny, &mytup, &buf))
    {
         
                                                                   
                                                               
                                                                        
                                                                        
                 
         
      result = TM_Ok;
      goto out_unlocked;
    }

  l4:
    CHECK_FOR_INTERRUPTS();

       
                                                                    
                                                                    
                                                                           
                                          
       
    if (PageIsAllVisible(BufferGetPage(buf)))
    {
      visibilitymap_pin(rel, block, &vmbuffer);
      pinned_desired_page = true;
    }
    else
    {
      pinned_desired_page = false;
    }

    LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);

       
                                                                        
                                                                        
                                                                        
                                                                       
       
                                                                     
                                                                          
                                                                       
                                                                       
       
    if (!pinned_desired_page && PageIsAllVisible(BufferGetPage(buf)))
    {
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
      visibilitymap_pin(rel, block, &vmbuffer);
      LockBuffer(buf, BUFFER_LOCK_EXCLUSIVE);
    }

       
                                                                           
                                                        
       
    if (TransactionIdIsValid(priorXmax) && !TransactionIdEquals(HeapTupleHeaderGetXmin(mytup.t_data), priorXmax))
    {
      result = TM_Ok;
      goto out_locked;
    }

       
                                                                
                                                                         
                                                  
       
    if (TransactionIdDidAbort(HeapTupleHeaderGetXmin(mytup.t_data)))
    {
      result = TM_Ok;
      goto out_locked;
    }

    old_infomask = mytup.t_data->t_infomask;
    old_infomask2 = mytup.t_data->t_infomask2;
    xmax = HeapTupleHeaderGetRawXmax(mytup.t_data);

       
                                                                           
                                                                   
                                                                          
                       
       
    if (!(old_infomask & HEAP_XMAX_INVALID))
    {
      TransactionId rawxmax;
      bool needwait;

      rawxmax = HeapTupleHeaderGetRawXmax(mytup.t_data);
      if (old_infomask & HEAP_XMAX_IS_MULTI)
      {
        int nmembers;
        int i;
        MultiXactMember *members;

           
                                                                      
                                                                       
                                                                   
                                                                       
                                                                 
                                                                       
                                  
           
        Assert(!HEAP_LOCKED_UPGRADED(mytup.t_data->t_infomask));

        nmembers = GetMultiXactIdMembers(rawxmax, &members, false, HEAP_XMAX_IS_LOCKED_ONLY(old_infomask));
        for (i = 0; i < nmembers; i++)
        {
          result = test_lockmode_for_conflict(members[i].status, members[i].xid, mode, &mytup, &needwait);

             
                                                               
                                                                 
                                                                    
                                                                   
                                                                   
                                                                  
                                                                     
                           
             
          if (result == TM_SelfModified)
          {
            pfree(members);
            goto next;
          }

          if (needwait)
          {
            LockBuffer(buf, BUFFER_LOCK_UNLOCK);
            XactLockTableWait(members[i].xid, rel, &mytup.t_self, XLTW_LockUpdated);
            pfree(members);
            goto l4;
          }
          if (result != TM_Ok)
          {
            pfree(members);
            goto out_locked;
          }
        }
        if (members)
        {
          pfree(members);
        }
      }
      else
      {
        MultiXactStatus status;

           
                                                              
                                                                     
           
        if (HEAP_XMAX_IS_LOCKED_ONLY(old_infomask))
        {
          if (HEAP_XMAX_IS_KEYSHR_LOCKED(old_infomask))
          {
            status = MultiXactStatusForKeyShare;
          }
          else if (HEAP_XMAX_IS_SHR_LOCKED(old_infomask))
          {
            status = MultiXactStatusForShare;
          }
          else if (HEAP_XMAX_IS_EXCL_LOCKED(old_infomask))
          {
            if (old_infomask2 & HEAP_KEYS_UPDATED)
            {
              status = MultiXactStatusForUpdate;
            }
            else
            {
              status = MultiXactStatusForNoKeyUpdate;
            }
          }
          else
          {
               
                                                                   
                                                                
                                                      
               
            elog(ERROR, "invalid lock status in tuple");
          }
        }
        else
        {
                                               
          if (old_infomask2 & HEAP_KEYS_UPDATED)
          {
            status = MultiXactStatusUpdate;
          }
          else
          {
            status = MultiXactStatusNoKeyUpdate;
          }
        }

        result = test_lockmode_for_conflict(status, rawxmax, mode, &mytup, &needwait);

           
                                                                      
                                                                
                                                                       
                                                                       
                                                                   
                                                                 
                                                         
           
        if (result == TM_SelfModified)
        {
          goto next;
        }

        if (needwait)
        {
          LockBuffer(buf, BUFFER_LOCK_UNLOCK);
          XactLockTableWait(rawxmax, rel, &mytup.t_self, XLTW_LockUpdated);
          goto l4;
        }
        if (result != TM_Ok)
        {
          goto out_locked;
        }
      }
    }

                                                                    
    compute_new_xmax_infomask(xmax, old_infomask, mytup.t_data->t_infomask2, xid, mode, false, &new_xmax, &new_infomask, &new_infomask2);

    if (PageIsAllVisible(BufferGetPage(buf)) && visibilitymap_clear(rel, block, vmbuffer, VISIBILITYMAP_ALL_FROZEN))
    {
      cleared_all_frozen = true;
    }

    START_CRIT_SECTION();

                          
    HeapTupleHeaderSetXmax(mytup.t_data, new_xmax);
    mytup.t_data->t_infomask &= ~HEAP_XMAX_BITS;
    mytup.t_data->t_infomask2 &= ~HEAP_KEYS_UPDATED;
    mytup.t_data->t_infomask |= new_infomask;
    mytup.t_data->t_infomask2 |= new_infomask2;

    MarkBufferDirty(buf);

                    
    if (RelationNeedsWAL(rel))
    {
      xl_heap_lock_updated xlrec;
      XLogRecPtr recptr;
      Page page = BufferGetPage(buf);

      XLogBeginInsert();
      XLogRegisterBuffer(0, buf, REGBUF_STANDARD);

      xlrec.offnum = ItemPointerGetOffsetNumber(&mytup.t_self);
      xlrec.xmax = new_xmax;
      xlrec.infobits_set = compute_infobits(new_infomask, new_infomask2);
      xlrec.flags = cleared_all_frozen ? XLH_LOCK_ALL_FROZEN_CLEARED : 0;

      XLogRegisterData((char *)&xlrec, SizeOfHeapLockUpdated);

      recptr = XLogInsert(RM_HEAP2_ID, XLOG_HEAP2_LOCK_UPDATED);

      PageSetLSN(page, recptr);
    }

    END_CRIT_SECTION();

  next:
                                                         
    if (mytup.t_data->t_infomask & HEAP_XMAX_INVALID || HeapTupleHeaderIndicatesMovedPartitions(mytup.t_data) || ItemPointerEquals(&mytup.t_self, &mytup.t_data->t_ctid) || HeapTupleHeaderIsOnlyLocked(mytup.t_data))
    {
      result = TM_Ok;
      goto out_locked;
    }

                        
    priorXmax = HeapTupleHeaderGetUpdateXid(mytup.t_data);
    ItemPointerCopy(&(mytup.t_data->t_ctid), &tupid);
    UnlockReleaseBuffer(buf);
  }

  result = TM_Ok;

out_locked:
  UnlockReleaseBuffer(buf);

out_unlocked:
  if (vmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(vmbuffer);
  }

  return result;
}

   
                           
                                                                            
                                    
   
                                                      
   
                                                                             
                                                                           
                                                                          
                                     
   
                                                                            
                                                                              
                                                                       
                                                                          
                                                                             
                                                                               
                                                                            
                                                                         
                                                                 
   
static TM_Result
heap_lock_updated_tuple(Relation rel, HeapTuple tuple, ItemPointer ctid, TransactionId xid, LockTupleMode mode)
{
     
                                                                            
                                       
     
  if (!HeapTupleHeaderIndicatesMovedPartitions(tuple->t_data) && !ItemPointerEquals(&tuple->t_self, ctid))
  {
       
                                                                     
                                                                   
                                                                           
                                                                        
                                                                       
                                                                     
                                
       
    MultiXactIdSetOldestMember();

    return heap_lock_updated_tuple_rec(rel, ctid, xid, mode);
  }

                       
  return TM_Ok;
}

   
                                                                      
   
                                                                               
                                                                      
                                                                               
                                                               
   
                                                                     
                                                                          
                                                                               
                                                                          
                                                                             
                                                                     
                                                                            
   
void
heap_finish_speculative(Relation relation, ItemPointer tid)
{
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  ItemId lp = NULL;
  HeapTupleHeader htup;

  buffer = ReadBuffer(relation, ItemPointerGetBlockNumber(tid));
  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
  page = (Page)BufferGetPage(buffer);

  offnum = ItemPointerGetOffsetNumber(tid);
  if (PageGetMaxOffsetNumber(page) >= offnum)
  {
    lp = PageGetItemId(page, offnum);
  }

  if (PageGetMaxOffsetNumber(page) < offnum || !ItemIdIsNormal(lp))
  {
    elog(ERROR, "invalid lp");
  }

  htup = (HeapTupleHeader)PageGetItem(page, lp);

                                                                            
  StaticAssertStmt(MaxOffsetNumber < SpecTokenOffsetNumber, "invalid speculative token constant");

                                                           
  START_CRIT_SECTION();

  Assert(HeapTupleHeaderIsSpeculative(htup));

  MarkBufferDirty(buffer);

     
                                                                             
                                            
     
  htup->t_ctid = *tid;

                  
  if (RelationNeedsWAL(relation))
  {
    xl_heap_confirm xlrec;
    XLogRecPtr recptr;

    xlrec.offnum = ItemPointerGetOffsetNumber(tid);

    XLogBeginInsert();

                                                                 
    XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

    XLogRegisterData((char *)&xlrec, SizeOfHeapConfirm);
    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

    recptr = XLogInsert(RM_HEAP_ID, XLOG_HEAP_CONFIRM);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  UnlockReleaseBuffer(buffer);
}

   
                                                                
   
                                                                              
                                                                             
                                                                    
                                                                               
                                                                          
                                                                           
            
   
                                                                            
                                                                       
                                                                      
                                                                                
                                                                              
                                                                            
                                                                  
   
                                                                        
                                                                              
                                                                         
   
                                                                     
                         
   
void
heap_abort_speculative(Relation relation, ItemPointer tid)
{
  TransactionId xid = GetCurrentTransactionId();
  ItemId lp;
  HeapTupleData tp;
  Page page;
  BlockNumber block;
  Buffer buffer;
  TransactionId prune_xid;

  Assert(ItemPointerIsValid(tid));

  block = ItemPointerGetBlockNumber(tid);
  buffer = ReadBuffer(relation, block);
  page = BufferGetPage(buffer);

  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

     
                                                                        
              
     
  Assert(!PageIsAllVisible(page));

  lp = PageGetItemId(page, ItemPointerGetOffsetNumber(tid));
  Assert(ItemIdIsNormal(lp));

  tp.t_tableOid = RelationGetRelid(relation);
  tp.t_data = (HeapTupleHeader)PageGetItem(page, lp);
  tp.t_len = ItemIdGetLength(lp);
  tp.t_self = *tid;

     
                                                                           
                     
     
  if (tp.t_data->t_choice.t_heap.t_xmin != xid)
  {
    elog(ERROR, "attempted to kill a tuple inserted by another transaction");
  }
  if (!(IsToastRelation(relation) || HeapTupleHeaderIsSpeculative(tp.t_data)))
  {
    elog(ERROR, "attempted to kill a non-speculative tuple");
  }
  Assert(!HeapTupleHeaderIsHeapOnly(tp.t_data));

     
                                                                         
                                                                           
                                             
     

  START_CRIT_SECTION();

     
                                                                       
                                                                         
                                                                         
                                                                   
                                                                          
                                                                            
                                                                  
                                                
     
  Assert(TransactionIdIsValid(TransactionXmin));
  if (TransactionIdPrecedes(TransactionXmin, relation->rd_rel->relfrozenxid))
  {
    prune_xid = relation->rd_rel->relfrozenxid;
  }
  else
  {
    prune_xid = TransactionXmin;
  }
  PageSetPrunable(page, prune_xid);

                                                                
  tp.t_data->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
  tp.t_data->t_infomask2 &= ~HEAP_KEYS_UPDATED;

     
                                                                        
                                                                   
                                                                     
     
  HeapTupleHeaderSetXmin(tp.t_data, InvalidTransactionId);

                                                 
  tp.t_data->t_ctid = tp.t_self;

  MarkBufferDirty(buffer);

     
                
     
                                                                            
                        
     
  if (RelationNeedsWAL(relation))
  {
    xl_heap_delete xlrec;
    XLogRecPtr recptr;

    xlrec.flags = XLH_DELETE_IS_SUPER;
    xlrec.infobits_set = compute_infobits(tp.t_data->t_infomask, tp.t_data->t_infomask2);
    xlrec.offnum = ItemPointerGetOffsetNumber(&tp.t_self);
    xlrec.xmax = xid;

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHeapDelete);
    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

                                                         

    recptr = XLogInsert(RM_HEAP_ID, XLOG_HEAP_DELETE);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  LockBuffer(buffer, BUFFER_LOCK_UNLOCK);

  if (HeapTupleHasExternal(&tp))
  {
    Assert(!IsToastRelation(relation));
    toast_delete(relation, &tp, true);
  }

     
                                                                             
                           
     

                                     
  ReleaseBuffer(buffer);

                                                       
  pgstat_count_heap_delete(relation);
}

   
                                                                      
   
                                                                        
                                                                       
                               
   
                                                                         
                                                                    
                                                                     
                                       
   
                                                                           
                                                                            
   
                                                                          
                                                                     
                                                                         
   
void
heap_inplace_update(Relation relation, HeapTuple tuple)
{
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  ItemId lp = NULL;
  HeapTupleHeader htup;
  uint32 oldlen;
  uint32 newlen;

     
                                                                         
                                                                          
                                                                       
                                                                           
     
  if (IsInParallelMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TRANSACTION_STATE), errmsg("cannot update tuples during a parallel operation")));
  }

  buffer = ReadBuffer(relation, ItemPointerGetBlockNumber(&(tuple->t_self)));
  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);
  page = (Page)BufferGetPage(buffer);

  offnum = ItemPointerGetOffsetNumber(&(tuple->t_self));
  if (PageGetMaxOffsetNumber(page) >= offnum)
  {
    lp = PageGetItemId(page, offnum);
  }

  if (PageGetMaxOffsetNumber(page) < offnum || !ItemIdIsNormal(lp))
  {
    elog(ERROR, "invalid lp");
  }

  htup = (HeapTupleHeader)PageGetItem(page, lp);

  oldlen = ItemIdGetLength(lp) - htup->t_hoff;
  newlen = tuple->t_len - tuple->t_data->t_hoff;
  if (oldlen != newlen || htup->t_hoff != tuple->t_data->t_hoff)
  {
    elog(ERROR, "wrong tuple length");
  }

                                                           
  START_CRIT_SECTION();

  memcpy((char *)htup + htup->t_hoff, (char *)tuple->t_data + tuple->t_data->t_hoff, newlen);

  MarkBufferDirty(buffer);

                  
  if (RelationNeedsWAL(relation))
  {
    xl_heap_inplace xlrec;
    XLogRecPtr recptr;

    xlrec.offnum = ItemPointerGetOffsetNumber(&tuple->t_self);

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfHeapInplace);

    XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);
    XLogRegisterBufData(0, (char *)htup + htup->t_hoff, newlen);

                                                                  

    recptr = XLogInsert(RM_HEAP_ID, XLOG_HEAP_INPLACE);

    PageSetLSN(page, recptr);
  }

  END_CRIT_SECTION();

  UnlockReleaseBuffer(buffer);

     
                                                                          
                                                                     
                                                                       
                                                                     
     
  if (!IsBootstrapProcessingMode())
  {
    CacheInvalidateHeapTuple(relation, tuple, NULL);
  }
}

#define FRM_NOOP 0x0001
#define FRM_INVALIDATE_XMAX 0x0002
#define FRM_RETURN_IS_XID 0x0004
#define FRM_RETURN_IS_MULTI 0x0008
#define FRM_MARK_COMMITTED 0x0010

   
                     
                                                                     
                 
   
                                                                        
   
                                                                              
                       
            
                                            
                       
                                                                 
                     
                                                                
                      
                                              
                       
                                                              
                                                                           
   
static TransactionId
FreezeMultiXactId(MultiXactId multi, uint16 t_infomask, TransactionId relfrozenxid, TransactionId relminmxid, TransactionId cutoff_xid, MultiXactId cutoff_multi, uint16 *flags)
{
  TransactionId xid = InvalidTransactionId;
  int i;
  MultiXactMember *members;
  int nmembers;
  bool need_replace;
  int nnewmembers;
  MultiXactMember *newmembers;
  bool has_lockers;
  TransactionId update_xid;
  bool update_committed;

  *flags = 0;

                                          
  Assert(t_infomask & HEAP_XMAX_IS_MULTI);

  if (!MultiXactIdIsValid(multi) || HEAP_LOCKED_UPGRADED(t_infomask))
  {
                                                          
    *flags |= FRM_INVALIDATE_XMAX;
    return InvalidTransactionId;
  }
  else if (MultiXactIdPrecedes(multi, relminmxid))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("found multixact %u from before relminmxid %u", multi, relminmxid)));
  }
  else if (MultiXactIdPrecedes(multi, cutoff_multi))
  {
       
                                                                      
                                                                        
                                                                         
                                     
       
    if (MultiXactIdIsRunning(multi, HEAP_XMAX_IS_LOCKED_ONLY(t_infomask)))
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("multixact %u from before cutoff %u found to be still running", multi, cutoff_multi)));
    }

    if (HEAP_XMAX_IS_LOCKED_ONLY(t_infomask))
    {
      *flags |= FRM_INVALIDATE_XMAX;
      xid = InvalidTransactionId;                             
    }
    else
    {
                                       
      xid = MultiXactIdGetUpdateXid(multi, t_infomask);

                                                     
      Assert(TransactionIdIsValid(xid));

      if (TransactionIdPrecedes(xid, relfrozenxid))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("found update xid %u from before relfrozenxid %u", xid, relfrozenxid)));
      }

         
                                                                      
                                                            
         
      if (TransactionIdPrecedes(xid, cutoff_xid))
      {
        if (TransactionIdDidCommit(xid))
        {
          ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("cannot freeze committed update xid %u", xid)));
        }
        *flags |= FRM_INVALIDATE_XMAX;
        xid = InvalidTransactionId;                             
      }
      else
      {
        *flags |= FRM_RETURN_IS_XID;
      }
    }

    return xid;
  }

     
                                                                            
                                                                       
                                                                           
                                                                       
               
     

  nmembers = GetMultiXactIdMembers(multi, &members, false, HEAP_XMAX_IS_LOCKED_ONLY(t_infomask));
  if (nmembers <= 0)
  {
                               
    *flags |= FRM_INVALIDATE_XMAX;
    return InvalidTransactionId;
  }

                                                
  need_replace = false;
  for (i = 0; i < nmembers; i++)
  {
    if (TransactionIdPrecedes(members[i].xid, cutoff_xid))
    {
      need_replace = true;
      break;
    }
  }

     
                                                                            
                                          
     
  if (!need_replace)
  {
    *flags |= FRM_NOOP;
    pfree(members);
    return InvalidTransactionId;
  }

     
                                                                           
              
     
  nnewmembers = 0;
  newmembers = palloc(sizeof(MultiXactMember) * nmembers);
  has_lockers = false;
  update_xid = InvalidTransactionId;
  update_committed = false;

  for (i = 0; i < nmembers; i++)
  {
       
                                                           
       
    if (ISUPDATE_from_mxstatus(members[i].status))
    {
      TransactionId xid = members[i].xid;

      Assert(TransactionIdIsValid(xid));
      if (TransactionIdPrecedes(xid, relfrozenxid))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("found update xid %u from before relfrozenxid %u", xid, relfrozenxid)));
      }

         
                                                                         
                                                                        
                                                                       
                                                                         
                                                                        
         
                                                                      
                                                                  
                                                           
                              
         
      if (TransactionIdIsCurrentTransactionId(xid) || TransactionIdIsInProgress(xid))
      {
        Assert(!TransactionIdIsValid(update_xid));
        update_xid = xid;
      }
      else if (TransactionIdDidCommit(xid))
      {
           
                                                                   
                                                                      
                                            
           
        Assert(!TransactionIdIsValid(update_xid));
        update_committed = true;
        update_xid = xid;
      }
      else
      {
           
                                                                
                                      
           
      }

         
                                                                     
                                                                      
                                                                         
                    
         
      if (TransactionIdIsValid(update_xid) && TransactionIdPrecedes(update_xid, cutoff_xid))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("found update xid %u from before xid cutoff %u", update_xid, cutoff_xid)));
      }

         
                                                                      
                                                                   
                                                                      
                                                                       
                                                                         
                
         
      if (TransactionIdIsValid(update_xid))
      {
        newmembers[nnewmembers++] = members[i];
      }
    }
    else
    {
                                                          
      if (TransactionIdIsCurrentTransactionId(members[i].xid) || TransactionIdIsInProgress(members[i].xid))
      {
                                                                     
        Assert(!TransactionIdPrecedes(members[i].xid, cutoff_xid));
        newmembers[nnewmembers++] = members[i];
        has_lockers = true;
      }
    }
  }

  pfree(members);

  if (nnewmembers == 0)
  {
                                                                       
    *flags |= FRM_INVALIDATE_XMAX;
    xid = InvalidTransactionId;
  }
  else if (TransactionIdIsValid(update_xid) && !has_lockers)
  {
       
                                                                         
                                                                           
                                                                           
                                                                
                                                     
       
    Assert(nnewmembers == 1);
    *flags |= FRM_RETURN_IS_XID;
    if (update_committed)
    {
      *flags |= FRM_MARK_COMMITTED;
    }
    xid = update_xid;
  }
  else
  {
       
                                                                         
                                             
       
    xid = MultiXactIdCreateFromMembers(nnewmembers, newmembers);
    *flags |= FRM_RETURN_IS_MULTI;
  }

  pfree(newmembers);

  return xid;
}

   
                             
   
                                                                            
                                                                           
                                                                         
                                                                               
                                                                              
                                                                            
                                              
   
                                                                       
   
                                                            
                                                                           
                                                            
   
                                                                           
                                                                         
                                                                        
                                     
                                                                      
                                                       
   
                                                                             
                
   
                                                                  
                                                                           
                                                 
   
bool
heap_prepare_freeze_tuple(HeapTupleHeader tuple, TransactionId relfrozenxid, TransactionId relminmxid, TransactionId cutoff_xid, TransactionId cutoff_multi, xl_heap_freeze_tuple *frz, bool *totally_frozen_p)
{
  bool changed = false;
  bool xmax_already_frozen = false;
  bool xmin_frozen;
  bool freeze_xmax;
  TransactionId xid;

  frz->frzflags = 0;
  frz->t_infomask2 = tuple->t_infomask2;
  frz->t_infomask = tuple->t_infomask;
  frz->xmax = HeapTupleHeaderGetRawXmax(tuple);

     
                                                                            
                                                                            
                                                                            
                                                                        
                                                                            
                           
     
  xid = HeapTupleHeaderGetXmin(tuple);
  if (!TransactionIdIsNormal(xid))
  {
    xmin_frozen = true;
  }
  else
  {
    if (TransactionIdPrecedes(xid, relfrozenxid))
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("found xmin %u from before relfrozenxid %u", xid, relfrozenxid)));
    }

    xmin_frozen = TransactionIdPrecedes(xid, cutoff_xid);
    if (xmin_frozen)
    {
      if (!TransactionIdDidCommit(xid))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("uncommitted xmin %u from before xid cutoff %u needs to be frozen", xid, cutoff_xid)));
      }

      frz->t_infomask |= HEAP_XMIN_FROZEN;
      changed = true;
    }
  }

     
                                                                            
                                                                        
                                                                             
                                                                            
                                                                    
     
                                                                  
     
  xid = HeapTupleHeaderGetRawXmax(tuple);

  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    TransactionId newxmax;
    uint16 flags;

    newxmax = FreezeMultiXactId(xid, tuple->t_infomask, relfrozenxid, relminmxid, cutoff_xid, cutoff_multi, &flags);

    freeze_xmax = (flags & FRM_INVALIDATE_XMAX);

    if (flags & FRM_RETURN_IS_XID)
    {
         
                                                                       
                                                                   
                                                                     
                                                              
                                             
         
      frz->t_infomask &= ~HEAP_XMAX_BITS;
      frz->xmax = newxmax;
      if (flags & FRM_MARK_COMMITTED)
      {
        frz->t_infomask |= HEAP_XMAX_COMMITTED;
      }
      changed = true;
    }
    else if (flags & FRM_RETURN_IS_MULTI)
    {
      uint16 newbits;
      uint16 newbits2;

         
                                                                       
                                                                       
                                                                       
                                          
         
      frz->t_infomask &= ~HEAP_XMAX_BITS;
      frz->t_infomask2 &= ~HEAP_KEYS_UPDATED;
      GetMultiXactIdHintBits(newxmax, &newbits, &newbits2);
      frz->t_infomask |= newbits;
      frz->t_infomask2 |= newbits2;

      frz->xmax = newxmax;

      changed = true;
    }
  }
  else if (TransactionIdIsNormal(xid))
  {
    if (TransactionIdPrecedes(xid, relfrozenxid))
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("found xmax %u from before relfrozenxid %u", xid, relfrozenxid)));
    }

    if (TransactionIdPrecedes(xid, cutoff_xid))
    {
         
                                                                      
                                                                    
                                                                         
                             
         
      if (!HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask) && TransactionIdDidCommit(xid))
      {
        ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("cannot freeze committed xmax %u", xid)));
      }
      freeze_xmax = true;
    }
    else
    {
      freeze_xmax = false;
    }
  }
  else if ((tuple->t_infomask & HEAP_XMAX_INVALID) || !TransactionIdIsValid(HeapTupleHeaderGetRawXmax(tuple)))
  {
    freeze_xmax = false;
    xmax_already_frozen = true;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_CORRUPTED), errmsg_internal("found xmax %u (infomask 0x%04x) not frozen, not multi, not normal", xid, tuple->t_infomask)));
  }

  if (freeze_xmax)
  {
    Assert(!xmax_already_frozen);

    frz->xmax = InvalidTransactionId;

       
                                                                         
                                                                           
                                                  
       
    frz->t_infomask &= ~HEAP_XMAX_BITS;
    frz->t_infomask |= HEAP_XMAX_INVALID;
    frz->t_infomask2 &= ~HEAP_HOT_UPDATED;
    frz->t_infomask2 &= ~HEAP_KEYS_UPDATED;
    changed = true;
  }

     
                                                                             
                                                                  
     
  if (tuple->t_infomask & HEAP_MOVED)
  {
    xid = HeapTupleHeaderGetXvac(tuple);

       
                                                                      
                                                                       
                                                                        
                                                                       
                                                                         
                                               
       
    if (TransactionIdIsNormal(xid))
    {
         
                                                                     
                                                                      
                                     
         
      if (tuple->t_infomask & HEAP_MOVED_OFF)
      {
        frz->frzflags |= XLH_INVALID_XVAC;
      }
      else
      {
        frz->frzflags |= XLH_FREEZE_XVAC;
      }

         
                                                                     
                                                                   
         
      Assert(!(tuple->t_infomask & HEAP_XMIN_INVALID));
      frz->t_infomask |= HEAP_XMIN_COMMITTED;
      changed = true;
    }
  }

  *totally_frozen_p = (xmin_frozen && (freeze_xmax || xmax_already_frozen));
  return changed;
}

   
                             
                                              
   
                                                                           
                                                                             
                                                                             
                                                              
   
                                                                               
                                                                               
                                                                               
                                                                            
                                                                              
                                                                         
                                                                           
                  
   
                                                                       
   
void
heap_execute_freeze_tuple(HeapTupleHeader tuple, xl_heap_freeze_tuple *frz)
{
  HeapTupleHeaderSetXmax(tuple, frz->xmax);

  if (frz->frzflags & XLH_FREEZE_XVAC)
  {
    HeapTupleHeaderSetXvac(tuple, FrozenTransactionId);
  }

  if (frz->frzflags & XLH_INVALID_XVAC)
  {
    HeapTupleHeaderSetXvac(tuple, InvalidTransactionId);
  }

  tuple->t_infomask = frz->t_infomask;
  tuple->t_infomask2 = frz->t_infomask2;
}

   
                     
                                                
   
                                                                       
   
bool
heap_freeze_tuple(HeapTupleHeader tuple, TransactionId relfrozenxid, TransactionId relminmxid, TransactionId cutoff_xid, TransactionId cutoff_multi)
{
  xl_heap_freeze_tuple frz;
  bool do_freeze;
  bool tuple_totally_frozen;

  do_freeze = heap_prepare_freeze_tuple(tuple, relfrozenxid, relminmxid, cutoff_xid, cutoff_multi, &frz, &tuple_totally_frozen);

     
                                                                            
                                              
     

  if (do_freeze)
  {
    heap_execute_freeze_tuple(tuple, &frz);
  }
  return do_freeze;
}

   
                                                                           
                     
   
                                                                             
                                                             
   
static void
GetMultiXactIdHintBits(MultiXactId multi, uint16 *new_infomask, uint16 *new_infomask2)
{
  int nmembers;
  MultiXactMember *members;
  int i;
  uint16 bits = HEAP_XMAX_IS_MULTI;
  uint16 bits2 = 0;
  bool has_update = false;
  LockTupleMode strongest = LockTupleKeyShare;

     
                                                                          
                     
     
  nmembers = GetMultiXactIdMembers(multi, &members, false, false);

  for (i = 0; i < nmembers; i++)
  {
    LockTupleMode mode;

       
                                                                  
                  
       
    mode = TUPLOCK_from_mxstatus(members[i].status);
    if (mode > strongest)
    {
      strongest = mode;
    }

                                     
    switch (members[i].status)
    {
    case MultiXactStatusForKeyShare:
    case MultiXactStatusForShare:
    case MultiXactStatusForNoKeyUpdate:
      break;

    case MultiXactStatusForUpdate:
      bits2 |= HEAP_KEYS_UPDATED;
      break;

    case MultiXactStatusNoKeyUpdate:
      has_update = true;
      break;

    case MultiXactStatusUpdate:
      bits2 |= HEAP_KEYS_UPDATED;
      has_update = true;
      break;
    }
  }

  if (strongest == LockTupleExclusive || strongest == LockTupleNoKeyExclusive)
  {
    bits |= HEAP_XMAX_EXCL_LOCK;
  }
  else if (strongest == LockTupleShare)
  {
    bits |= HEAP_XMAX_SHR_LOCK;
  }
  else if (strongest == LockTupleKeyShare)
  {
    bits |= HEAP_XMAX_KEYSHR_LOCK;
  }

  if (!has_update)
  {
    bits |= HEAP_XMAX_LOCK_ONLY;
  }

  if (nmembers > 0)
  {
    pfree(members);
  }

  *new_infomask = bits;
  *new_infomask2 = bits2;
}

   
                           
   
                                                                              
                                                                          
                
   
                                                                          
              
   
static TransactionId
MultiXactIdGetUpdateXid(TransactionId xmax, uint16 t_infomask)
{
  TransactionId update_xact = InvalidTransactionId;
  MultiXactMember *members;
  int nmembers;

  Assert(!(t_infomask & HEAP_XMAX_LOCK_ONLY));
  Assert(t_infomask & HEAP_XMAX_IS_MULTI);

     
                                                                             
                     
     
  nmembers = GetMultiXactIdMembers(xmax, &members, false, false);

  if (nmembers > 0)
  {
    int i;

    for (i = 0; i < nmembers; i++)
    {
                          
      if (!ISUPDATE_from_mxstatus(members[i].status))
      {
        continue;
      }

                                            
      Assert(update_xact == InvalidTransactionId);
      update_xact = members[i].xid;
#ifndef USE_ASSERT_CHECKING

         
                                                                    
                                   
         
      break;
#endif
    }

    pfree(members);
  }

  return update_xact;
}

   
                         
                                        
   
                                                                              
                           
   
TransactionId
HeapTupleGetUpdateXid(HeapTupleHeader tuple)
{
  return MultiXactIdGetUpdateXid(HeapTupleHeaderGetRawXmax(tuple), tuple->t_infomask);
}

   
                                                                             
                                     
   
                                                                              
   
                                                                        
                                                   
   
static bool
DoesMultiXactIdConflict(MultiXactId multi, uint16 infomask, LockTupleMode lockmode, bool *current_is_member)
{
  int nmembers;
  MultiXactMember *members;
  bool result = false;
  LOCKMODE wanted = tupleLockExtraInfo[lockmode].hwlock;

  if (HEAP_LOCKED_UPGRADED(infomask))
  {
    return false;
  }

  nmembers = GetMultiXactIdMembers(multi, &members, false, HEAP_XMAX_IS_LOCKED_ONLY(infomask));
  if (nmembers >= 0)
  {
    int i;

    for (i = 0; i < nmembers; i++)
    {
      TransactionId memxid;
      LOCKMODE memlockmode;

      if (result && (current_is_member == NULL || *current_is_member))
      {
        break;
      }

      memlockmode = LOCKMODE_from_mxstatus(members[i].status);

                                                                       
      memxid = members[i].xid;
      if (TransactionIdIsCurrentTransactionId(memxid))
      {
        if (current_is_member != NULL)
        {
          *current_is_member = true;
        }
        continue;
      }
      else if (result)
      {
        continue;
      }

                                                                    
      if (!DoLockModesConflict(memlockmode, wanted))
      {
        continue;
      }

      if (ISUPDATE_from_mxstatus(members[i].status))
      {
                                     
        if (TransactionIdDidAbort(memxid))
        {
          continue;
        }
      }
      else
      {
                                                                
        if (!TransactionIdIsInProgress(memxid))
        {
          continue;
        }
      }

         
                                                                         
                                                                         
                                                                      
                                                                       
                    
         
      result = true;
    }
    pfree(members);
  }

  return result;
}

   
                      
                                                       
   
                                                                             
                                                                              
                                                                               
                                                                              
                                                                             
                                                                               
                                                                             
                                           
   
                                                                       
                                                                             
                                                                            
                                                                        
                                                                         
                                                                              
                                                                   
                 
   
                                                                              
                                                                          
   
                                                                         
                      
   
static bool
Do_MultiXactIdWait(MultiXactId multi, MultiXactStatus status, uint16 infomask, bool nowait, Relation rel, ItemPointer ctid, XLTW_Oper oper, int *remaining)
{
  bool result = true;
  MultiXactMember *members;
  int nmembers;
  int remain = 0;

                                                          
  nmembers = HEAP_LOCKED_UPGRADED(infomask) ? -1 : GetMultiXactIdMembers(multi, &members, false, HEAP_XMAX_IS_LOCKED_ONLY(infomask));

  if (nmembers >= 0)
  {
    int i;

    for (i = 0; i < nmembers; i++)
    {
      TransactionId memxid = members[i].xid;
      MultiXactStatus memstatus = members[i].status;

      if (TransactionIdIsCurrentTransactionId(memxid))
      {
        remain++;
        continue;
      }

      if (!DoLockModesConflict(LOCKMODE_from_mxstatus(memstatus), LOCKMODE_from_mxstatus(status)))
      {
        if (remaining && TransactionIdIsInProgress(memxid))
        {
          remain++;
        }
        continue;
      }

         
                                                                       
                                                     
         
                                                                        
                                                                       
                                                                     
                                                                       
                                                                    
         
      if (nowait)
      {
        result = ConditionalXactLockTableWait(memxid);
        if (!result)
        {
          break;
        }
      }
      else
      {
        XactLockTableWait(memxid, rel, ctid, oper);
      }
    }

    pfree(members);
  }

  if (remaining)
  {
    *remaining = remain;
  }

  return result;
}

   
                   
                            
   
                                                                          
                                                                          
   
                                                                               
                                                                                
   
static void
MultiXactIdWait(MultiXactId multi, MultiXactStatus status, uint16 infomask, Relation rel, ItemPointer ctid, XLTW_Oper oper, int *remaining)
{
  (void)Do_MultiXactIdWait(multi, status, infomask, false, rel, ctid, oper, remaining);
}

   
                              
                                                                     
   
                                                                          
                                                                          
   
                                                                         
                                        
   
                                                                               
                                                                                
   
static bool
ConditionalMultiXactIdWait(MultiXactId multi, MultiXactStatus status, uint16 infomask, Relation rel, int *remaining)
{
  return Do_MultiXactIdWait(multi, status, infomask, true, rel, NULL, XLTW_None, remaining);
}

   
                                    
   
                                                                            
                                                                          
                                                                            
                                                     
   
bool
heap_tuple_needs_eventual_freeze(HeapTupleHeader tuple)
{
  TransactionId xid;

     
                                                                      
             
     
  xid = HeapTupleHeaderGetXmin(tuple);
  if (TransactionIdIsNormal(xid))
  {
    return true;
  }

     
                                                                          
     
  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    MultiXactId multi;

    multi = HeapTupleHeaderGetRawXmax(tuple);
    if (MultiXactIdIsValid(multi))
    {
      return true;
    }
  }
  else
  {
    xid = HeapTupleHeaderGetRawXmax(tuple);
    if (TransactionIdIsNormal(xid))
    {
      return true;
    }
  }

  if (tuple->t_infomask & HEAP_MOVED)
  {
    xid = HeapTupleHeaderGetXvac(tuple);
    if (TransactionIdIsNormal(xid))
    {
      return true;
    }
  }

  return false;
}

   
                           
   
                                                                            
                                                                                
   
                                                                         
                                                                        
   
                                                                             
                 
   
bool
heap_tuple_needs_freeze(HeapTupleHeader tuple, TransactionId cutoff_xid, MultiXactId cutoff_multi, Buffer buf)
{
  TransactionId xid;

  xid = HeapTupleHeaderGetXmin(tuple);
  if (TransactionIdIsNormal(xid) && TransactionIdPrecedes(xid, cutoff_xid))
  {
    return true;
  }

     
                                                                
                                                                            
                               
     
  if (tuple->t_infomask & HEAP_XMAX_IS_MULTI)
  {
    MultiXactId multi;

    multi = HeapTupleHeaderGetRawXmax(tuple);
    if (!MultiXactIdIsValid(multi))
    {
                               
      ;
    }
    else if (HEAP_LOCKED_UPGRADED(tuple->t_infomask))
    {
      return true;
    }
    else if (MultiXactIdPrecedes(multi, cutoff_multi))
    {
      return true;
    }
    else
    {
      MultiXactMember *members;
      int nmembers;
      int i;

                                                                    

      nmembers = GetMultiXactIdMembers(multi, &members, false, HEAP_XMAX_IS_LOCKED_ONLY(tuple->t_infomask));

      for (i = 0; i < nmembers; i++)
      {
        if (TransactionIdPrecedes(members[i].xid, cutoff_xid))
        {
          pfree(members);
          return true;
        }
      }
      if (nmembers > 0)
      {
        pfree(members);
      }
    }
  }
  else
  {
    xid = HeapTupleHeaderGetRawXmax(tuple);
    if (TransactionIdIsNormal(xid) && TransactionIdPrecedes(xid, cutoff_xid))
    {
      return true;
    }
  }

  if (tuple->t_infomask & HEAP_MOVED)
  {
    xid = HeapTupleHeaderGetXvac(tuple);
    if (TransactionIdIsNormal(xid) && TransactionIdPrecedes(xid, cutoff_xid))
    {
      return true;
    }
  }

  return false;
}

   
                                                                      
                                                                
                                                                      
                                                                     
                 
   
void
HeapTupleHeaderAdvanceLatestRemovedXid(HeapTupleHeader tuple, TransactionId *latestRemovedXid)
{
  TransactionId xmin = HeapTupleHeaderGetXmin(tuple);
  TransactionId xmax = HeapTupleHeaderGetUpdateXid(tuple);
  TransactionId xvac = HeapTupleHeaderGetXvac(tuple);

  if (tuple->t_infomask & HEAP_MOVED)
  {
    if (TransactionIdPrecedes(*latestRemovedXid, xvac))
    {
      *latestRemovedXid = xvac;
    }
  }

     
                                                                          
                                                   
     
                                                                          
                                                                        
                                  
     
  if (HeapTupleHeaderXminCommitted(tuple) || (!HeapTupleHeaderXminInvalid(tuple) && TransactionIdDidCommit(xmin)))
  {
    if (xmax != xmin && TransactionIdFollows(xmax, *latestRemovedXid))
    {
      *latestRemovedXid = xmax;
    }
  }

                                                     
}

#ifdef USE_PREFETCH
   
                                                                            
                                                                        
                                                                          
                                                                             
                                        
   
static void
xid_horizon_prefetch_buffer(Relation rel, XidHorizonPrefetchState *prefetch_state, int prefetch_count)
{
  BlockNumber cur_hblkno = prefetch_state->cur_hblkno;
  int count = 0;
  int i;
  int nitems = prefetch_state->nitems;
  ItemPointerData *tids = prefetch_state->tids;

  for (i = prefetch_state->next_item; i < nitems && count < prefetch_count; i++)
  {
    ItemPointer htid = &tids[i];

    if (cur_hblkno == InvalidBlockNumber || ItemPointerGetBlockNumber(htid) != cur_hblkno)
    {
      cur_hblkno = ItemPointerGetBlockNumber(htid);
      PrefetchBuffer(rel, MAIN_FORKNUM, cur_hblkno);
      count++;
    }
  }

     
                                                                            
               
     
  prefetch_state->next_item = i;
  prefetch_state->cur_hblkno = cur_hblkno;
}
#endif

   
                                                                        
                         
   
                                                                           
                                                                           
                                                                            
                                                                            
                                                                         
   
                                                                            
                                                                            
                                                                              
                   
   
TransactionId
heap_compute_xid_horizon_for_tuples(Relation rel, ItemPointerData *tids, int nitems)
{
                                                                        
  TransactionId latestRemovedXid = InvalidTransactionId;
  BlockNumber blkno = InvalidBlockNumber;
  Buffer buf = InvalidBuffer;
  Page page = NULL;
  OffsetNumber maxoff = InvalidOffsetNumber;
  TransactionId priorXmax;
#ifdef USE_PREFETCH
  XidHorizonPrefetchState prefetch_state;
  int io_concurrency;
  int prefetch_distance;
#endif

     
                                                                           
                                                                       
                                                                            
                                                              
     
  qsort((void *)tids, nitems, sizeof(ItemPointerData), (int (*)(const void *, const void *))ItemPointerCompare);

#ifdef USE_PREFETCH
                                  
  prefetch_state.cur_hblkno = InvalidBlockNumber;
  prefetch_state.next_item = 0;
  prefetch_state.nitems = nitems;
  prefetch_state.tids = tids;

     
                                                                     
     
                                                                        
                                                                        
                                                                        
                                                                            
                                                                           
                                                                            
                                                                            
                                   
     
                                                                             
                                                                      
                                                  
     
  if (IsCatalogRelation(rel))
  {
    io_concurrency = effective_io_concurrency;
  }
  else
  {
    io_concurrency = get_tablespace_io_concurrency(rel->rd_rel->reltablespace);
  }
  prefetch_distance = Min((io_concurrency) + 10, MAX_IO_CONCURRENCY);

                          
  xid_horizon_prefetch_buffer(rel, &prefetch_state, prefetch_distance);
#endif

                                                      
  for (int i = 0; i < nitems; i++)
  {
    ItemPointer htid = &tids[i];
    OffsetNumber offnum;

       
                                                                        
                                  
       
    if (blkno == InvalidBlockNumber || ItemPointerGetBlockNumber(htid) != blkno)
    {
                              
      if (BufferIsValid(buf))
      {
        LockBuffer(buf, BUFFER_LOCK_UNLOCK);
        ReleaseBuffer(buf);
      }

      blkno = ItemPointerGetBlockNumber(htid);

      buf = ReadBuffer(rel, blkno);

#ifdef USE_PREFETCH

         
                                                                       
                            
         
      xid_horizon_prefetch_buffer(rel, &prefetch_state, 1);
#endif

      LockBuffer(buf, BUFFER_LOCK_SHARE);

      page = BufferGetPage(buf);
      maxoff = PageGetMaxOffsetNumber(page);
    }

       
                                                                         
                                                                     
                                                           
       
    offnum = ItemPointerGetOffsetNumber(htid);
    priorXmax = InvalidTransactionId;                              
    for (;;)
    {
      ItemId lp;
      HeapTupleHeader htup;

                              
      if (offnum < FirstOffsetNumber || offnum > maxoff)
      {
        break;
      }

      lp = PageGetItemId(page, offnum);
      if (ItemIdIsRedirected(lp))
      {
        offnum = ItemIdGetRedirect(lp);
        continue;
      }

         
                                                                     
                                                                  
                                                                  
                                                                     
                                                                     
                
         
                                                                     
                                                                      
                                                                        
                                    
         
      if (!ItemIdIsNormal(lp))
      {
        break;
      }

      htup = (HeapTupleHeader)PageGetItem(page, lp);

         
                                                         
         
      if (TransactionIdIsValid(priorXmax) && !TransactionIdEquals(HeapTupleHeaderGetXmin(htup), priorXmax))
      {
        break;
      }

      HeapTupleHeaderAdvanceLatestRemovedXid(htup, &latestRemovedXid);

         
                                                                         
                                                                        
                                                                     
                                         
         
      if (!HeapTupleHeaderIsHotUpdated(htup))
      {
        break;
      }

                                            
      Assert(ItemPointerGetBlockNumber(&htup->t_ctid) == blkno);
      offnum = ItemPointerGetOffsetNumber(&htup->t_ctid);
      priorXmax = HeapTupleHeaderGetUpdateXid(htup);
    }
  }

  if (BufferIsValid(buf))
  {
    LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    ReleaseBuffer(buf);
  }

  return latestRemovedXid;
}

   
                                                                     
                                                              
                                                              
                                               
   
XLogRecPtr
log_heap_cleanup_info(RelFileNode rnode, TransactionId latestRemovedXid)
{
  xl_heap_cleanup_info xlrec;
  XLogRecPtr recptr;

  xlrec.node = rnode;
  xlrec.latestRemovedXid = latestRemovedXid;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfHeapCleanupInfo);

  recptr = XLogInsert(RM_HEAP2_ID, XLOG_HEAP2_CLEANUP_INFO);

  return recptr;
}

   
                                                                       
                                                 
   
                                                                          
                                                                     
                    
   
                                                                          
                                                                         
                                                             
   
XLogRecPtr
log_heap_clean(Relation reln, Buffer buffer, OffsetNumber *redirected, int nredirected, OffsetNumber *nowdead, int ndead, OffsetNumber *nowunused, int nunused, TransactionId latestRemovedXid)
{
  xl_heap_clean xlrec;
  XLogRecPtr recptr;

                                                              
  Assert(RelationNeedsWAL(reln));

  xlrec.latestRemovedXid = latestRemovedXid;
  xlrec.nredirected = nredirected;
  xlrec.ndead = ndead;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfHeapClean);

  XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);

     
                                                                            
                                                                         
                                                                            
                                                                       
                                                                         
                                             
     
  if (nredirected > 0)
  {
    XLogRegisterBufData(0, (char *)redirected, nredirected * sizeof(OffsetNumber) * 2);
  }

  if (ndead > 0)
  {
    XLogRegisterBufData(0, (char *)nowdead, ndead * sizeof(OffsetNumber));
  }

  if (nunused > 0)
  {
    XLogRegisterBufData(0, (char *)nowunused, nunused * sizeof(OffsetNumber));
  }

  recptr = XLogInsert(RM_HEAP2_ID, XLOG_HEAP2_CLEAN);

  return recptr;
}

   
                                                                             
                                            
   
XLogRecPtr
log_heap_freeze(Relation reln, Buffer buffer, TransactionId cutoff_xid, xl_heap_freeze_tuple *tuples, int ntuples)
{
  xl_heap_freeze_page xlrec;
  XLogRecPtr recptr;

                                                              
  Assert(RelationNeedsWAL(reln));
                                              
  Assert(ntuples > 0);

  xlrec.cutoff_xid = cutoff_xid;
  xlrec.ntuples = ntuples;

  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfHeapFreezePage);

     
                                                                           
                                                                           
                        
     
  XLogRegisterBuffer(0, buffer, REGBUF_STANDARD);
  XLogRegisterBufData(0, (char *)tuples, ntuples * sizeof(xl_heap_freeze_tuple));

  recptr = XLogInsert(RM_HEAP2_ID, XLOG_HEAP2_FREEZE_PAGE);

  return recptr;
}

   
                                                                          
                                                                        
                                                                               
                
   
                                                                   
                              
   
XLogRecPtr
log_heap_visible(RelFileNode rnode, Buffer heap_buffer, Buffer vm_buffer, TransactionId cutoff_xid, uint8 vmflags)
{
  xl_heap_visible xlrec;
  XLogRecPtr recptr;
  uint8 flags;

  Assert(BufferIsValid(heap_buffer));
  Assert(BufferIsValid(vm_buffer));

  xlrec.cutoff_xid = cutoff_xid;
  xlrec.flags = vmflags;
  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfHeapVisible);

  XLogRegisterBuffer(0, vm_buffer, 0);

  flags = REGBUF_STANDARD;
  if (!XLogHintBitIsNeeded())
  {
    flags |= REGBUF_NO_IMAGE;
  }
  XLogRegisterBuffer(1, heap_buffer, flags);

  recptr = XLogInsert(RM_HEAP2_ID, XLOG_HEAP2_VISIBLE);

  return recptr;
}

   
                                                                        
                                                      
   
static XLogRecPtr
log_heap_update(Relation reln, Buffer oldbuf, Buffer newbuf, HeapTuple oldtup, HeapTuple newtup, HeapTuple old_key_tuple, bool all_visible_cleared, bool new_all_visible_cleared)
{
  xl_heap_update xlrec;
  xl_heap_header xlhdr;
  xl_heap_header xlhdr_idx;
  uint8 info;
  uint16 prefix_suffix[2];
  uint16 prefixlen = 0, suffixlen = 0;
  XLogRecPtr recptr;
  Page page = BufferGetPage(newbuf);
  bool need_tuple_data = RelationIsLogicallyLogged(reln);
  bool init;
  int bufflags;

                                                              
  Assert(RelationNeedsWAL(reln));

  XLogBeginInsert();

  if (HeapTupleIsHeapOnly(newtup))
  {
    info = XLOG_HEAP_HOT_UPDATE;
  }
  else
  {
    info = XLOG_HEAP_UPDATE;
  }

     
                                                                            
                                                                            
                                                                            
                                                                     
                                                                      
     
                                                                            
                                                                           
                                                                          
                                                                           
                                                                        
                                                                          
                                                                
     
                                                                        
                                                                       
                                                                             
                                                            
     
  if (oldbuf == newbuf && !need_tuple_data && !XLogCheckBufferNeedsBackup(newbuf))
  {
    char *oldp = (char *)oldtup->t_data + oldtup->t_data->t_hoff;
    char *newp = (char *)newtup->t_data + newtup->t_data->t_hoff;
    int oldlen = oldtup->t_len - oldtup->t_data->t_hoff;
    int newlen = newtup->t_len - newtup->t_data->t_hoff;

                                                           
    for (prefixlen = 0; prefixlen < Min(oldlen, newlen); prefixlen++)
    {
      if (newp[prefixlen] != oldp[prefixlen])
      {
        break;
      }
    }

       
                                                                          
                                             
       
    if (prefixlen < 3)
    {
      prefixlen = 0;
    }

                         
    for (suffixlen = 0; suffixlen < Min(oldlen, newlen) - prefixlen; suffixlen++)
    {
      if (newp[newlen - suffixlen - 1] != oldp[oldlen - suffixlen - 1])
      {
        break;
      }
    }
    if (suffixlen < 3)
    {
      suffixlen = 0;
    }
  }

                                   
  xlrec.flags = 0;
  if (all_visible_cleared)
  {
    xlrec.flags |= XLH_UPDATE_OLD_ALL_VISIBLE_CLEARED;
  }
  if (new_all_visible_cleared)
  {
    xlrec.flags |= XLH_UPDATE_NEW_ALL_VISIBLE_CLEARED;
  }
  if (prefixlen > 0)
  {
    xlrec.flags |= XLH_UPDATE_PREFIX_FROM_OLD;
  }
  if (suffixlen > 0)
  {
    xlrec.flags |= XLH_UPDATE_SUFFIX_FROM_OLD;
  }
  if (need_tuple_data)
  {
    xlrec.flags |= XLH_UPDATE_CONTAINS_NEW_TUPLE;
    if (old_key_tuple)
    {
      if (reln->rd_rel->relreplident == REPLICA_IDENTITY_FULL)
      {
        xlrec.flags |= XLH_UPDATE_CONTAINS_OLD_TUPLE;
      }
      else
      {
        xlrec.flags |= XLH_UPDATE_CONTAINS_OLD_KEY;
      }
    }
  }

                                                             
  if (ItemPointerGetOffsetNumber(&(newtup->t_self)) == FirstOffsetNumber && PageGetMaxOffsetNumber(page) == FirstOffsetNumber)
  {
    info |= XLOG_HEAP_INIT_PAGE;
    init = true;
  }
  else
  {
    init = false;
  }

                                         
  xlrec.old_offnum = ItemPointerGetOffsetNumber(&oldtup->t_self);
  xlrec.old_xmax = HeapTupleHeaderGetRawXmax(oldtup->t_data);
  xlrec.old_infobits_set = compute_infobits(oldtup->t_data->t_infomask, oldtup->t_data->t_infomask2);

                                         
  xlrec.new_offnum = ItemPointerGetOffsetNumber(&newtup->t_self);
  xlrec.new_xmax = HeapTupleHeaderGetRawXmax(newtup->t_data);

  bufflags = REGBUF_STANDARD;
  if (init)
  {
    bufflags |= REGBUF_WILL_INIT;
  }
  if (need_tuple_data)
  {
    bufflags |= REGBUF_KEEP_DATA;
  }

  XLogRegisterBuffer(0, newbuf, bufflags);
  if (oldbuf != newbuf)
  {
    XLogRegisterBuffer(1, oldbuf, REGBUF_STANDARD);
  }

  XLogRegisterData((char *)&xlrec, SizeOfHeapUpdate);

     
                                         
     
  if (prefixlen > 0 || suffixlen > 0)
  {
    if (prefixlen > 0 && suffixlen > 0)
    {
      prefix_suffix[0] = prefixlen;
      prefix_suffix[1] = suffixlen;
      XLogRegisterBufData(0, (char *)&prefix_suffix, sizeof(uint16) * 2);
    }
    else if (prefixlen > 0)
    {
      XLogRegisterBufData(0, (char *)&prefixlen, sizeof(uint16));
    }
    else
    {
      XLogRegisterBufData(0, (char *)&suffixlen, sizeof(uint16));
    }
  }

  xlhdr.t_infomask2 = newtup->t_data->t_infomask2;
  xlhdr.t_infomask = newtup->t_data->t_infomask;
  xlhdr.t_hoff = newtup->t_data->t_hoff;
  Assert(SizeofHeapTupleHeader + prefixlen + suffixlen <= newtup->t_len);

     
                                                         
     
                                                             
     
  XLogRegisterBufData(0, (char *)&xlhdr, SizeOfHeapHeader);
  if (prefixlen == 0)
  {
    XLogRegisterBufData(0, ((char *)newtup->t_data) + SizeofHeapTupleHeader, newtup->t_len - SizeofHeapTupleHeader - suffixlen);
  }
  else
  {
       
                                                                         
                                   
       
                                    
    if (newtup->t_data->t_hoff - SizeofHeapTupleHeader > 0)
    {
      XLogRegisterBufData(0, ((char *)newtup->t_data) + SizeofHeapTupleHeader, newtup->t_data->t_hoff - SizeofHeapTupleHeader);
    }

                                  
    XLogRegisterBufData(0, ((char *)newtup->t_data) + newtup->t_data->t_hoff + prefixlen, newtup->t_len - newtup->t_data->t_hoff - prefixlen - suffixlen);
  }

                                       
  if (need_tuple_data && old_key_tuple)
  {
                                                              
    xlhdr_idx.t_infomask2 = old_key_tuple->t_data->t_infomask2;
    xlhdr_idx.t_infomask = old_key_tuple->t_data->t_infomask;
    xlhdr_idx.t_hoff = old_key_tuple->t_data->t_hoff;

    XLogRegisterData((char *)&xlhdr_idx, SizeOfHeapHeader);

                                                             
    XLogRegisterData((char *)old_key_tuple->t_data + SizeofHeapTupleHeader, old_key_tuple->t_len - SizeofHeapTupleHeader);
  }

                                                                 
  XLogSetRecordFlags(XLOG_INCLUDE_ORIGIN);

  recptr = XLogInsert(RM_HEAP_ID, info);

  return recptr;
}

   
                                                      
   
                                                                             
           
   
static XLogRecPtr
log_heap_new_cid(Relation relation, HeapTuple tup)
{
  xl_heap_new_cid xlrec;

  XLogRecPtr recptr;
  HeapTupleHeader hdr = tup->t_data;

  Assert(ItemPointerIsValid(&tup->t_self));
  Assert(tup->t_tableOid != InvalidOid);

  xlrec.top_xid = GetTopTransactionId();
  xlrec.target_node = relation->rd_node;
  xlrec.target_tid = tup->t_self;

     
                                                                             
                                  
     
  if (hdr->t_infomask & HEAP_COMBOCID)
  {
    Assert(!(hdr->t_infomask & HEAP_XMAX_INVALID));
    Assert(!HeapTupleHeaderXminInvalid(hdr));
    xlrec.cmin = HeapTupleHeaderGetCmin(hdr);
    xlrec.cmax = HeapTupleHeaderGetCmax(hdr);
    xlrec.combocid = HeapTupleHeaderGetRawCommandId(hdr);
  }
                                                               
  else
  {
       
                       
       
                                                                  
                                                                        
                                                                     
                 
       
    if (hdr->t_infomask & HEAP_XMAX_INVALID || HEAP_XMAX_IS_LOCKED_ONLY(hdr->t_infomask))
    {
      xlrec.cmin = HeapTupleHeaderGetRawCommandId(hdr);
      xlrec.cmax = InvalidCommandId;
    }
                                                       
    else
    {
      xlrec.cmin = InvalidCommandId;
      xlrec.cmax = HeapTupleHeaderGetRawCommandId(hdr);
    }
    xlrec.combocid = InvalidCommandId;
  }

     
                                                                       
                                                                       
                                                                
     
  XLogBeginInsert();
  XLogRegisterData((char *)&xlrec, SizeOfHeapNewCid);

                                                

  recptr = XLogInsert(RM_HEAP2_ID, XLOG_HEAP2_NEW_CID);

  return recptr;
}

   
                                                                                
                                        
   
                                                                                
                
   
                                                                               
                                                                      
   
                                                                             
                                      
   
static HeapTuple
ExtractReplicaIdentity(Relation relation, HeapTuple tp, bool key_required, bool *copy)
{
  TupleDesc desc = RelationGetDescr(relation);
  char replident = relation->rd_rel->relreplident;
  Bitmapset *idattrs;
  HeapTuple key_tuple;
  bool nulls[MaxHeapAttributeNumber];
  Datum values[MaxHeapAttributeNumber];

  *copy = false;

  if (!RelationIsLogicallyLogged(relation))
  {
    return NULL;
  }

  if (replident == REPLICA_IDENTITY_NOTHING)
  {
    return NULL;
  }

  if (replident == REPLICA_IDENTITY_FULL)
  {
       
                                                                     
                                                         
       
    if (HeapTupleHasExternal(tp))
    {
      *copy = true;
      tp = toast_flatten_tuple(tp, desc);
    }
    return tp;
  }

                                                                            
  if (!key_required)
  {
    return NULL;
  }

                                             
  idattrs = RelationGetIndexAttrBitmap(relation, INDEX_ATTR_BITMAP_IDENTITY_KEY);

     
                                                                             
                                                                            
                                                                     
                                                                          
     
  if (bms_is_empty(idattrs))
  {
    return NULL;
  }

     
                                                                         
                                                                       
                                   
     
  heap_deform_tuple(tp, desc, values, nulls);

  for (int i = 0; i < desc->natts; i++)
  {
    if (bms_is_member(i + 1 - FirstLowInvalidHeapAttributeNumber, idattrs))
    {
      Assert(!nulls[i]);
    }
    else
    {
      nulls[i] = true;
    }
  }

  key_tuple = heap_form_tuple(desc, values, nulls);
  *copy = true;

  bms_free(idattrs);

     
                                                                          
                                                                          
                                                                      
                                                                           
                                                              
     
  if (HeapTupleHasExternal(key_tuple))
  {
    HeapTuple oldtup = key_tuple;

    key_tuple = toast_flatten_tuple(oldtup, desc);
    heap_freetuple(oldtup);
  }

  return key_tuple;
}

   
                        
   
static void
heap_xlog_cleanup_info(XLogReaderState *record)
{
  xl_heap_cleanup_info *xlrec = (xl_heap_cleanup_info *)XLogRecGetData(record);

  if (InHotStandby)
  {
    ResolveRecoveryConflictWithSnapshot(xlrec->latestRemovedXid, xlrec->node);
  }

     
                                                                            
                                                                            
                                                      
     

                                                          
  Assert(!XLogRecHasAnyBlockRefs(record));
}

   
                                        
   
static void
heap_xlog_clean(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_clean *xlrec = (xl_heap_clean *)XLogRecGetData(record);
  Buffer buffer;
  RelFileNode rnode;
  BlockNumber blkno;
  XLogRedoAction action;

  XLogRecGetBlockTag(record, 0, &rnode, NULL, &blkno);

     
                                                                            
                                                                        
     
                                                                             
                                                                           
                                                            
     
  if (InHotStandby && TransactionIdIsValid(xlrec->latestRemovedXid))
  {
    ResolveRecoveryConflictWithSnapshot(xlrec->latestRemovedXid, rnode);
  }

     
                                                                         
                 
     
  action = XLogReadBufferForRedoExtended(record, 0, RBM_NORMAL, true, &buffer);
  if (action == BLK_NEEDS_REDO)
  {
    Page page = (Page)BufferGetPage(buffer);
    OffsetNumber *end;
    OffsetNumber *redirected;
    OffsetNumber *nowdead;
    OffsetNumber *nowunused;
    int nredirected;
    int ndead;
    int nunused;
    Size datalen;

    redirected = (OffsetNumber *)XLogRecGetBlockData(record, 0, &datalen);

    nredirected = xlrec->nredirected;
    ndead = xlrec->ndead;
    end = (OffsetNumber *)((char *)redirected + datalen);
    nowdead = redirected + (nredirected * 2);
    nowunused = nowdead + ndead;
    nunused = (end - nowunused);
    Assert(nunused >= 0);

                                                                           
    heap_page_prune_execute(buffer, redirected, nredirected, nowdead, ndead, nowunused, nunused);

       
                                                                         
                                                                    
       

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }

  if (BufferIsValid(buffer))
  {
    Size freespace = PageGetHeapFreeSpace(BufferGetPage(buffer));

    UnlockReleaseBuffer(buffer);

       
                                                                         
                                                                       
                                                                          
                                 
       
                                                                        
                                           
       
    XLogRecordPageWithFreeSpace(rnode, blkno, freespace);
  }
}

   
                                     
   
                                                                             
                                                                       
                                                                          
                                                                 
   
static void
heap_xlog_visible(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_visible *xlrec = (xl_heap_visible *)XLogRecGetData(record);
  Buffer vmbuffer = InvalidBuffer;
  Buffer buffer;
  Page page;
  RelFileNode rnode;
  BlockNumber blkno;
  XLogRedoAction action;

  XLogRecGetBlockTag(record, 1, &rnode, NULL, &blkno);

     
                                                                         
                                                                        
                                                                             
     
                                                                            
                                                                           
                                                   
     
  if (InHotStandby)
  {
    ResolveRecoveryConflictWithSnapshot(xlrec->cutoff_xid, rnode);
  }

     
                                                                             
                                                                             
                                             
     
  action = XLogReadBufferForRedo(record, 1, &buffer);
  if (action == BLK_NEEDS_REDO)
  {
       
                                                                          
                                                                       
                                                                      
                                                                      
                   
       
                                                                           
                                                                           
                                                                          
                                                                        
                                                                  
       
    page = BufferGetPage(buffer);

    PageSetAllVisible(page);

    if (XLogHintBitIsNeeded())
    {
      PageSetLSN(page, lsn);
    }

    MarkBufferDirty(buffer);
  }
  else if (action == BLK_RESTORED)
  {
       
                                                                       
                                                                   
                               
       
  }

  if (BufferIsValid(buffer))
  {
    Size space = PageGetFreeSpace(BufferGetPage(buffer));

    UnlockReleaseBuffer(buffer);

       
                                                                      
                                                                           
                                                                    
                                                                           
                                                                         
                                                                        
                                                                        
                                                                          
              
       
                                                                         
                                              
       
                                                                        
                                           
       
    if (xlrec->flags & VISIBILITYMAP_VALID_BITS)
    {
      XLogRecordPageWithFreeSpace(rnode, blkno, space);
    }
  }

     
                                                                            
                                                                          
                                                                         
                                                         
     
  if (XLogReadBufferForRedoExtended(record, 0, RBM_ZERO_ON_ERROR, false, &vmbuffer) == BLK_NEEDS_REDO)
  {
    Page vmpage = BufferGetPage(vmbuffer);
    Relation reln;

                                                     
    if (PageIsNew(vmpage))
    {
      PageInit(vmpage, BLCKSZ, 0);
    }

       
                                                            
                                                     
       
    LockBuffer(vmbuffer, BUFFER_LOCK_UNLOCK);

    reln = CreateFakeRelcacheEntry(rnode);
    visibilitymap_pin(reln, blkno, &vmbuffer);

       
                                                                  
       
                                                                         
                                                                    
                                                                           
                                                                       
                                                                           
                                                                       
                                                           
       
    if (lsn > PageGetLSN(vmpage))
    {
      visibilitymap_set(reln, blkno, InvalidBuffer, lsn, vmbuffer, xlrec->cutoff_xid, xlrec->flags);
    }

    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }
  else if (BufferIsValid(vmbuffer))
  {
    UnlockReleaseBuffer(vmbuffer);
  }
}

   
                                         
   
static void
heap_xlog_freeze_page(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_freeze_page *xlrec = (xl_heap_freeze_page *)XLogRecGetData(record);
  TransactionId cutoff_xid = xlrec->cutoff_xid;
  Buffer buffer;
  int ntup;

     
                                                                             
                                          
     
  if (InHotStandby)
  {
    RelFileNode rnode;
    TransactionId latestRemovedXid = cutoff_xid;

    TransactionIdRetreat(latestRemovedXid);

    XLogRecGetBlockTag(record, 0, &rnode, NULL, NULL);
    ResolveRecoveryConflictWithSnapshot(latestRemovedXid, rnode);
  }

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    Page page = BufferGetPage(buffer);
    xl_heap_freeze_tuple *tuples;

    tuples = (xl_heap_freeze_tuple *)XLogRecGetBlockData(record, 0, NULL);

                                                       
    for (ntup = 0; ntup < xlrec->ntuples; ntup++)
    {
      xl_heap_freeze_tuple *xlrec_tp;
      ItemId lp;
      HeapTupleHeader tuple;

      xlrec_tp = &tuples[ntup];
      lp = PageGetItemId(page, xlrec_tp->offset);                            
      tuple = (HeapTupleHeader)PageGetItem(page, lp);

      heap_execute_freeze_tuple(tuple, xlrec_tp);
    }

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

   
                                                                              
                                                                     
   
                                              
   
static void
fix_infomask_from_infobits(uint8 infobits, uint16 *infomask, uint16 *infomask2)
{
  *infomask &= ~(HEAP_XMAX_IS_MULTI | HEAP_XMAX_LOCK_ONLY | HEAP_XMAX_KEYSHR_LOCK | HEAP_XMAX_EXCL_LOCK);
  *infomask2 &= ~HEAP_KEYS_UPDATED;

  if (infobits & XLHL_XMAX_IS_MULTI)
  {
    *infomask |= HEAP_XMAX_IS_MULTI;
  }
  if (infobits & XLHL_XMAX_LOCK_ONLY)
  {
    *infomask |= HEAP_XMAX_LOCK_ONLY;
  }
  if (infobits & XLHL_XMAX_EXCL_LOCK)
  {
    *infomask |= HEAP_XMAX_EXCL_LOCK;
  }
                                                     
  if (infobits & XLHL_XMAX_KEYSHR_LOCK)
  {
    *infomask |= HEAP_XMAX_KEYSHR_LOCK;
  }

  if (infobits & XLHL_KEYS_UPDATED)
  {
    *infomask2 |= HEAP_KEYS_UPDATED;
  }
}

static void
heap_xlog_delete(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_delete *xlrec = (xl_heap_delete *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  ItemId lp = NULL;
  HeapTupleHeader htup;
  BlockNumber blkno;
  RelFileNode target_node;
  ItemPointerData target_tid;

  XLogRecGetBlockTag(record, 0, &target_node, NULL, &blkno);
  ItemPointerSetBlockNumber(&target_tid, blkno);
  ItemPointerSetOffsetNumber(&target_tid, xlrec->offnum);

     
                                                                      
                         
     
  if (xlrec->flags & XLH_DELETE_ALL_VISIBLE_CLEARED)
  {
    Relation reln = CreateFakeRelcacheEntry(target_node);
    Buffer vmbuffer = InvalidBuffer;

    visibilitymap_pin(reln, blkno, &vmbuffer);
    visibilitymap_clear(reln, blkno, vmbuffer, VISIBILITYMAP_VALID_BITS);
    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    page = BufferGetPage(buffer);

    if (PageGetMaxOffsetNumber(page) >= xlrec->offnum)
    {
      lp = PageGetItemId(page, xlrec->offnum);
    }

    if (PageGetMaxOffsetNumber(page) < xlrec->offnum || !ItemIdIsNormal(lp))
    {
      elog(PANIC, "invalid lp");
    }

    htup = (HeapTupleHeader)PageGetItem(page, lp);

    htup->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
    htup->t_infomask2 &= ~HEAP_KEYS_UPDATED;
    HeapTupleHeaderClearHotUpdated(htup);
    fix_infomask_from_infobits(xlrec->infobits_set, &htup->t_infomask, &htup->t_infomask2);
    if (!(xlrec->flags & XLH_DELETE_IS_SUPER))
    {
      HeapTupleHeaderSetXmax(htup, xlrec->xmax);
    }
    else
    {
      HeapTupleHeaderSetXmin(htup, InvalidTransactionId);
    }
    HeapTupleHeaderSetCmax(htup, FirstCommandId, false);

                                                  
    PageSetPrunable(page, XLogRecGetXid(record));

    if (xlrec->flags & XLH_DELETE_ALL_VISIBLE_CLEARED)
    {
      PageClearAllVisible(page);
    }

                                           
    if (xlrec->flags & XLH_DELETE_IS_PARTITION_MOVE)
    {
      HeapTupleHeaderSetMovedPartitions(htup);
    }
    else
    {
      htup->t_ctid = target_tid;
    }
    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

static void
heap_xlog_insert(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_insert *xlrec = (xl_heap_insert *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  union
  {
    HeapTupleHeaderData hdr;
    char data[MaxHeapTupleSize];
  } tbuf;
  HeapTupleHeader htup;
  xl_heap_header xlhdr;
  uint32 newlen;
  Size freespace = 0;
  RelFileNode target_node;
  BlockNumber blkno;
  ItemPointerData target_tid;
  XLogRedoAction action;

  XLogRecGetBlockTag(record, 0, &target_node, NULL, &blkno);
  ItemPointerSetBlockNumber(&target_tid, blkno);
  ItemPointerSetOffsetNumber(&target_tid, xlrec->offnum);

     
                                                                      
                         
     
  if (xlrec->flags & XLH_INSERT_ALL_VISIBLE_CLEARED)
  {
    Relation reln = CreateFakeRelcacheEntry(target_node);
    Buffer vmbuffer = InvalidBuffer;

    visibilitymap_pin(reln, blkno, &vmbuffer);
    visibilitymap_clear(reln, blkno, vmbuffer, VISIBILITYMAP_VALID_BITS);
    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }

     
                                                                            
                        
     
  if (XLogRecGetInfo(record) & XLOG_HEAP_INIT_PAGE)
  {
    buffer = XLogInitBufferForRedo(record, 0);
    page = BufferGetPage(buffer);
    PageInit(page, BufferGetPageSize(buffer), 0);
    action = BLK_NEEDS_REDO;
  }
  else
  {
    action = XLogReadBufferForRedo(record, 0, &buffer);
  }
  if (action == BLK_NEEDS_REDO)
  {
    Size datalen;
    char *data;

    page = BufferGetPage(buffer);

    if (PageGetMaxOffsetNumber(page) + 1 < xlrec->offnum)
    {
      elog(PANIC, "invalid max offset number");
    }

    data = XLogRecGetBlockData(record, 0, &datalen);

    newlen = datalen - SizeOfHeapHeader;
    Assert(datalen > SizeOfHeapHeader && newlen <= MaxHeapTupleSize);
    memcpy((char *)&xlhdr, data, SizeOfHeapHeader);
    data += SizeOfHeapHeader;

    htup = &tbuf.hdr;
    MemSet((char *)htup, 0, SizeofHeapTupleHeader);
                                                           
    memcpy((char *)htup + SizeofHeapTupleHeader, data, newlen);
    newlen += SizeofHeapTupleHeader;
    htup->t_infomask2 = xlhdr.t_infomask2;
    htup->t_infomask = xlhdr.t_infomask;
    htup->t_hoff = xlhdr.t_hoff;
    HeapTupleHeaderSetXmin(htup, XLogRecGetXid(record));
    HeapTupleHeaderSetCmin(htup, FirstCommandId);
    htup->t_ctid = target_tid;

    if (PageAddItem(page, (Item)htup, newlen, xlrec->offnum, true, true) == InvalidOffsetNumber)
    {
      elog(PANIC, "failed to add tuple");
    }

    freespace = PageGetHeapFreeSpace(page);                                 

    PageSetLSN(page, lsn);

    if (xlrec->flags & XLH_INSERT_ALL_VISIBLE_CLEARED)
    {
      PageClearAllVisible(page);
    }

    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

     
                                                                       
                                                                             
                                                                     
     
                                                                          
                                                                        
                              
     
  if (action == BLK_NEEDS_REDO && freespace < BLCKSZ / 5)
  {
    XLogRecordPageWithFreeSpace(target_node, blkno, freespace);
  }
}

   
                                     
   
static void
heap_xlog_multi_insert(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_multi_insert *xlrec;
  RelFileNode rnode;
  BlockNumber blkno;
  Buffer buffer;
  Page page;
  union
  {
    HeapTupleHeaderData hdr;
    char data[MaxHeapTupleSize];
  } tbuf;
  HeapTupleHeader htup;
  uint32 newlen;
  Size freespace = 0;
  int i;
  bool isinit = (XLogRecGetInfo(record) & XLOG_HEAP_INIT_PAGE) != 0;
  XLogRedoAction action;

     
                                                                         
               
     
  xlrec = (xl_heap_multi_insert *)XLogRecGetData(record);

  XLogRecGetBlockTag(record, 0, &rnode, NULL, &blkno);

     
                                                                      
                         
     
  if (xlrec->flags & XLH_INSERT_ALL_VISIBLE_CLEARED)
  {
    Relation reln = CreateFakeRelcacheEntry(rnode);
    Buffer vmbuffer = InvalidBuffer;

    visibilitymap_pin(reln, blkno, &vmbuffer);
    visibilitymap_clear(reln, blkno, vmbuffer, VISIBILITYMAP_VALID_BITS);
    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }

  if (isinit)
  {
    buffer = XLogInitBufferForRedo(record, 0);
    page = BufferGetPage(buffer);
    PageInit(page, BufferGetPageSize(buffer), 0);
    action = BLK_NEEDS_REDO;
  }
  else
  {
    action = XLogReadBufferForRedo(record, 0, &buffer);
  }
  if (action == BLK_NEEDS_REDO)
  {
    char *tupdata;
    char *endptr;
    Size len;

                                         
    tupdata = XLogRecGetBlockData(record, 0, &len);
    endptr = tupdata + len;

    page = (Page)BufferGetPage(buffer);

    for (i = 0; i < xlrec->ntuples; i++)
    {
      OffsetNumber offnum;
      xl_multi_insert_tuple *xlhdr;

         
                                                                    
                                                                     
                                                                    
         
      if (isinit)
      {
        offnum = FirstOffsetNumber + i;
      }
      else
      {
        offnum = xlrec->offsets[i];
      }
      if (PageGetMaxOffsetNumber(page) + 1 < offnum)
      {
        elog(PANIC, "invalid max offset number");
      }

      xlhdr = (xl_multi_insert_tuple *)SHORTALIGN(tupdata);
      tupdata = ((char *)xlhdr) + SizeOfMultiInsertTuple;

      newlen = xlhdr->datalen;
      Assert(newlen <= MaxHeapTupleSize);
      htup = &tbuf.hdr;
      MemSet((char *)htup, 0, SizeofHeapTupleHeader);
                                                             
      memcpy((char *)htup + SizeofHeapTupleHeader, (char *)tupdata, newlen);
      tupdata += newlen;

      newlen += SizeofHeapTupleHeader;
      htup->t_infomask2 = xlhdr->t_infomask2;
      htup->t_infomask = xlhdr->t_infomask;
      htup->t_hoff = xlhdr->t_hoff;
      HeapTupleHeaderSetXmin(htup, XLogRecGetXid(record));
      HeapTupleHeaderSetCmin(htup, FirstCommandId);
      ItemPointerSetBlockNumber(&htup->t_ctid, blkno);
      ItemPointerSetOffsetNumber(&htup->t_ctid, offnum);

      offnum = PageAddItem(page, (Item)htup, newlen, offnum, true, true);
      if (offnum == InvalidOffsetNumber)
      {
        elog(PANIC, "failed to add tuple");
      }
    }
    if (tupdata != endptr)
    {
      elog(PANIC, "total tuple length mismatch");
    }

    freespace = PageGetHeapFreeSpace(page);                                 

    PageSetLSN(page, lsn);

    if (xlrec->flags & XLH_INSERT_ALL_VISIBLE_CLEARED)
    {
      PageClearAllVisible(page);
    }

    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }

     
                                                                       
                                                                             
                                                                     
     
                                                                          
                                                                        
                              
     
  if (action == BLK_NEEDS_REDO && freespace < BLCKSZ / 5)
  {
    XLogRecordPageWithFreeSpace(rnode, blkno, freespace);
  }
}

   
                                 
   
static void
heap_xlog_update(XLogReaderState *record, bool hot_update)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_update *xlrec = (xl_heap_update *)XLogRecGetData(record);
  RelFileNode rnode;
  BlockNumber oldblk;
  BlockNumber newblk;
  ItemPointerData newtid;
  Buffer obuffer, nbuffer;
  Page page;
  OffsetNumber offnum;
  ItemId lp = NULL;
  HeapTupleData oldtup;
  HeapTupleHeader htup;
  uint16 prefixlen = 0, suffixlen = 0;
  char *newp;
  union
  {
    HeapTupleHeaderData hdr;
    char data[MaxHeapTupleSize];
  } tbuf;
  xl_heap_header xlhdr;
  uint32 newlen;
  Size freespace = 0;
  XLogRedoAction oldaction;
  XLogRedoAction newaction;

                                             
  oldtup.t_data = NULL;
  oldtup.t_len = 0;

  XLogRecGetBlockTag(record, 0, &rnode, NULL, &newblk);
  if (XLogRecGetBlockTag(record, 1, NULL, NULL, &oldblk))
  {
                                                 
    Assert(!hot_update);
  }
  else
  {
    oldblk = newblk;
  }

  ItemPointerSet(&newtid, newblk, xlrec->new_offnum);

     
                                                                      
                         
     
  if (xlrec->flags & XLH_UPDATE_OLD_ALL_VISIBLE_CLEARED)
  {
    Relation reln = CreateFakeRelcacheEntry(rnode);
    Buffer vmbuffer = InvalidBuffer;

    visibilitymap_pin(reln, oldblk, &vmbuffer);
    visibilitymap_clear(reln, oldblk, vmbuffer, VISIBILITYMAP_VALID_BITS);
    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }

     
                                                                   
                                                                         
                                                                           
                                                                             
                                                                             
                                                                             
                                          
     

                                   
  oldaction = XLogReadBufferForRedo(record, (oldblk == newblk) ? 0 : 1, &obuffer);
  if (oldaction == BLK_NEEDS_REDO)
  {
    page = BufferGetPage(obuffer);
    offnum = xlrec->old_offnum;
    if (PageGetMaxOffsetNumber(page) >= offnum)
    {
      lp = PageGetItemId(page, offnum);
    }

    if (PageGetMaxOffsetNumber(page) < offnum || !ItemIdIsNormal(lp))
    {
      elog(PANIC, "invalid lp");
    }

    htup = (HeapTupleHeader)PageGetItem(page, lp);

    oldtup.t_data = htup;
    oldtup.t_len = ItemIdGetLength(lp);

    htup->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
    htup->t_infomask2 &= ~HEAP_KEYS_UPDATED;
    if (hot_update)
    {
      HeapTupleHeaderSetHotUpdated(htup);
    }
    else
    {
      HeapTupleHeaderClearHotUpdated(htup);
    }
    fix_infomask_from_infobits(xlrec->old_infobits_set, &htup->t_infomask, &htup->t_infomask2);
    HeapTupleHeaderSetXmax(htup, xlrec->old_xmax);
    HeapTupleHeaderSetCmax(htup, FirstCommandId, false);
                                          
    htup->t_ctid = newtid;

                                                  
    PageSetPrunable(page, XLogRecGetXid(record));

    if (xlrec->flags & XLH_UPDATE_OLD_ALL_VISIBLE_CLEARED)
    {
      PageClearAllVisible(page);
    }

    PageSetLSN(page, lsn);
    MarkBufferDirty(obuffer);
  }

     
                                                                   
     
  if (oldblk == newblk)
  {
    nbuffer = obuffer;
    newaction = oldaction;
  }
  else if (XLogRecGetInfo(record) & XLOG_HEAP_INIT_PAGE)
  {
    nbuffer = XLogInitBufferForRedo(record, 0);
    page = (Page)BufferGetPage(nbuffer);
    PageInit(page, BufferGetPageSize(nbuffer), 0);
    newaction = BLK_NEEDS_REDO;
  }
  else
  {
    newaction = XLogReadBufferForRedo(record, 0, &nbuffer);
  }

     
                                                                      
                         
     
  if (xlrec->flags & XLH_UPDATE_NEW_ALL_VISIBLE_CLEARED)
  {
    Relation reln = CreateFakeRelcacheEntry(rnode);
    Buffer vmbuffer = InvalidBuffer;

    visibilitymap_pin(reln, newblk, &vmbuffer);
    visibilitymap_clear(reln, newblk, vmbuffer, VISIBILITYMAP_VALID_BITS);
    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }

                           
  if (newaction == BLK_NEEDS_REDO)
  {
    char *recdata;
    char *recdata_end;
    Size datalen;
    Size tuplen;

    recdata = XLogRecGetBlockData(record, 0, &datalen);
    recdata_end = recdata + datalen;

    page = BufferGetPage(nbuffer);

    offnum = xlrec->new_offnum;
    if (PageGetMaxOffsetNumber(page) + 1 < offnum)
    {
      elog(PANIC, "invalid max offset number");
    }

    if (xlrec->flags & XLH_UPDATE_PREFIX_FROM_OLD)
    {
      Assert(newblk == oldblk);
      memcpy(&prefixlen, recdata, sizeof(uint16));
      recdata += sizeof(uint16);
    }
    if (xlrec->flags & XLH_UPDATE_SUFFIX_FROM_OLD)
    {
      Assert(newblk == oldblk);
      memcpy(&suffixlen, recdata, sizeof(uint16));
      recdata += sizeof(uint16);
    }

    memcpy((char *)&xlhdr, recdata, SizeOfHeapHeader);
    recdata += SizeOfHeapHeader;

    tuplen = recdata_end - recdata;
    Assert(tuplen <= MaxHeapTupleSize);

    htup = &tbuf.hdr;
    MemSet((char *)htup, 0, SizeofHeapTupleHeader);

       
                                                                         
                                                         
       
    newp = (char *)htup + SizeofHeapTupleHeader;
    if (prefixlen > 0)
    {
      int len;

                                                           
      len = xlhdr.t_hoff - SizeofHeapTupleHeader;
      memcpy(newp, recdata, len);
      recdata += len;
      newp += len;

                                      
      memcpy(newp, (char *)oldtup.t_data + oldtup.t_data->t_hoff, prefixlen);
      newp += prefixlen;

                                               
      len = tuplen - (xlhdr.t_hoff - SizeofHeapTupleHeader);
      memcpy(newp, recdata, len);
      recdata += len;
      newp += len;
    }
    else
    {
         
                                                                        
            
         
      memcpy(newp, recdata, tuplen);
      recdata += tuplen;
      newp += tuplen;
    }
    Assert(recdata == recdata_end);

                                    
    if (suffixlen > 0)
    {
      memcpy(newp, (char *)oldtup.t_data + oldtup.t_len - suffixlen, suffixlen);
    }

    newlen = SizeofHeapTupleHeader + tuplen + prefixlen + suffixlen;
    htup->t_infomask2 = xlhdr.t_infomask2;
    htup->t_infomask = xlhdr.t_infomask;
    htup->t_hoff = xlhdr.t_hoff;

    HeapTupleHeaderSetXmin(htup, XLogRecGetXid(record));
    HeapTupleHeaderSetCmin(htup, FirstCommandId);
    HeapTupleHeaderSetXmax(htup, xlrec->new_xmax);
                                                            
    htup->t_ctid = newtid;

    offnum = PageAddItem(page, (Item)htup, newlen, offnum, true, true);
    if (offnum == InvalidOffsetNumber)
    {
      elog(PANIC, "failed to add tuple");
    }

    if (xlrec->flags & XLH_UPDATE_NEW_ALL_VISIBLE_CLEARED)
    {
      PageClearAllVisible(page);
    }

    freespace = PageGetHeapFreeSpace(page);                                 

    PageSetLSN(page, lsn);
    MarkBufferDirty(nbuffer);
  }

  if (BufferIsValid(nbuffer) && nbuffer != obuffer)
  {
    UnlockReleaseBuffer(nbuffer);
  }
  if (BufferIsValid(obuffer))
  {
    UnlockReleaseBuffer(obuffer);
  }

     
                                                                           
                                                                             
                                                                     
     
                                                                       
                                                                          
                                                                            
                                                                           
                          
     
                                                                          
                                                                        
                              
     
  if (newaction == BLK_NEEDS_REDO && !hot_update && freespace < BLCKSZ / 5)
  {
    XLogRecordPageWithFreeSpace(rnode, newblk, freespace);
  }
}

static void
heap_xlog_confirm(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_confirm *xlrec = (xl_heap_confirm *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  ItemId lp = NULL;
  HeapTupleHeader htup;

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    page = BufferGetPage(buffer);

    offnum = xlrec->offnum;
    if (PageGetMaxOffsetNumber(page) >= offnum)
    {
      lp = PageGetItemId(page, offnum);
    }

    if (PageGetMaxOffsetNumber(page) < offnum || !ItemIdIsNormal(lp))
    {
      elog(PANIC, "invalid lp");
    }

    htup = (HeapTupleHeader)PageGetItem(page, lp);

       
                                          
       
    ItemPointerSet(&htup->t_ctid, BufferGetBlockNumber(buffer), offnum);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

static void
heap_xlog_lock(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_lock *xlrec = (xl_heap_lock *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  ItemId lp = NULL;
  HeapTupleHeader htup;

     
                                                                      
                         
     
  if (xlrec->flags & XLH_LOCK_ALL_FROZEN_CLEARED)
  {
    RelFileNode rnode;
    Buffer vmbuffer = InvalidBuffer;
    BlockNumber block;
    Relation reln;

    XLogRecGetBlockTag(record, 0, &rnode, NULL, &block);
    reln = CreateFakeRelcacheEntry(rnode);

    visibilitymap_pin(reln, block, &vmbuffer);
    visibilitymap_clear(reln, block, vmbuffer, VISIBILITYMAP_ALL_FROZEN);

    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    page = (Page)BufferGetPage(buffer);

    offnum = xlrec->offnum;
    if (PageGetMaxOffsetNumber(page) >= offnum)
    {
      lp = PageGetItemId(page, offnum);
    }

    if (PageGetMaxOffsetNumber(page) < offnum || !ItemIdIsNormal(lp))
    {
      elog(PANIC, "invalid lp");
    }

    htup = (HeapTupleHeader)PageGetItem(page, lp);

    htup->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
    htup->t_infomask2 &= ~HEAP_KEYS_UPDATED;
    fix_infomask_from_infobits(xlrec->infobits_set, &htup->t_infomask, &htup->t_infomask2);

       
                                                                           
                          
       
    if (HEAP_XMAX_IS_LOCKED_ONLY(htup->t_infomask))
    {
      HeapTupleHeaderClearHotUpdated(htup);
                                                              
      ItemPointerSet(&htup->t_ctid, BufferGetBlockNumber(buffer), offnum);
    }
    HeapTupleHeaderSetXmax(htup, xlrec->locking_xid);
    HeapTupleHeaderSetCmax(htup, FirstCommandId, false);
    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

static void
heap_xlog_lock_updated(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_lock_updated *xlrec;
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  ItemId lp = NULL;
  HeapTupleHeader htup;

  xlrec = (xl_heap_lock_updated *)XLogRecGetData(record);

     
                                                                      
                         
     
  if (xlrec->flags & XLH_LOCK_ALL_FROZEN_CLEARED)
  {
    RelFileNode rnode;
    Buffer vmbuffer = InvalidBuffer;
    BlockNumber block;
    Relation reln;

    XLogRecGetBlockTag(record, 0, &rnode, NULL, &block);
    reln = CreateFakeRelcacheEntry(rnode);

    visibilitymap_pin(reln, block, &vmbuffer);
    visibilitymap_clear(reln, block, vmbuffer, VISIBILITYMAP_ALL_FROZEN);

    ReleaseBuffer(vmbuffer);
    FreeFakeRelcacheEntry(reln);
  }

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    page = BufferGetPage(buffer);

    offnum = xlrec->offnum;
    if (PageGetMaxOffsetNumber(page) >= offnum)
    {
      lp = PageGetItemId(page, offnum);
    }

    if (PageGetMaxOffsetNumber(page) < offnum || !ItemIdIsNormal(lp))
    {
      elog(PANIC, "invalid lp");
    }

    htup = (HeapTupleHeader)PageGetItem(page, lp);

    htup->t_infomask &= ~(HEAP_XMAX_BITS | HEAP_MOVED);
    htup->t_infomask2 &= ~HEAP_KEYS_UPDATED;
    fix_infomask_from_infobits(xlrec->infobits_set, &htup->t_infomask, &htup->t_infomask2);
    HeapTupleHeaderSetXmax(htup, xlrec->xmax);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

static void
heap_xlog_inplace(XLogReaderState *record)
{
  XLogRecPtr lsn = record->EndRecPtr;
  xl_heap_inplace *xlrec = (xl_heap_inplace *)XLogRecGetData(record);
  Buffer buffer;
  Page page;
  OffsetNumber offnum;
  ItemId lp = NULL;
  HeapTupleHeader htup;
  uint32 oldlen;
  Size newlen;

  if (XLogReadBufferForRedo(record, 0, &buffer) == BLK_NEEDS_REDO)
  {
    char *newtup = XLogRecGetBlockData(record, 0, &newlen);

    page = BufferGetPage(buffer);

    offnum = xlrec->offnum;
    if (PageGetMaxOffsetNumber(page) >= offnum)
    {
      lp = PageGetItemId(page, offnum);
    }

    if (PageGetMaxOffsetNumber(page) < offnum || !ItemIdIsNormal(lp))
    {
      elog(PANIC, "invalid lp");
    }

    htup = (HeapTupleHeader)PageGetItem(page, lp);

    oldlen = ItemIdGetLength(lp) - htup->t_hoff;
    if (oldlen != newlen)
    {
      elog(PANIC, "wrong tuple length");
    }

    memcpy((char *)htup + htup->t_hoff, newtup, newlen);

    PageSetLSN(page, lsn);
    MarkBufferDirty(buffer);
  }
  if (BufferIsValid(buffer))
  {
    UnlockReleaseBuffer(buffer);
  }
}

void
heap_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

     
                                                                             
                                          
     

  switch (info & XLOG_HEAP_OPMASK)
  {
  case XLOG_HEAP_INSERT:
    heap_xlog_insert(record);
    break;
  case XLOG_HEAP_DELETE:
    heap_xlog_delete(record);
    break;
  case XLOG_HEAP_UPDATE:
    heap_xlog_update(record, false);
    break;
  case XLOG_HEAP_TRUNCATE:

       
                                                                     
                                                                      
                 
       
    break;
  case XLOG_HEAP_HOT_UPDATE:
    heap_xlog_update(record, true);
    break;
  case XLOG_HEAP_CONFIRM:
    heap_xlog_confirm(record);
    break;
  case XLOG_HEAP_LOCK:
    heap_xlog_lock(record);
    break;
  case XLOG_HEAP_INPLACE:
    heap_xlog_inplace(record);
    break;
  default:
    elog(PANIC, "heap_redo: unknown op code %u", info);
  }
}

void
heap2_redo(XLogReaderState *record)
{
  uint8 info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

  switch (info & XLOG_HEAP_OPMASK)
  {
  case XLOG_HEAP2_CLEAN:
    heap_xlog_clean(record);
    break;
  case XLOG_HEAP2_FREEZE_PAGE:
    heap_xlog_freeze_page(record);
    break;
  case XLOG_HEAP2_CLEANUP_INFO:
    heap_xlog_cleanup_info(record);
    break;
  case XLOG_HEAP2_VISIBLE:
    heap_xlog_visible(record);
    break;
  case XLOG_HEAP2_MULTI_INSERT:
    heap_xlog_multi_insert(record);
    break;
  case XLOG_HEAP2_LOCK_UPDATED:
    heap_xlog_lock_updated(record);
    break;
  case XLOG_HEAP2_NEW_CID:

       
                                                                
                 
       
    break;
  case XLOG_HEAP2_REWRITE:
    heap_xlog_logical_rewrite(record);
    break;
  default:
    elog(PANIC, "heap2_redo: unknown op code %u", info);
  }
}

   
                                                                  
   
                                                                             
                                                                           
                                                                           
                                                                         
   
                                                                          
                                                                       
                                                                        
                                                                             
                     
   
void
heap_sync(Relation rel)
{
                                              
  if (!RelationNeedsWAL(rel))
  {
    return;
  }

                 
  FlushRelationBuffers(rel);
  smgrimmedsync(RelationGetSmgr(rel), MAIN_FORKNUM);

                                                    

                          
  if (OidIsValid(rel->rd_rel->reltoastrelid))
  {
    Relation toastrel;

    toastrel = table_open(rel->rd_rel->reltoastrelid, AccessShareLock);
    FlushRelationBuffers(toastrel);
    smgrimmedsync(RelationGetSmgr(toastrel), MAIN_FORKNUM);
    table_close(toastrel, AccessShareLock);
  }
}

   
                                                                
   
void
heap_mask(char *pagedata, BlockNumber blkno)
{
  Page page = (Page)pagedata;
  OffsetNumber off;

  mask_page_lsn_and_checksum(page);

  mask_page_hint_bits(page);
  mask_unused_space(page);

  for (off = 1; off <= PageGetMaxOffsetNumber(page); off++)
  {
    ItemId iid = PageGetItemId(page, off);
    char *page_item;

    page_item = (char *)(page + ItemIdGetOffset(iid));

    if (ItemIdIsNormal(iid))
    {
      HeapTupleHeader page_htup = (HeapTupleHeader)page_item;

         
                                                                
                                                                 
                       
         
      if (!HeapTupleHeaderXminFrozen(page_htup))
      {
        page_htup->t_infomask &= ~HEAP_XACT_MASK;
      }
      else
      {
                                                   
        page_htup->t_infomask &= ~HEAP_XMAX_INVALID;
        page_htup->t_infomask &= ~HEAP_XMAX_COMMITTED;
      }

         
                                                                         
                                                 
         
      page_htup->t_choice.t_heap.t_field3.t_cid = MASK_MARKER;

         
                                                                         
                                                                    
                                                                         
                                                                        
                            
         
                                                                      
                                                                  
                                                                   
                                                                   
                        
         
      if (HeapTupleHeaderIsSpeculative(page_htup))
      {
        ItemPointerSet(&page_htup->t_ctid, blkno, off);
      }

         
                                                                     
                                                                        
                                                                        
                                              
         
    }

       
                                                                        
                               
       
    if (ItemIdHasStorage(iid))
    {
      int len = ItemIdGetLength(iid);
      int padlen = MAXALIGN(len) - len;

      if (padlen > 0)
      {
        memset(page_item + len, MASK_MARKER, padlen);
      }
    }
  }
}
