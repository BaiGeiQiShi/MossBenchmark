                                                                            
   
                
                                    
   
   
                                                                                
                                                                               
                                                                                
                                         
   
                                                                  
                                                                       
                                                                              
                                                                           
                                                                               
                                                                               
                                                                  
   
                                                                             
                                                                               
                                                                              
                                                                              
   
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/heapam_xlog.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/transam.h"
#include "access/visibilitymap.h"
#include "access/xlog.h"
#include "catalog/storage.h"
#include "commands/dbcommands.h"
#include "commands/progress.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "portability/instr_time.h"
#include "postmaster/autovacuum.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/lmgr.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/timestamp.h"

   
                                                                     
   
                                                                     
                                                                        
                                        
   
#define REL_TRUNCATE_MINIMUM 1000
#define REL_TRUNCATE_FRACTION 16

   
                                                      
   
                                                                            
                                                                            
                    
   
#define VACUUM_TRUNCATE_LOCK_CHECK_INTERVAL 20         
#define VACUUM_TRUNCATE_LOCK_WAIT_INTERVAL 50          
#define VACUUM_TRUNCATE_LOCK_TIMEOUT 5000              

   
                                                                              
                                                                              
                                                                              
                                                          
   
#define VACUUM_FSM_EVERY_PAGES ((BlockNumber)(((uint64)8 * 1024 * 1024 * 1024) / BLCKSZ))

   
                                                                     
                                                                   
           
   
#define LAZY_ALLOC_TUPLES MaxHeapTuplesPerPage

   
                                                                
                                                                   
   
#define SKIP_PAGES_THRESHOLD ((BlockNumber)32)

   
                                                                          
                             
   
#define PREFETCH_SIZE ((BlockNumber)32)

typedef struct LVRelStats
{
                                                                     
  bool useindex;
                                    
  BlockNumber old_rel_pages;                                                
  BlockNumber rel_pages;                                      
  BlockNumber scanned_pages;                                        
  BlockNumber pinskipped_pages;                                            
  BlockNumber frozenskipped_pages;                                   
  BlockNumber tupcount_pages;                                         
  double old_live_tuples;                                                    
  double new_rel_tuples;                                                
  double new_live_tuples;                                                    
  double new_dead_tuples;                                                    
  BlockNumber pages_removed;
  double tuples_deleted;
  BlockNumber nonempty_pages;                                       
                                                  
                                               
  int num_dead_tuples;                               
  int max_dead_tuples;                                     
  ItemPointer dead_tuples;                               
  int num_index_scans;
  TransactionId latestRemovedXid;
  bool lock_waiter_detected;
} LVRelStats;

                                                                        
static int elevel = -1;

static TransactionId OldestXmin;
static TransactionId FreezeLimit;
static MultiXactId MultiXactCutoff;

static BufferAccessStrategy vac_strategy;

                                    
static void
lazy_scan_heap(Relation onerel, VacuumParams *params, LVRelStats *vacrelstats, Relation *Irel, int nindexes, bool aggressive);
static void
lazy_vacuum_heap(Relation onerel, LVRelStats *vacrelstats);
static bool
lazy_check_needs_freeze(Buffer buf, bool *hastup);
static void
lazy_vacuum_index(Relation indrel, IndexBulkDeleteResult **stats, LVRelStats *vacrelstats);
static void
lazy_cleanup_index(Relation indrel, IndexBulkDeleteResult *stats, LVRelStats *vacrelstats);
static int
lazy_vacuum_page(Relation onerel, BlockNumber blkno, Buffer buffer, int tupindex, LVRelStats *vacrelstats, Buffer *vmbuffer);
static bool
should_attempt_truncation(VacuumParams *params, LVRelStats *vacrelstats);
static void
lazy_truncate_heap(Relation onerel, LVRelStats *vacrelstats);
static BlockNumber
count_nondeletable_pages(Relation onerel, LVRelStats *vacrelstats);
static void
lazy_space_alloc(LVRelStats *vacrelstats, BlockNumber relblocks);
static void
lazy_record_dead_tuple(LVRelStats *vacrelstats, ItemPointer itemptr);
static bool
lazy_tid_reaped(ItemPointer itemptr, void *state);
static int
vac_cmp_itemptr(const void *left, const void *right);
static bool
heap_page_is_all_visible(Relation rel, Buffer buf, TransactionId *visibility_cutoff_xid, bool *all_frozen);

   
                                                             
   
                                                                    
                                                   
   
                                                                   
                             
   
void
heap_vacuum_rel(Relation onerel, VacuumParams *params, BufferAccessStrategy bstrategy)
{
  LVRelStats *vacrelstats;
  Relation *Irel;
  int nindexes;
  PGRUsage ru0;
  TimestampTz starttime = 0;
  long secs;
  int usecs;
  double read_rate, write_rate;
  bool aggressive;                                                   
  bool scanned_all_unfrozen;                                       
  TransactionId xidFullScanLimit;
  MultiXactId mxactFullScanLimit;
  BlockNumber new_rel_pages;
  BlockNumber new_rel_allvisible;
  double new_live_tuples;
  TransactionId new_frozen_xid;
  MultiXactId new_min_multi;

  Assert(params != NULL);
  Assert(params->index_cleanup != VACOPT_TERNARY_DEFAULT);
  Assert(params->truncate != VACOPT_TERNARY_DEFAULT);

                                                              
  Assert(TransactionIdIsNormal(onerel->rd_rel->relfrozenxid));
  Assert(MultiXactIdIsValid(onerel->rd_rel->relminmxid));

                                                               
  if (IsAutoVacuumWorkerProcess() && params->log_min_duration >= 0)
  {
    pg_rusage_init(&ru0);
    starttime = GetCurrentTimestamp();
  }

  if (params->options & VACOPT_VERBOSE)
  {
    elevel = INFO;
  }
  else
  {
    elevel = DEBUG2;
  }

  pgstat_progress_start_command(PROGRESS_COMMAND_VACUUM, RelationGetRelid(onerel));

  vac_strategy = bstrategy;

  vacuum_set_xid_limits(onerel, params->freeze_min_age, params->freeze_table_age, params->multixact_freeze_min_age, params->multixact_freeze_table_age, &OldestXmin, &FreezeLimit, &xidFullScanLimit, &MultiXactCutoff, &mxactFullScanLimit);

     
                                                                          
                                                                         
                                                                         
                                                                            
     
  aggressive = TransactionIdPrecedesOrEquals(onerel->rd_rel->relfrozenxid, xidFullScanLimit);
  aggressive |= MultiXactIdPrecedesOrEquals(onerel->rd_rel->relminmxid, mxactFullScanLimit);
  if (params->options & VACOPT_DISABLE_PAGE_SKIPPING)
  {
    aggressive = true;
  }

  vacrelstats = (LVRelStats *)palloc0(sizeof(LVRelStats));

  vacrelstats->old_rel_pages = onerel->rd_rel->relpages;
  vacrelstats->old_live_tuples = onerel->rd_rel->reltuples;
  vacrelstats->num_index_scans = 0;
  vacrelstats->pages_removed = 0;
  vacrelstats->lock_waiter_detected = false;

                                        
  vac_open_indexes(onerel, RowExclusiveLock, &nindexes, &Irel);
  vacrelstats->useindex = (nindexes > 0 && params->index_cleanup == VACOPT_TERNARY_ENABLED);

                        
  lazy_scan_heap(onerel, params, vacrelstats, Irel, nindexes, aggressive);

                         
  vac_close_indexes(nindexes, Irel, NoLock);

     
                                                                            
                                                
     
                                                                            
                              
     
  if ((vacrelstats->scanned_pages + vacrelstats->frozenskipped_pages) < vacrelstats->rel_pages)
  {
    Assert(!aggressive);
    scanned_all_unfrozen = false;
  }
  else
  {
    scanned_all_unfrozen = true;
  }

     
                                       
     
  if (should_attempt_truncation(params, vacrelstats))
  {
    lazy_truncate_heap(onerel, vacrelstats);
  }

                                                  
  pgstat_progress_update_param(PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_PHASE_FINAL_CLEANUP);

     
                                    
     
                                                                            
                                                                           
                                                                            
                                                                     
                                                                        
                                                                         
     
                                                                             
                                                                          
                                                                             
     
                                                                            
                                                                            
                                                         
     
                                                                         
                                                                             
     
  new_rel_pages = vacrelstats->rel_pages;
  new_live_tuples = vacrelstats->new_live_tuples;
  if (vacrelstats->tupcount_pages == 0 && new_rel_pages > 0)
  {
    new_rel_pages = vacrelstats->old_rel_pages;
    new_live_tuples = vacrelstats->old_live_tuples;
  }

  visibilitymap_count(onerel, &new_rel_allvisible, NULL);
  if (new_rel_allvisible > new_rel_pages)
  {
    new_rel_allvisible = new_rel_pages;
  }

  new_frozen_xid = scanned_all_unfrozen ? FreezeLimit : InvalidTransactionId;
  new_min_multi = scanned_all_unfrozen ? MultiXactCutoff : InvalidMultiXactId;

  vac_update_relstats(onerel, new_rel_pages, new_live_tuples, new_rel_allvisible, nindexes > 0, new_frozen_xid, new_min_multi, false);

                                                  
  pgstat_report_vacuum(RelationGetRelid(onerel), onerel->rd_rel->relisshared, new_live_tuples, vacrelstats->new_dead_tuples);
  pgstat_progress_end_command();

                                         
  if (IsAutoVacuumWorkerProcess() && params->log_min_duration >= 0)
  {
    TimestampTz endtime = GetCurrentTimestamp();

    if (params->log_min_duration == 0 || TimestampDifferenceExceeds(starttime, endtime, params->log_min_duration))
    {
      StringInfoData buf;
      char *msgfmt;

      TimestampDifference(starttime, endtime, &secs, &usecs);

      read_rate = 0;
      write_rate = 0;
      if ((secs > 0) || (usecs > 0))
      {
        read_rate = (double)BLCKSZ * VacuumPageMiss / (1024 * 1024) / (secs + usecs / 1000000.0);
        write_rate = (double)BLCKSZ * VacuumPageDirty / (1024 * 1024) / (secs + usecs / 1000000.0);
      }

         
                                                                      
                                                                       
         
      initStringInfo(&buf);
      if (params->is_wraparound)
      {
        if (aggressive)
        {
          msgfmt = _("automatic aggressive vacuum to prevent wraparound of table \"%s.%s.%s\": index scans: %d\n");
        }
        else
        {
          msgfmt = _("automatic vacuum to prevent wraparound of table \"%s.%s.%s\": index scans: %d\n");
        }
      }
      else
      {
        if (aggressive)
        {
          msgfmt = _("automatic aggressive vacuum of table \"%s.%s.%s\": index scans: %d\n");
        }
        else
        {
          msgfmt = _("automatic vacuum of table \"%s.%s.%s\": index scans: %d\n");
        }
      }
      appendStringInfo(&buf, msgfmt, get_database_name(MyDatabaseId), get_namespace_name(RelationGetNamespace(onerel)), RelationGetRelationName(onerel), vacrelstats->num_index_scans);
      appendStringInfo(&buf, _("pages: %u removed, %u remain, %u skipped due to pins, %u skipped frozen\n"), vacrelstats->pages_removed, vacrelstats->rel_pages, vacrelstats->pinskipped_pages, vacrelstats->frozenskipped_pages);
      appendStringInfo(&buf, _("tuples: %.0f removed, %.0f remain, %.0f are dead but not yet removable, oldest xmin: %u\n"), vacrelstats->tuples_deleted, vacrelstats->new_rel_tuples, vacrelstats->new_dead_tuples, OldestXmin);
      appendStringInfo(&buf, _("buffer usage: %d hits, %d misses, %d dirtied\n"), VacuumPageHit, VacuumPageMiss, VacuumPageDirty);
      appendStringInfo(&buf, _("avg read rate: %.3f MB/s, avg write rate: %.3f MB/s\n"), read_rate, write_rate);
      appendStringInfo(&buf, _("system usage: %s"), pg_rusage_show(&ru0));

      ereport(LOG, (errmsg_internal("%s", buf.data)));
      pfree(buf.data);
    }
  }
}

   
                                                                        
                                                                      
                                                                    
                                                                     
                                                                   
                                                                      
                                                                          
                                                                    
                                                                      
                                                                       
                                                                     
                                                                        
                                                                        
                                                           
   
static void
vacuum_log_cleanup_info(Relation rel, LVRelStats *vacrelstats)
{
     
                                                                            
                                             
     
  if (!RelationNeedsWAL(rel) || !XLogIsNeeded())
  {
    return;
  }

     
                                                                         
     
  if (TransactionIdIsValid(vacrelstats->latestRemovedXid))
  {
    (void)log_heap_cleanup_info(rel->rd_node, vacrelstats->latestRemovedXid);
  }
}

   
                                                  
   
                                                                      
                                                                      
                                                                            
                                                                          
                                                                 
                                                                            
                                                                           
                                   
   
                                                                          
                                                                           
                                     
   
static void
lazy_scan_heap(Relation onerel, VacuumParams *params, LVRelStats *vacrelstats, Relation *Irel, int nindexes, bool aggressive)
{
  BlockNumber nblocks, blkno;
  HeapTupleData tuple;
  char *relname;
  TransactionId relfrozenxid = onerel->rd_rel->relfrozenxid;
  TransactionId relminmxid = onerel->rd_rel->relminmxid;
  BlockNumber empty_pages, vacuumed_pages, next_fsm_block_to_vacuum;
  double num_tuples,                                          
      live_tuples,                                         
      tups_vacuumed,                                  
      nkeep,                                            
      nunused;                                 
  IndexBulkDeleteResult **indstats;
  int i;
  PGRUsage ru0;
  Buffer vmbuffer = InvalidBuffer;
  BlockNumber next_unskippable_block;
  bool skipping_blocks;
  xl_heap_freeze_tuple *frozen;
  StringInfoData buf;
  const int initprog_index[] = {PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_TOTAL_HEAP_BLKS, PROGRESS_VACUUM_MAX_DEAD_TUPLES};
  int64 initprog_val[3];

  pg_rusage_init(&ru0);

  relname = RelationGetRelationName(onerel);
  if (aggressive)
  {
    ereport(elevel, (errmsg("aggressively vacuuming \"%s.%s\"", get_namespace_name(RelationGetNamespace(onerel)), relname)));
  }
  else
  {
    ereport(elevel, (errmsg("vacuuming \"%s.%s\"", get_namespace_name(RelationGetNamespace(onerel)), relname)));
  }

  empty_pages = vacuumed_pages = 0;
  next_fsm_block_to_vacuum = (BlockNumber)0;
  num_tuples = live_tuples = tups_vacuumed = nkeep = nunused = 0;

  indstats = (IndexBulkDeleteResult **)palloc0(nindexes * sizeof(IndexBulkDeleteResult *));

  nblocks = RelationGetNumberOfBlocks(onerel);
  vacrelstats->rel_pages = nblocks;
  vacrelstats->scanned_pages = 0;
  vacrelstats->tupcount_pages = 0;
  vacrelstats->nonempty_pages = 0;
  vacrelstats->latestRemovedXid = InvalidTransactionId;

  lazy_space_alloc(vacrelstats, nblocks);
  frozen = palloc(sizeof(xl_heap_freeze_tuple) * MaxHeapTuplesPerPage);

                                                                          
  initprog_val[0] = PROGRESS_VACUUM_PHASE_SCAN_HEAP;
  initprog_val[1] = nblocks;
  initprog_val[2] = vacrelstats->max_dead_tuples;
  pgstat_progress_update_multi_param(3, initprog_index, initprog_val);

     
                                                                   
                                                                            
                                                                           
                                                                          
                                                                    
                                                                         
                                                                            
                                              
     
                                                                       
                                                                         
                                                                             
                                                
     
                                                                 
                                                                            
                                                                             
                                                                            
                                                                          
                 
     
                                                                            
                                                                           
                                                                           
                                                                             
                                                                            
                                                                           
                                                                         
                                                                             
                                                                          
                                                                          
                                                                             
                                                                             
     
                                                                   
                                                                            
                                                                             
                                                                      
                                                                             
                                                                          
                                                                            
                                                                 
     
  next_unskippable_block = 0;
  if ((params->options & VACOPT_DISABLE_PAGE_SKIPPING) == 0)
  {
    while (next_unskippable_block < nblocks)
    {
      uint8 vmstatus;

      vmstatus = visibilitymap_get_status(onerel, next_unskippable_block, &vmbuffer);
      if (aggressive)
      {
        if ((vmstatus & VISIBILITYMAP_ALL_FROZEN) == 0)
        {
          break;
        }
      }
      else
      {
        if ((vmstatus & VISIBILITYMAP_ALL_VISIBLE) == 0)
        {
          break;
        }
      }
      vacuum_delay_point();
      next_unskippable_block++;
    }
  }

  if (next_unskippable_block >= SKIP_PAGES_THRESHOLD)
  {
    skipping_blocks = true;
  }
  else
  {
    skipping_blocks = false;
  }

  for (blkno = 0; blkno < nblocks; blkno++)
  {
    Buffer buf;
    Page page;
    OffsetNumber offnum, maxoff;
    bool tupgone, hastup;
    int prev_dead_count;
    int nfrozen;
    Size freespace;
    bool all_visible_according_to_vm = false;
    bool all_visible;
    bool all_frozen = true;                                        
    bool has_dead_tuples;
    TransactionId visibility_cutoff_xid = InvalidTransactionId;

                                                            
#define FORCE_CHECK_PAGE() (blkno == nblocks - 1 && should_attempt_truncation(params, vacrelstats))

    pgstat_progress_update_param(PROGRESS_VACUUM_HEAP_BLKS_SCANNED, blkno);

    if (blkno == next_unskippable_block)
    {
                                                  
      next_unskippable_block++;
      if ((params->options & VACOPT_DISABLE_PAGE_SKIPPING) == 0)
      {
        while (next_unskippable_block < nblocks)
        {
          uint8 vmskipflags;

          vmskipflags = visibilitymap_get_status(onerel, next_unskippable_block, &vmbuffer);
          if (aggressive)
          {
            if ((vmskipflags & VISIBILITYMAP_ALL_FROZEN) == 0)
            {
              break;
            }
          }
          else
          {
            if ((vmskipflags & VISIBILITYMAP_ALL_VISIBLE) == 0)
            {
              break;
            }
          }
          vacuum_delay_point();
          next_unskippable_block++;
        }
      }

         
                                                              
                                                                        
         
      if (next_unskippable_block - blkno > SKIP_PAGES_THRESHOLD)
      {
        skipping_blocks = true;
      }
      else
      {
        skipping_blocks = false;
      }

         
                                                                         
                                                                         
                                                                     
         
      if (aggressive && VM_ALL_VISIBLE(onerel, blkno, &vmbuffer))
      {
        all_visible_according_to_vm = true;
      }
    }
    else
    {
         
                                                                     
                                                                         
                                                               
                                                                 
                                                                       
         
      if (skipping_blocks && !FORCE_CHECK_PAGE())
      {
           
                                                                      
                                                                       
                                                                    
                                                                    
                                                                      
                                                                   
                                                                      
                                                     
           
        if (aggressive || VM_ALL_FROZEN(onerel, blkno, &vmbuffer))
        {
          vacrelstats->frozenskipped_pages++;
        }
        continue;
      }
      all_visible_according_to_vm = true;
    }

    vacuum_delay_point();

       
                                                                         
                                                                           
       
    if ((vacrelstats->max_dead_tuples - vacrelstats->num_dead_tuples) < MaxHeapTuplesPerPage && vacrelstats->num_dead_tuples > 0)
    {
      const int hvp_index[] = {PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_NUM_INDEX_VACUUMS};
      int64 hvp_val[2];

         
                                                                     
                                                                    
                                                                   
                                                
         
      if (BufferIsValid(vmbuffer))
      {
        ReleaseBuffer(vmbuffer);
        vmbuffer = InvalidBuffer;
      }

                                                    
      vacuum_log_cleanup_info(onerel, vacrelstats);

                                                    
      pgstat_progress_update_param(PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_PHASE_VACUUM_INDEX);

                                
      for (i = 0; i < nindexes; i++)
      {
        lazy_vacuum_index(Irel[i], &indstats[i], vacrelstats);
      }

         
                                                                      
                                                            
                                                               
                                
         
      hvp_val[0] = PROGRESS_VACUUM_PHASE_VACUUM_HEAP;
      hvp_val[1] = vacrelstats->num_index_scans + 1;
      pgstat_progress_update_multi_param(2, hvp_index, hvp_val);

                                   
      lazy_vacuum_heap(onerel, vacrelstats);

         
                                                                      
                                                                      
                
         
      vacrelstats->num_dead_tuples = 0;
      vacrelstats->num_index_scans++;

         
                                                                        
                                                                       
         
      FreeSpaceMapVacuumRange(onerel, next_fsm_block_to_vacuum, blkno);
      next_fsm_block_to_vacuum = blkno;

                                                           
      pgstat_progress_update_param(PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_PHASE_SCAN_HEAP);
    }

       
                                                                    
                                                                          
                                                                   
                                                                          
                                                                           
                                 
       
       
    visibilitymap_pin(onerel, blkno, &vmbuffer);

    buf = ReadBufferExtended(onerel, MAIN_FORKNUM, blkno, RBM_NORMAL, vac_strategy);

                                                                      
    if (!ConditionalLockBufferForCleanup(buf))
    {
         
                                                                         
                                                                        
                                                                         
                                                   
         
      if (!aggressive && !FORCE_CHECK_PAGE())
      {
        ReleaseBuffer(buf);
        vacrelstats->pinskipped_pages++;
        continue;
      }

         
                                                                        
                                                                      
                                                                        
         
                                                                         
                                                                
                                                                       
                                                              
         
                                                                        
                                                                      
                                                                        
                                                                       
                                                                       
         
      LockBuffer(buf, BUFFER_LOCK_SHARE);
      if (!lazy_check_needs_freeze(buf, &hastup))
      {
        UnlockReleaseBuffer(buf);
        vacrelstats->scanned_pages++;
        vacrelstats->pinskipped_pages++;
        if (hastup)
        {
          vacrelstats->nonempty_pages = blkno + 1;
        }
        continue;
      }
      if (!aggressive)
      {
           
                                                                      
                                                                   
           
        UnlockReleaseBuffer(buf);
        vacrelstats->pinskipped_pages++;
        if (hastup)
        {
          vacrelstats->nonempty_pages = blkno + 1;
        }
        continue;
      }
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
      LockBufferForCleanup(buf);
                                             
    }

    vacrelstats->scanned_pages++;
    vacrelstats->tupcount_pages++;

    page = BufferGetPage(buf);

    if (PageIsNew(page))
    {
      bool still_new;

         
                                                                       
                                                                     
                                                                       
                                                                         
                                                             
         
                                                                     
                                                                      
                                     
         
                                                                        
                                                                       
                                                                        
                                                                  
                                                                       
                                                                      
         

         
                                                                  
                                 
         
      still_new = PageIsNew(page);
      UnlockReleaseBuffer(buf);

      if (still_new)
      {
        empty_pages++;

        if (GetRecordedFreeSpace(onerel, blkno) == 0)
        {
          Size freespace;

          freespace = BufferGetPageSize(buf) - SizeOfPageHeaderData;
          RecordPageWithFreeSpace(onerel, blkno, freespace);
        }
      }
      continue;
    }

    if (PageIsEmpty(page))
    {
      empty_pages++;
      freespace = PageGetHeapFreeSpace(page);

         
                                                                      
                                                                   
         
      if (!PageIsAllVisible(page))
      {
        START_CRIT_SECTION();

                                                           
        MarkBufferDirty(buf);

           
                                                                     
                                                                     
                                                                     
                                                                    
                                                                       
                                                                   
                                                                    
                
           
        if (RelationNeedsWAL(onerel) && PageGetLSN(page) == InvalidXLogRecPtr)
        {
          log_newpage_buffer(buf, true);
        }

        PageSetAllVisible(page);
        visibilitymap_set(onerel, blkno, buf, InvalidXLogRecPtr, vmbuffer, InvalidTransactionId, VISIBILITYMAP_ALL_VISIBLE | VISIBILITYMAP_ALL_FROZEN);
        END_CRIT_SECTION();
      }

      UnlockReleaseBuffer(buf);
      RecordPageWithFreeSpace(onerel, blkno, freespace);
      continue;
    }

       
                                                 
       
                                                                         
       
    tups_vacuumed += heap_page_prune(onerel, buf, OldestXmin, false, &vacrelstats->latestRemovedXid);

       
                                                                          
                           
       
    all_visible = true;
    has_dead_tuples = false;
    nfrozen = 0;
    hastup = false;
    prev_dead_count = vacrelstats->num_dead_tuples;
    maxoff = PageGetMaxOffsetNumber(page);

       
                                                                    
                                                                    
       
    for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
    {
      ItemId itemid;

      itemid = PageGetItemId(page, offnum);

                                                                
      if (!ItemIdIsUsed(itemid))
      {
        nunused += 1;
        continue;
      }

                                             
      if (ItemIdIsRedirected(itemid))
      {
        hastup = true;                                     
        continue;
      }

      ItemPointerSet(&(tuple.t_self), blkno, offnum);

         
                                                                      
                                                                       
                                                                        
                           
         
      if (ItemIdIsDead(itemid))
      {
        lazy_record_dead_tuple(vacrelstats, &(tuple.t_self));
        all_visible = false;
        continue;
      }

      Assert(ItemIdIsNormal(itemid));

      tuple.t_data = (HeapTupleHeader)PageGetItem(page, itemid);
      tuple.t_len = ItemIdGetLength(itemid);
      tuple.t_tableOid = RelationGetRelid(onerel);

      tupgone = false;

         
                                                                         
                                                                      
                                                                   
                                                                
         
                                                                        
                                                                       
                                                                 
                       
         
      switch (HeapTupleSatisfiesVacuum(&tuple, OldestXmin, buf))
      {
      case HEAPTUPLE_DEAD:

           
                                                              
                                                               
                                                             
                                                             
                                                             
                                                    
           
                                                            
                                                                  
                                                             
                                                               
                                                                  
                                                                
                                                              
                                                                   
                                                                
                 
           
                                                                   
                                                           
                                                            
                                                                   
                                                          
                                  
           
        if (HeapTupleIsHotUpdated(&tuple) || HeapTupleIsHeapOnly(&tuple) || params->index_cleanup == VACOPT_TERNARY_DISABLED)
        {
          nkeep += 1;
        }
        else
        {
          tupgone = true;                              
        }
        all_visible = false;
        break;
      case HEAPTUPLE_LIVE:

           
                                                                 
                                                 
           
        live_tuples += 1;

           
                                                                
           
                                                               
                                                         
                                                                
                                                              
                 
           
        if (all_visible)
        {
          TransactionId xmin;

          if (!HeapTupleHeaderXminCommitted(tuple.t_data))
          {
            all_visible = false;
            break;
          }

             
                                                              
                                                        
             
          xmin = HeapTupleHeaderGetXmin(tuple.t_data);
          if (!TransactionIdPrecedes(xmin, OldestXmin))
          {
            all_visible = false;
            break;
          }

                                          
          if (TransactionIdFollows(xmin, visibility_cutoff_xid))
          {
            visibility_cutoff_xid = xmin;
          }
        }
        break;
      case HEAPTUPLE_RECENTLY_DEAD:

           
                                                                   
                          
           
        nkeep += 1;
        all_visible = false;
        break;
      case HEAPTUPLE_INSERT_IN_PROGRESS:

           
                                                              
           
                                                                 
                                                               
                                                                
                                                                
                                                            
                       
           
        all_visible = false;
        break;
      case HEAPTUPLE_DELETE_IN_PROGRESS:
                                                               
        all_visible = false;

           
                                                             
                                                           
                                     
           
        live_tuples += 1;
        break;
      default:
        elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
        break;
      }

      if (tupgone)
      {
        lazy_record_dead_tuple(vacrelstats, &(tuple.t_self));
        HeapTupleHeaderAdvanceLatestRemovedXid(tuple.t_data, &vacrelstats->latestRemovedXid);
        tups_vacuumed += 1;
        has_dead_tuples = true;
      }
      else
      {
        bool tuple_totally_frozen;

        num_tuples += 1;
        hastup = true;

           
                                                                       
                                                                  
           
        if (heap_prepare_freeze_tuple(tuple.t_data, relfrozenxid, relminmxid, FreezeLimit, MultiXactCutoff, &frozen[nfrozen], &tuple_totally_frozen))
        {
          frozen[nfrozen++].offset = offnum;
        }

        if (!tuple_totally_frozen)
        {
          all_frozen = false;
        }
      }
    }                      

       
                                                                      
                                                                    
                                                     
       
    if (nfrozen > 0)
    {
      START_CRIT_SECTION();

      MarkBufferDirty(buf);

                                     
      for (i = 0; i < nfrozen; i++)
      {
        ItemId itemid;
        HeapTupleHeader htup;

        itemid = PageGetItemId(page, frozen[i].offset);
        htup = (HeapTupleHeader)PageGetItem(page, itemid);

        heap_execute_freeze_tuple(htup, &frozen[i]);
      }

                                             
      if (RelationNeedsWAL(onerel))
      {
        XLogRecPtr recptr;

        recptr = log_heap_freeze(onerel, buf, FreezeLimit, frozen, nfrozen);
        PageSetLSN(page, recptr);
      }

      END_CRIT_SECTION();
    }

       
                                                                           
                                                                         
                                       
       
    if (!vacrelstats->useindex && vacrelstats->num_dead_tuples > 0)
    {
      if (nindexes == 0)
      {
                                                               
        lazy_vacuum_page(onerel, blkno, buf, 0, vacrelstats, &vmbuffer);
        vacuumed_pages++;
        has_dead_tuples = false;
      }
      else
      {
           
                                                                
                                                                     
                        
           
                                                                      
                                                                      
                                                                      
                                                         
           
        Assert(params->index_cleanup == VACOPT_TERNARY_DISABLED);
      }

         
                                                                      
                                                                      
                
         
      vacrelstats->num_dead_tuples = 0;

         
                                                                       
                                                                         
                                                                       
                                                                    
         
      if (blkno - next_fsm_block_to_vacuum >= VACUUM_FSM_EVERY_PAGES)
      {
        FreeSpaceMapVacuumRange(onerel, next_fsm_block_to_vacuum, blkno);
        next_fsm_block_to_vacuum = blkno;
      }
    }

    freespace = PageGetHeapFreeSpace(page);

                                               
    if (all_visible && !all_visible_according_to_vm)
    {
      uint8 flags = VISIBILITYMAP_ALL_VISIBLE;

      if (all_frozen)
      {
        flags |= VISIBILITYMAP_ALL_FROZEN;
      }

         
                                                                         
                                                                       
                                                                        
                                      
         
                                                                        
                                                                      
                                                                    
                                                                      
                                                                         
                                                               
         
      PageSetAllVisible(page);
      MarkBufferDirty(buf);
      visibilitymap_set(onerel, blkno, buf, InvalidXLogRecPtr, vmbuffer, visibility_cutoff_xid, flags);
    }

       
                                                                           
                                                                         
                                                                     
                                                                         
                                        
       
    else if (all_visible_according_to_vm && !PageIsAllVisible(page) && VM_ALL_VISIBLE(onerel, blkno, &vmbuffer))
    {
      elog(WARNING, "page is not marked all-visible but visibility map bit is set in relation \"%s\" page %u", relname, blkno);
      visibilitymap_clear(onerel, blkno, vmbuffer, VISIBILITYMAP_VALID_BITS);
    }

       
                                                                       
                                                                        
                                                                       
                                                                
                                                                     
                                                                           
                                                                           
                                                             
       
                                                                       
                     
       
    else if (PageIsAllVisible(page) && has_dead_tuples)
    {
      elog(WARNING, "page containing dead tuples is marked as all-visible in relation \"%s\" page %u", relname, blkno);
      PageClearAllVisible(page);
      MarkBufferDirty(buf);
      visibilitymap_clear(onerel, blkno, vmbuffer, VISIBILITYMAP_VALID_BITS);
    }

       
                                                                      
                                                                         
                                                      
       
    else if (all_visible_according_to_vm && all_visible && all_frozen && !VM_ALL_FROZEN(onerel, blkno, &vmbuffer))
    {
         
                                                                  
                                                                   
                    
         
      visibilitymap_set(onerel, blkno, buf, InvalidXLogRecPtr, vmbuffer, InvalidTransactionId, VISIBILITYMAP_ALL_FROZEN);
    }

    UnlockReleaseBuffer(buf);

                                                                         
    if (hastup)
    {
      vacrelstats->nonempty_pages = blkno + 1;
    }

       
                                                                       
                                                                        
                                                                          
                                                                          
                                       
       
    if (vacrelstats->num_dead_tuples == prev_dead_count)
    {
      RecordPageWithFreeSpace(onerel, blkno, freespace);
    }
  }

                                                      
  pgstat_progress_update_param(PROGRESS_VACUUM_HEAP_BLKS_SCANNED, blkno);

  pfree(frozen);

                                
  vacrelstats->tuples_deleted = tups_vacuumed;
  vacrelstats->new_dead_tuples = nkeep;

                                                               
  vacrelstats->new_live_tuples = vac_estimate_reltuples(onerel, nblocks, vacrelstats->tupcount_pages, live_tuples);

                                                           
  vacrelstats->new_rel_tuples = vacrelstats->new_live_tuples + vacrelstats->new_dead_tuples;

     
                                                       
     
  if (BufferIsValid(vmbuffer))
  {
    ReleaseBuffer(vmbuffer);
    vmbuffer = InvalidBuffer;
  }

                                                                    
                                                         
  if (vacrelstats->num_dead_tuples > 0)
  {
    const int hvp_index[] = {PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_NUM_INDEX_VACUUMS};
    int64 hvp_val[2];

                                                  
    vacuum_log_cleanup_info(onerel, vacrelstats);

                                                  
    pgstat_progress_update_param(PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_PHASE_VACUUM_INDEX);

                              
    for (i = 0; i < nindexes; i++)
    {
      lazy_vacuum_index(Irel[i], &indstats[i], vacrelstats);
    }

                                                   
    hvp_val[0] = PROGRESS_VACUUM_PHASE_VACUUM_HEAP;
    hvp_val[1] = vacrelstats->num_index_scans + 1;
    pgstat_progress_update_multi_param(2, hvp_index, hvp_val);

                                 
    pgstat_progress_update_param(PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_PHASE_VACUUM_HEAP);
    lazy_vacuum_heap(onerel, vacrelstats);
    vacrelstats->num_index_scans++;
  }

     
                                                                             
                             
     
  if (blkno > next_fsm_block_to_vacuum)
  {
    FreeSpaceMapVacuumRange(onerel, next_fsm_block_to_vacuum, blkno);
  }

                                                              
  pgstat_progress_update_param(PROGRESS_VACUUM_HEAP_BLKS_VACUUMED, blkno);
  pgstat_progress_update_param(PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_PHASE_INDEX_CLEANUP);

                                                                   
  if (vacrelstats->useindex)
  {
    for (i = 0; i < nindexes; i++)
    {
      lazy_cleanup_index(Irel[i], indstats[i], vacrelstats);
    }
  }

                                                                          
  if (vacuumed_pages)
  {
    ereport(elevel, (errmsg("\"%s\": removed %.0f row versions in %u pages", RelationGetRelationName(onerel), tups_vacuumed, vacuumed_pages)));
  }

     
                                                                           
                                                          
     
  initStringInfo(&buf);
  appendStringInfo(&buf, _("%.0f dead row versions cannot be removed yet, oldest xmin: %u\n"), nkeep, OldestXmin);
  appendStringInfo(&buf, _("There were %.0f unused item identifiers.\n"), nunused);
  appendStringInfo(&buf, ngettext("Skipped %u page due to buffer pins, ", "Skipped %u pages due to buffer pins, ", vacrelstats->pinskipped_pages), vacrelstats->pinskipped_pages);
  appendStringInfo(&buf, ngettext("%u frozen page.\n", "%u frozen pages.\n", vacrelstats->frozenskipped_pages), vacrelstats->frozenskipped_pages);
  appendStringInfo(&buf, ngettext("%u page is entirely empty.\n", "%u pages are entirely empty.\n", empty_pages), empty_pages);
  appendStringInfo(&buf, _("%s."), pg_rusage_show(&ru0));

  ereport(elevel, (errmsg("\"%s\": found %.0f removable, %.0f nonremovable row versions in %u out of %u pages", RelationGetRelationName(onerel), tups_vacuumed, num_tuples, vacrelstats->scanned_pages, nblocks), errdetail_internal("%s", buf.data)));
  pfree(buf.data);
}

   
                                                   
   
                                                                   
                                                                      
                                           
   
                                                                        
                                                                      
                                                                
   
static void
lazy_vacuum_heap(Relation onerel, LVRelStats *vacrelstats)
{
  int tupindex;
  int npages;
  PGRUsage ru0;
  Buffer vmbuffer = InvalidBuffer;

  pg_rusage_init(&ru0);
  npages = 0;

  tupindex = 0;
  while (tupindex < vacrelstats->num_dead_tuples)
  {
    BlockNumber tblk;
    Buffer buf;
    Page page;
    Size freespace;

    vacuum_delay_point();

    tblk = ItemPointerGetBlockNumber(&vacrelstats->dead_tuples[tupindex]);
    buf = ReadBufferExtended(onerel, MAIN_FORKNUM, tblk, RBM_NORMAL, vac_strategy);
    if (!ConditionalLockBufferForCleanup(buf))
    {
      ReleaseBuffer(buf);
      ++tupindex;
      continue;
    }
    tupindex = lazy_vacuum_page(onerel, tblk, buf, tupindex, vacrelstats, &vmbuffer);

                                                                       
    page = BufferGetPage(buf);
    freespace = PageGetHeapFreeSpace(page);

    UnlockReleaseBuffer(buf);
    RecordPageWithFreeSpace(onerel, tblk, freespace);
    npages++;
  }

  if (BufferIsValid(vmbuffer))
  {
    ReleaseBuffer(vmbuffer);
    vmbuffer = InvalidBuffer;
  }

  ereport(elevel, (errmsg("\"%s\": removed %d row versions in %d pages", RelationGetRelationName(onerel), tupindex, npages), errdetail_internal("%s", pg_rusage_show(&ru0))));
}

   
                                                    
                                      
   
                                                               
   
                                                                       
                                                                 
                                                                         
   
static int
lazy_vacuum_page(Relation onerel, BlockNumber blkno, Buffer buffer, int tupindex, LVRelStats *vacrelstats, Buffer *vmbuffer)
{
  Page page = BufferGetPage(buffer);
  OffsetNumber unused[MaxOffsetNumber];
  int uncnt = 0;
  TransactionId visibility_cutoff_xid;
  bool all_frozen;

  pgstat_progress_update_param(PROGRESS_VACUUM_HEAP_BLKS_VACUUMED, blkno);

  START_CRIT_SECTION();

  for (; tupindex < vacrelstats->num_dead_tuples; tupindex++)
  {
    BlockNumber tblk;
    OffsetNumber toff;
    ItemId itemid;

    tblk = ItemPointerGetBlockNumber(&vacrelstats->dead_tuples[tupindex]);
    if (tblk != blkno)
    {
      break;                                        
    }
    toff = ItemPointerGetOffsetNumber(&vacrelstats->dead_tuples[tupindex]);
    itemid = PageGetItemId(page, toff);
    ItemIdSetUnused(itemid);
    unused[uncnt++] = toff;
  }

  PageRepairFragmentation(page);

     
                                            
     
  MarkBufferDirty(buffer);

                  
  if (RelationNeedsWAL(onerel))
  {
    XLogRecPtr recptr;

    recptr = log_heap_clean(onerel, buffer, NULL, 0, NULL, 0, unused, uncnt, vacrelstats->latestRemovedXid);
    PageSetLSN(page, recptr);
  }

     
                                                                       
                                                                            
                                                                       
                                                             
     
  END_CRIT_SECTION();

     
                                                                        
                                                                           
                                                                           
                                            
     
  if (heap_page_is_all_visible(onerel, buffer, &visibility_cutoff_xid, &all_frozen))
  {
    PageSetAllVisible(page);
  }

     
                                                                         
                                                                             
                                                                   
     
  if (PageIsAllVisible(page))
  {
    uint8 vm_status = visibilitymap_get_status(onerel, blkno, vmbuffer);
    uint8 flags = 0;

                                                      
    if ((vm_status & VISIBILITYMAP_ALL_VISIBLE) == 0)
    {
      flags |= VISIBILITYMAP_ALL_VISIBLE;
    }
    if ((vm_status & VISIBILITYMAP_ALL_FROZEN) == 0 && all_frozen)
    {
      flags |= VISIBILITYMAP_ALL_FROZEN;
    }

    Assert(BufferIsValid(*vmbuffer));
    if (flags != 0)
    {
      visibilitymap_set(onerel, blkno, buffer, InvalidXLogRecPtr, *vmbuffer, visibility_cutoff_xid, flags);
    }
  }

  return tupindex;
}

   
                                                               
                                               
   
                                                                     
                                                                           
   
static bool
lazy_check_needs_freeze(Buffer buf, bool *hastup)
{
  Page page = BufferGetPage(buf);
  OffsetNumber offnum, maxoff;
  HeapTupleHeader tupleheader;

  *hastup = false;

     
                                                                         
                                                                            
                                                                       
                                            
     
  if (PageIsNew(page) || PageIsEmpty(page))
  {
    return false;
  }

  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
  {
    ItemId itemid;

    itemid = PageGetItemId(page, offnum);

                                                                     
    if (ItemIdIsUsed(itemid))
    {
      *hastup = true;
    }

                                                     
    if (!ItemIdIsNormal(itemid))
    {
      continue;
    }

    tupleheader = (HeapTupleHeader)PageGetItem(page, itemid);

    if (heap_tuple_needs_freeze(tupleheader, FreezeLimit, MultiXactCutoff, buf))
    {
      return true;
    }
  }                      

  return false;
}

   
                                                     
   
                                                              
                                                             
   
static void
lazy_vacuum_index(Relation indrel, IndexBulkDeleteResult **stats, LVRelStats *vacrelstats)
{
  IndexVacuumInfo ivinfo;
  PGRUsage ru0;

  pg_rusage_init(&ru0);

  ivinfo.index = indrel;
  ivinfo.analyze_only = false;
  ivinfo.report_progress = false;
  ivinfo.estimated_count = true;
  ivinfo.message_level = elevel;
                                                                        
  ivinfo.num_heap_tuples = vacrelstats->old_live_tuples;
  ivinfo.strategy = vac_strategy;

                        
  *stats = index_bulk_delete(&ivinfo, *stats, lazy_tid_reaped, (void *)vacrelstats);

  ereport(elevel, (errmsg("scanned index \"%s\" to remove %d row versions", RelationGetRelationName(indrel), vacrelstats->num_dead_tuples), errdetail_internal("%s", pg_rusage_show(&ru0))));
}

   
                                                                          
   
static void
lazy_cleanup_index(Relation indrel, IndexBulkDeleteResult *stats, LVRelStats *vacrelstats)
{
  IndexVacuumInfo ivinfo;
  PGRUsage ru0;

  pg_rusage_init(&ru0);

  ivinfo.index = indrel;
  ivinfo.analyze_only = false;
  ivinfo.report_progress = false;
  ivinfo.estimated_count = (vacrelstats->tupcount_pages < vacrelstats->rel_pages);
  ivinfo.message_level = elevel;

     
                                                                       
                                                                       
                                       
     
  ivinfo.num_heap_tuples = vacrelstats->new_rel_tuples;
  ivinfo.strategy = vac_strategy;

  stats = index_vacuum_cleanup(&ivinfo, stats);

  if (!stats)
  {
    return;
  }

     
                                                                             
                  
     
  if (!stats->estimated_count)
  {
    vac_update_relstats(indrel, stats->num_pages, stats->num_index_tuples, 0, false, InvalidTransactionId, InvalidMultiXactId, false);
  }

  ereport(elevel, (errmsg("index \"%s\" now contains %.0f row versions in %u pages", RelationGetRelationName(indrel), stats->num_index_tuples, stats->num_pages), errdetail("%.0f index row versions were removed.\n"
                                                                                                                                                                            "%u index pages have been deleted, %u are currently reusable.\n"
                                                                                                                                                                            "%s.",
                                                                                                                                                                      stats->tuples_removed, stats->pages_deleted, stats->pages_free, pg_rusage_show(&ru0))));

  pfree(stats);
}

   
                                                                       
   
                                                                         
                                                               
   
                                                                            
                                                                          
                                                                             
                                                                             
                                                                         
                                                                             
                                                                               
                      
   
                                                                           
                                                                          
                                                                            
   
static bool
should_attempt_truncation(VacuumParams *params, LVRelStats *vacrelstats)
{
  BlockNumber possibly_freeable;

  if (params->truncate == VACOPT_TERNARY_DISABLED)
  {
    return false;
  }

  possibly_freeable = vacrelstats->rel_pages - vacrelstats->nonempty_pages;
  if (possibly_freeable > 0 && (possibly_freeable >= REL_TRUNCATE_MINIMUM || possibly_freeable >= vacrelstats->rel_pages / REL_TRUNCATE_FRACTION) && old_snapshot_threshold < 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                                       
   
static void
lazy_truncate_heap(Relation onerel, LVRelStats *vacrelstats)
{
  BlockNumber old_rel_pages = vacrelstats->rel_pages;
  BlockNumber new_rel_pages;
  int lock_retry;

                                         
  pgstat_progress_update_param(PROGRESS_VACUUM_PHASE, PROGRESS_VACUUM_PHASE_TRUNCATE);

     
                                                
     
  do
  {
    PGRUsage ru0;

    pg_rusage_init(&ru0);

       
                                                                  
                                                                          
                                                                         
                                                                          
              
       
    vacrelstats->lock_waiter_detected = false;
    lock_retry = 0;
    while (true)
    {
      if (ConditionalLockRelation(onerel, AccessExclusiveLock))
      {
        break;
      }

         
                                                                         
               
         
      CHECK_FOR_INTERRUPTS();

      if (++lock_retry > (VACUUM_TRUNCATE_LOCK_TIMEOUT / VACUUM_TRUNCATE_LOCK_WAIT_INTERVAL))
      {
           
                                                                      
                                                      
           
        vacrelstats->lock_waiter_detected = true;
        ereport(elevel, (errmsg("\"%s\": stopping truncate due to conflicting lock request", RelationGetRelationName(onerel))));
        return;
      }

      pg_usleep(VACUUM_TRUNCATE_LOCK_WAIT_INTERVAL * 1000L);
    }

       
                                                                         
                                                                          
                                                                      
       
    new_rel_pages = RelationGetNumberOfBlocks(onerel);
    if (new_rel_pages != old_rel_pages)
    {
         
                                                                         
                                                                        
                                                                      
                                                                       
                                                                      
         
      UnlockRelation(onerel, AccessExclusiveLock);
      return;
    }

       
                                                                         
                                                                      
                                                                       
                       
       
    new_rel_pages = count_nondeletable_pages(onerel, vacrelstats);

    if (new_rel_pages >= old_rel_pages)
    {
                                       
      UnlockRelation(onerel, AccessExclusiveLock);
      return;
    }

       
                         
       
    RelationTruncate(onerel, new_rel_pages);

       
                                                                       
                                                                       
                                                                          
                                                                           
                                          
       
    UnlockRelation(onerel, AccessExclusiveLock);

       
                                                                     
                                                                     
                                  
       
    vacrelstats->pages_removed += old_rel_pages - new_rel_pages;
    vacrelstats->rel_pages = new_rel_pages;

    ereport(elevel, (errmsg("\"%s\": truncated %u to %u pages", RelationGetRelationName(onerel), old_rel_pages, new_rel_pages), errdetail_internal("%s", pg_rusage_show(&ru0))));
    old_rel_pages = new_rel_pages;
  } while (new_rel_pages > vacrelstats->nonempty_pages && vacrelstats->lock_waiter_detected);
}

   
                                                                     
   
                                                                  
   
static BlockNumber
count_nondeletable_pages(Relation onerel, LVRelStats *vacrelstats)
{
  BlockNumber blkno;
  BlockNumber prefetchedUntil;
  instr_time starttime;

                                                                          
  INSTR_TIME_SET_CURRENT(starttime);

     
                                                                          
                                                                            
                                                                             
                                                                   
     
  blkno = vacrelstats->rel_pages;
  StaticAssertStmt((PREFETCH_SIZE & (PREFETCH_SIZE - 1)) == 0, "prefetch size must be power of 2");
  prefetchedUntil = InvalidBlockNumber;
  while (blkno > vacrelstats->nonempty_pages)
  {
    Buffer buf;
    Page page;
    OffsetNumber offnum, maxoff;
    bool hastup;

       
                                                                        
                                                                        
                                                                         
                                                                       
                                                                    
                             
       
    if ((blkno % 32) == 0)
    {
      instr_time currenttime;
      instr_time elapsed;

      INSTR_TIME_SET_CURRENT(currenttime);
      elapsed = currenttime;
      INSTR_TIME_SUBTRACT(elapsed, starttime);
      if ((INSTR_TIME_GET_MICROSEC(elapsed) / 1000) >= VACUUM_TRUNCATE_LOCK_CHECK_INTERVAL)
      {
        if (LockHasWaitersRelation(onerel, AccessExclusiveLock))
        {
          ereport(elevel, (errmsg("\"%s\": suspending truncate due to conflicting lock request", RelationGetRelationName(onerel))));

          vacrelstats->lock_waiter_detected = true;
          return blkno;
        }
        starttime = currenttime;
      }
    }

       
                                                                     
                                                                        
                                                                         
       
    CHECK_FOR_INTERRUPTS();

    blkno--;

                                                           
    if (prefetchedUntil > blkno)
    {
      BlockNumber prefetchStart;
      BlockNumber pblkno;

      prefetchStart = blkno & ~(PREFETCH_SIZE - 1);
      for (pblkno = prefetchStart; pblkno <= blkno; pblkno++)
      {
        PrefetchBuffer(onerel, MAIN_FORKNUM, pblkno);
        CHECK_FOR_INTERRUPTS();
      }
      prefetchedUntil = prefetchStart;
    }

    buf = ReadBufferExtended(onerel, MAIN_FORKNUM, blkno, RBM_NORMAL, vac_strategy);

                                                                
    LockBuffer(buf, BUFFER_LOCK_SHARE);

    page = BufferGetPage(buf);

    if (PageIsNew(page) || PageIsEmpty(page))
    {
      UnlockReleaseBuffer(buf);
      continue;
    }

    hastup = false;
    maxoff = PageGetMaxOffsetNumber(page);
    for (offnum = FirstOffsetNumber; offnum <= maxoff; offnum = OffsetNumberNext(offnum))
    {
      ItemId itemid;

      itemid = PageGetItemId(page, offnum);

         
                                                                       
                                                                   
                                                                       
                                  
         
      if (ItemIdIsUsed(itemid))
      {
        hastup = true;
        break;                        
      }
    }                      

    UnlockReleaseBuffer(buf);

                                                
    if (hastup)
    {
      return blkno + 1;
    }
  }

     
                                                                        
                                                                            
           
     
  return vacrelstats->nonempty_pages;
}

   
                                                                 
   
                                                            
   
static void
lazy_space_alloc(LVRelStats *vacrelstats, BlockNumber relblocks)
{
  long maxtuples;
  int vac_work_mem = IsAutoVacuumWorkerProcess() && autovacuum_work_mem != -1 ? autovacuum_work_mem : maintenance_work_mem;

  if (vacrelstats->useindex)
  {
    maxtuples = (vac_work_mem * 1024L) / sizeof(ItemPointerData);
    maxtuples = Min(maxtuples, INT_MAX);
    maxtuples = Min(maxtuples, MaxAllocSize / sizeof(ItemPointerData));

                                                                         
    if ((BlockNumber)(maxtuples / LAZY_ALLOC_TUPLES) > relblocks)
    {
      maxtuples = relblocks * LAZY_ALLOC_TUPLES;
    }

                                                 
    maxtuples = Max(maxtuples, MaxHeapTuplesPerPage);
  }
  else
  {
    maxtuples = MaxHeapTuplesPerPage;
  }

  vacrelstats->num_dead_tuples = 0;
  vacrelstats->max_dead_tuples = (int)maxtuples;
  vacrelstats->dead_tuples = (ItemPointer)palloc(maxtuples * sizeof(ItemPointerData));
}

   
                                                         
   
static void
lazy_record_dead_tuple(LVRelStats *vacrelstats, ItemPointer itemptr)
{
     
                                                                        
                                                                        
                                                                      
     
  if (vacrelstats->num_dead_tuples < vacrelstats->max_dead_tuples)
  {
    vacrelstats->dead_tuples[vacrelstats->num_dead_tuples] = *itemptr;
    vacrelstats->num_dead_tuples++;
    pgstat_progress_update_param(PROGRESS_VACUUM_NUM_DEAD_TUPLES, vacrelstats->num_dead_tuples);
  }
}

   
                                                       
   
                                                                   
   
                                                  
   
static bool
lazy_tid_reaped(ItemPointer itemptr, void *state)
{
  LVRelStats *vacrelstats = (LVRelStats *)state;
  ItemPointer res;

  res = (ItemPointer)bsearch((void *)itemptr, (void *)vacrelstats->dead_tuples, vacrelstats->num_dead_tuples, sizeof(ItemPointerData), vac_cmp_itemptr);

  return (res != NULL);
}

   
                                                           
   
static int
vac_cmp_itemptr(const void *left, const void *right)
{
  BlockNumber lblk, rblk;
  OffsetNumber loff, roff;

  lblk = ItemPointerGetBlockNumber((ItemPointer)left);
  rblk = ItemPointerGetBlockNumber((ItemPointer)right);

  if (lblk < rblk)
  {
    return -1;
  }
  if (lblk > rblk)
  {
    return 1;
  }

  loff = ItemPointerGetOffsetNumber((ItemPointer)left);
  roff = ItemPointerGetOffsetNumber((ItemPointer)right);

  if (loff < roff)
  {
    return -1;
  }
  if (loff > roff)
  {
    return 1;
  }

  return 0;
}

   
                                                                               
                                                                            
                                                                            
                           
   
static bool
heap_page_is_all_visible(Relation rel, Buffer buf, TransactionId *visibility_cutoff_xid, bool *all_frozen)
{
  Page page = BufferGetPage(buf);
  BlockNumber blockno = BufferGetBlockNumber(buf);
  OffsetNumber offnum, maxoff;
  bool all_visible = true;

  *visibility_cutoff_xid = InvalidTransactionId;
  *all_frozen = true;

     
                                                                 
                                                                             
     
  maxoff = PageGetMaxOffsetNumber(page);
  for (offnum = FirstOffsetNumber; offnum <= maxoff && all_visible; offnum = OffsetNumberNext(offnum))
  {
    ItemId itemid;
    HeapTupleData tuple;

    itemid = PageGetItemId(page, offnum);

                                                             
    if (!ItemIdIsUsed(itemid) || ItemIdIsRedirected(itemid))
    {
      continue;
    }

    ItemPointerSet(&(tuple.t_self), blockno, offnum);

       
                                                                       
                                        
       
    if (ItemIdIsDead(itemid))
    {
      all_visible = false;
      *all_frozen = false;
      break;
    }

    Assert(ItemIdIsNormal(itemid));

    tuple.t_data = (HeapTupleHeader)PageGetItem(page, itemid);
    tuple.t_len = ItemIdGetLength(itemid);
    tuple.t_tableOid = RelationGetRelid(rel);

    switch (HeapTupleSatisfiesVacuum(&tuple, OldestXmin, buf))
    {
    case HEAPTUPLE_LIVE:
    {
      TransactionId xmin;

                                             
      if (!HeapTupleHeaderXminCommitted(tuple.t_data))
      {
        all_visible = false;
        *all_frozen = false;
        break;
      }

         
                                                                 
                                             
         
      xmin = HeapTupleHeaderGetXmin(tuple.t_data);
      if (!TransactionIdPrecedes(xmin, OldestXmin))
      {
        all_visible = false;
        *all_frozen = false;
        break;
      }

                                      
      if (TransactionIdFollows(xmin, *visibility_cutoff_xid))
      {
        *visibility_cutoff_xid = xmin;
      }

                                                             
      if (all_visible && *all_frozen && heap_tuple_needs_eventual_freeze(tuple.t_data))
      {
        *all_frozen = false;
      }
    }
    break;

    case HEAPTUPLE_DEAD:
    case HEAPTUPLE_RECENTLY_DEAD:
    case HEAPTUPLE_INSERT_IN_PROGRESS:
    case HEAPTUPLE_DELETE_IN_PROGRESS:
    {
      all_visible = false;
      *all_frozen = false;
      break;
    }
    default:
      elog(ERROR, "unexpected HeapTupleSatisfiesVacuum result");
      break;
    }
  }                      

  return all_visible;
}
