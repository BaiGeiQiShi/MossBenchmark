                                                                            
   
             
                                                                        
   
         
   
                                                                 
                                                                    
                                                                 
                                                                    
                                                                  
                                                                  
                                                                        
                                                                         
   
                                                                    
                                                                          
                                                                        
                                                                          
                                                                              
                                                                             
                                                                              
                                             
   
                                                                         
                                                                           
                                                                           
                                                                         
                                                                           
                                                                              
                       
   
                                                                     
                                                                       
                                                                           
                                                                            
                                                                            
                                                                             
                                                                             
                                                                            
                                                                            
                                                                           
           
   
                                                                             
                          
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include "access/nbtree.h"
#include "access/parallel.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/index.h"
#include "commands/progress.h"
#include "executor/instrument.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h"                         
#include "utils/rel.h"
#include "utils/sortsupport.h"
#include "utils/tuplesort.h"

                                              
#define PARALLEL_KEY_BTREE_SHARED UINT64CONST(0xA000000000000001)
#define PARALLEL_KEY_TUPLESORT UINT64CONST(0xA000000000000002)
#define PARALLEL_KEY_TUPLESORT_SPOOL2 UINT64CONST(0xA000000000000003)
#define PARALLEL_KEY_QUERY_TEXT UINT64CONST(0xA000000000000004)
#define PARALLEL_KEY_BUFFER_USAGE UINT64CONST(0xA000000000000005)

   
                                                                       
                                                                  
                                    
   

   
                                                                       
                                                                      
                 
   
typedef struct BTSpool
{
  Tuplesortstate *sortstate;                                 
  Relation heap;
  Relation index;
  bool isunique;
} BTSpool;

   
                                                                          
                                                                               
                                                                             
   
typedef struct BTShared
{
     
                                                                          
                                                                           
                                               
     
  Oid heaprelid;
  Oid indexrelid;
  bool isunique;
  bool isconcurrent;
  int scantuplesortstates;

     
                                                                             
                                                                         
                                                                            
                                          
     
  ConditionVariable workersdonecv;

     
                                                
     
                                                                         
                                                                             
     
  slock_t mutex;

     
                                                                       
                                     
     
                                                               
     
                                                         
     
                                                                        
            
     
                                                                          
     
                                                                        
                   
     
  int nparticipantsdone;
  double reltuples;
  bool havedead;
  double indtuples;
  bool brokenhotchain;

     
                                                                           
                                                                          
                         
     
} BTShared;

   
                                                       
   
                                                                         
             
   
#define ParallelTableScanFromBTShared(shared) (ParallelTableScanDesc)((char *)(shared) + BUFFERALIGN(sizeof(BTShared)))

   
                                              
   
typedef struct BTLeader
{
                               
  ParallelContext *pcxt;

     
                                                                    
                                                                            
                                                                   
                                 
     
  int nparticipanttuplesorts;

     
                                                                            
               
     
                                                                       
                                                                       
                                                                            
                                                                             
                                   
     
  BTShared *btshared;
  Sharedsort *sharedsort;
  Sharedsort *sharedsort2;
  Snapshot snapshot;
  BufferUsage *bufferusage;
} BTLeader;

   
                                               
   
                                                                        
                
   
typedef struct BTBuildState
{
  bool isunique;
  bool havedead;
  Relation heap;
  BTSpool *spool;

     
                                                                             
                                                                          
     
  BTSpool *spool2;
  double indtuples;

     
                                                                            
                                                                  
                                                                      
     
  BTLeader *btleader;
} BTBuildState;

   
                                                                     
                               
   
                                                                       
                                                                    
                                                                        
                                                                        
                                                                      
                                                                      
                                                                      
                                          
   
typedef struct BTPageState
{
  Page btps_page;                                                 
  BlockNumber btps_blkno;                                           
  IndexTuple btps_minkey;                                                      
  OffsetNumber btps_lastoff;                                  
  uint32 btps_level;                                        
  Size btps_full;                                                              
  struct BTPageState *btps_next;                                   
} BTPageState;

   
                                                  
   
typedef struct BTWriteState
{
  Relation heap;
  Relation index;
  BTScanInsert inskey;                                           
  bool btws_use_wal;                                      
  BlockNumber btws_pages_alloced;                        
  BlockNumber btws_pages_written;                          
  Page btws_zeropage;                                               
} BTWriteState;

static double
_bt_spools_heapscan(Relation heap, Relation index, BTBuildState *buildstate, IndexInfo *indexInfo);
static void
_bt_spooldestroy(BTSpool *btspool);
static void
_bt_spool(BTSpool *btspool, ItemPointer self, Datum *values, bool *isnull);
static void
_bt_leafbuild(BTSpool *btspool, BTSpool *btspool2);
static void
_bt_build_callback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state);
static Page
_bt_blnewpage(uint32 level);
static BTPageState *
_bt_pagestate(BTWriteState *wstate, uint32 level);
static void
_bt_slideleft(Page page);
static void
_bt_sortaddtup(Page page, Size itemsize, IndexTuple itup, OffsetNumber itup_off);
static void
_bt_buildadd(BTWriteState *wstate, BTPageState *state, IndexTuple itup);
static void
_bt_uppershutdown(BTWriteState *wstate, BTPageState *state);
static void
_bt_load(BTWriteState *wstate, BTSpool *btspool, BTSpool *btspool2);
static void
_bt_begin_parallel(BTBuildState *buildstate, bool isconcurrent, int request);
static void
_bt_end_parallel(BTLeader *btleader);
static Size
_bt_parallel_estimate_shared(Relation heap, Snapshot snapshot);
static double
_bt_parallel_heapscan(BTBuildState *buildstate, bool *brokenhotchain);
static void
_bt_leader_participate_as_worker(BTBuildState *buildstate);
static void
_bt_parallel_scan_and_sort(BTSpool *btspool, BTSpool *btspool2, BTShared *btshared, Sharedsort *sharedsort, Sharedsort *sharedsort2, int sortmem, bool progress);

   
                                         
   
IndexBuildResult *
btbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{
  IndexBuildResult *result;
  BTBuildState buildstate;
  double reltuples;

#ifdef BTREE_BUILD_STATS
  if (log_btree_build_stats)
  {
    ResetUsage();
  }
#endif                        

  buildstate.isunique = indexInfo->ii_Unique;
  buildstate.havedead = false;
  buildstate.heap = heap;
  buildstate.spool = NULL;
  buildstate.spool2 = NULL;
  buildstate.indtuples = 0;
  buildstate.btleader = NULL;

     
                                                                           
                                               
     
  if (RelationGetNumberOfBlocks(index) != 0)
  {
    elog(ERROR, "index \"%s\" already contains data", RelationGetRelationName(index));
  }

  reltuples = _bt_spools_heapscan(heap, index, &buildstate, indexInfo);

     
                                                                        
                                                                             
                                                                           
     
  _bt_leafbuild(buildstate.spool, buildstate.spool2);
  _bt_spooldestroy(buildstate.spool);
  if (buildstate.spool2)
  {
    _bt_spooldestroy(buildstate.spool2);
  }
  if (buildstate.btleader)
  {
    _bt_end_parallel(buildstate.btleader);
  }

  result = (IndexBuildResult *)palloc(sizeof(IndexBuildResult));

  result->heap_tuples = reltuples;
  result->index_tuples = buildstate.indtuples;

#ifdef BTREE_BUILD_STATS
  if (log_btree_build_stats)
  {
    ShowUsage("BTREE BUILD STATS");
    ResetUsage();
  }
#endif                        

  return result;
}

   
                                                                                
                                                                                
           
   
                                                                                
                                                                               
                                                                                
   
                                                    
   
static double
_bt_spools_heapscan(Relation heap, Relation index, BTBuildState *buildstate, IndexInfo *indexInfo)
{
  BTSpool *btspool = (BTSpool *)palloc0(sizeof(BTSpool));
  SortCoordinate coordinate = NULL;
  double reltuples = 0;

     
                                                                           
                                                                           
                                                                  
                                                  
     
  btspool->heap = heap;
  btspool->index = index;
  btspool->isunique = indexInfo->ii_Unique;

                             
  buildstate->spool = btspool;

                                       
  pgstat_progress_update_param(PROGRESS_CREATEIDX_SUBPHASE, PROGRESS_BTREE_PHASE_INDEXBUILD_TABLESCAN);

                                                            
  if (indexInfo->ii_ParallelWorkers > 0)
  {
    _bt_begin_parallel(buildstate, indexInfo->ii_Concurrent, indexInfo->ii_ParallelWorkers);
  }

     
                                                                     
                                                      
     
  if (buildstate->btleader)
  {
    coordinate = (SortCoordinate)palloc0(sizeof(SortCoordinateData));
    coordinate->isWorker = false;
    coordinate->nParticipants = buildstate->btleader->nparticipanttuplesorts;
    coordinate->sharedsort = buildstate->btleader->sharedsort;
  }

     
                                    
     
                                                                          
                                                                             
                                                                        
                                                           
                                   
     
                                                                     
                                                                          
                                                                   
                                                                          
                                                                     
                                                                           
                                                                        
                                                                         
                                                                            
                                                                             
                                                 
     
  buildstate->spool->sortstate = tuplesort_begin_index_btree(heap, index, buildstate->isunique, maintenance_work_mem, coordinate, false);

     
                                                                           
                                                                             
                                                                    
     
  if (indexInfo->ii_Unique)
  {
    BTSpool *btspool2 = (BTSpool *)palloc0(sizeof(BTSpool));
    SortCoordinate coordinate2 = NULL;

                                    
    btspool2->heap = heap;
    btspool2->index = index;
    btspool2->isunique = false;
                                 
    buildstate->spool2 = btspool2;

    if (buildstate->btleader)
    {
         
                                                    
                                                                  
                                          
         
      coordinate2 = (SortCoordinate)palloc0(sizeof(SortCoordinateData));
      coordinate2->isWorker = false;
      coordinate2->nParticipants = buildstate->btleader->nparticipanttuplesorts;
      coordinate2->sharedsort = buildstate->btleader->sharedsort2;
    }

       
                                                                      
                                         
       
    buildstate->spool2->sortstate = tuplesort_begin_index_btree(heap, index, false, work_mem, coordinate2, false);
  }

                                                            
  if (!buildstate->btleader)
  {
    reltuples = table_index_build_scan(heap, index, indexInfo, true, true, _bt_build_callback, (void *)buildstate, NULL);
  }
  else
  {
    reltuples = _bt_parallel_heapscan(buildstate, &indexInfo->ii_BrokenHotChain);
  }

     
                                                                         
                                          
     
  {
    const int index[] = {PROGRESS_CREATEIDX_TUPLES_TOTAL, PROGRESS_SCAN_BLOCKS_TOTAL, PROGRESS_SCAN_BLOCKS_DONE};
    const int64 val[] = {buildstate->indtuples, 0, 0};

    pgstat_progress_update_multi_param(3, index, val);
  }

                                         
  if (buildstate->spool2 && !buildstate->havedead)
  {
                                            
    _bt_spooldestroy(buildstate->spool2);
    buildstate->spool2 = NULL;
  }

  return reltuples;
}

   
                                                     
   
static void
_bt_spooldestroy(BTSpool *btspool)
{
  tuplesort_end(btspool->sortstate);
  pfree(btspool);
}

   
                                            
   
static void
_bt_spool(BTSpool *btspool, ItemPointer self, Datum *values, bool *isnull)
{
  tuplesort_putindextuplevalues(btspool->sortstate, btspool->index, self, values, isnull);
}

   
                                                          
                           
   
static void
_bt_leafbuild(BTSpool *btspool, BTSpool *btspool2)
{
  BTWriteState wstate;

#ifdef BTREE_BUILD_STATS
  if (log_btree_build_stats)
  {
    ShowUsage("BTREE BUILD (Spool) STATISTICS");
    ResetUsage();
  }
#endif                        

                        
  pgstat_progress_update_param(PROGRESS_CREATEIDX_SUBPHASE, PROGRESS_BTREE_PHASE_PERFORMSORT_1);
  tuplesort_performsort(btspool->sortstate);
  if (btspool2)
  {
    pgstat_progress_update_param(PROGRESS_CREATEIDX_SUBPHASE, PROGRESS_BTREE_PHASE_PERFORMSORT_2);
    tuplesort_performsort(btspool2->sortstate);
  }

  wstate.heap = btspool->heap;
  wstate.index = btspool->index;
  wstate.inskey = _bt_mkscankey(wstate.index, NULL);

     
                                                                         
                                                       
     
  wstate.btws_use_wal = XLogIsNeeded() && RelationNeedsWAL(wstate.index);

                            
  wstate.btws_pages_alloced = BTREE_METAPAGE + 1;
  wstate.btws_pages_written = 0;
  wstate.btws_zeropage = NULL;                   

  pgstat_progress_update_param(PROGRESS_CREATEIDX_SUBPHASE, PROGRESS_BTREE_PHASE_LEAF_LOAD);
  _bt_load(&wstate, btspool, btspool2);
}

   
                                                 
   
static void
_bt_build_callback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{
  BTBuildState *buildstate = (BTBuildState *)state;

     
                                                                           
                
     
  if (tupleIsAlive || buildstate->spool2 == NULL)
  {
    _bt_spool(buildstate->spool, &htup->t_self, values, isnull);
  }
  else
  {
                                         
    buildstate->havedead = true;
    _bt_spool(buildstate->spool2, &htup->t_self, values, isnull);
  }

  buildstate->indtuples += 1;
}

   
                                                                               
   
static Page
_bt_blnewpage(uint32 level)
{
  Page page;
  BTPageOpaque opaque;

  page = (Page)palloc(BLCKSZ);

                                                          
  _bt_pageinit(page, BLCKSZ);

                                  
  opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  opaque->btpo_prev = opaque->btpo_next = P_NONE;
  opaque->btpo.level = level;
  opaque->btpo_flags = (level > 0) ? 0 : BTP_LEAF;
  opaque->btpo_cycleid = 0;

                                                      
  ((PageHeader)page)->pd_lower += sizeof(ItemIdData);

  return page;
}

   
                                                                 
   
static void
_bt_blwritepage(BTWriteState *wstate, Page page, BlockNumber blkno)
{
                  
  if (wstate->btws_use_wal)
  {
                                                      
    log_newpage(&wstate->index->rd_node, MAIN_FORKNUM, blkno, page, true);
  }

     
                                                                       
                                                                     
                                                                          
                                                                          
                                     
     
  while (blkno > wstate->btws_pages_written)
  {
    if (!wstate->btws_zeropage)
    {
      wstate->btws_zeropage = (Page)palloc0(BLCKSZ);
    }
                                              
    smgrextend(RelationGetSmgr(wstate->index), MAIN_FORKNUM, wstate->btws_pages_written++, (char *)wstate->btws_zeropage, true);
  }

  PageSetChecksumInplace(page, blkno);

     
                                                                            
                                                                
     
  if (blkno == wstate->btws_pages_written)
  {
                               
    smgrextend(RelationGetSmgr(wstate->index), MAIN_FORKNUM, blkno, (char *)page, true);
    wstate->btws_pages_written++;
  }
  else
  {
                                                   
    smgrwrite(RelationGetSmgr(wstate->index), MAIN_FORKNUM, blkno, (char *)page, true);
  }

  pfree(page);
}

   
                                                                      
                                                  
   
static BTPageState *
_bt_pagestate(BTWriteState *wstate, uint32 level)
{
  BTPageState *state = (BTPageState *)palloc0(sizeof(BTPageState));

                                     
  state->btps_page = _bt_blnewpage(level);

                                     
  state->btps_blkno = wstate->btws_pages_alloced++;

  state->btps_minkey = NULL;
                                                             
  state->btps_lastoff = P_HIKEY;
  state->btps_level = level;
                                                                        
  if (level > 0)
  {
    state->btps_full = (BLCKSZ * (100 - BTREE_NONLEAF_FILLFACTOR) / 100);
  }
  else
  {
    state->btps_full = RelationGetTargetPageFreeSpace(wstate->index, BTREE_DEFAULT_FILLFACTOR);
  }
                            
  state->btps_next = NULL;

  return state;
}

   
                                                               
                                                                       
                                                                     
                     
   
static void
_bt_slideleft(Page page)
{
  OffsetNumber off;
  OffsetNumber maxoff;
  ItemId previi;
  ItemId thisii;

  if (!PageIsEmpty(page))
  {
    maxoff = PageGetMaxOffsetNumber(page);
    previi = PageGetItemId(page, P_HIKEY);
    for (off = P_FIRSTKEY; off <= maxoff; off = OffsetNumberNext(off))
    {
      thisii = PageGetItemId(page, off);
      *previi = *thisii;
      previi = thisii;
    }
    ((PageHeader)page)->pd_lower -= sizeof(ItemIdData);
  }
}

   
                                      
   
                                                                        
                                                                     
                                                                     
                                       
   
                                                                      
                                                                      
                                                                     
                                                               
   
static void
_bt_sortaddtup(Page page, Size itemsize, IndexTuple itup, OffsetNumber itup_off)
{
  BTPageOpaque opaque = (BTPageOpaque)PageGetSpecialPointer(page);
  IndexTupleData trunctuple;

  if (!P_ISLEAF(opaque) && itup_off == P_FIRSTKEY)
  {
    trunctuple = *itup;
    trunctuple.t_info = sizeof(IndexTupleData);
                                                   
    BTreeTupleSetNAtts(&trunctuple, 0);
    itup = &trunctuple;
    itemsize = sizeof(IndexTupleData);
  }

  if (PageAddItem(page, (Item)itup, itemsize, itup_off, false, false) == InvalidOffsetNumber)
  {
    elog(ERROR, "failed to add item to the index page");
  }
}

             
                                                    
   
                                                                             
                                                                           
                                                                      
                                           
   
                                       
   
                                                        
                                                        
                                                        
                             
                                                        
                          
                    
                                                        
                                               
                                                        
                                                  
                                                        
   
                                                                  
                                                                   
                                                                    
                                                                     
                                                                      
                                                                     
                                                                       
                                       
   
                                                               
             
   
static void
_bt_buildadd(BTWriteState *wstate, BTPageState *state, IndexTuple itup)
{
  Page npage;
  BlockNumber nblkno;
  OffsetNumber last_off;
  Size pgspc;
  Size itupsz;
  bool isleaf;

     
                                                                           
                                   
     
  CHECK_FOR_INTERRUPTS();

  npage = state->btps_page;
  nblkno = state->btps_blkno;
  last_off = state->btps_lastoff;

  pgspc = PageGetFreeSpace(npage);
  itupsz = IndexTupleSize(itup);
  itupsz = MAXALIGN(itupsz);
                                                                       
  isleaf = (state->btps_level == 0);

     
                                                                            
          
     
                                                                          
                                                                             
                                                                            
                                                                         
                                                                             
                                                                            
                                                                             
                                                                          
                                                                    
     
  if (unlikely(itupsz > BTMaxItemSize(npage)))
  {
    _bt_check_third_page(wstate->index, wstate->heap, isleaf, npage, itup);
  }

     
                                                                             
                                                                          
     
                                                                         
                                                                             
                                                                      
                                                                          
                                                
     
                                                                        
                                                                             
                                                                           
                                                                         
                                                                             
                                                                             
                                
     
  if (pgspc < itupsz + (isleaf ? MAXALIGN(sizeof(ItemPointerData)) : 0) || (pgspc < state->btps_full && last_off > P_FIRSTKEY))
  {
       
                                             
       
    Page opage = npage;
    BlockNumber oblkno = nblkno;
    ItemId ii;
    ItemId hii;
    IndexTuple oitup;

                                       
    npage = _bt_blnewpage(state->btps_level);

                                       
    nblkno = wstate->btws_pages_alloced++;

       
                                                                     
                                                                           
                                                                       
                                                                         
             
       
    Assert(last_off > P_FIRSTKEY);
    ii = PageGetItemId(opage, last_off);
    oitup = (IndexTuple)PageGetItem(opage, ii);
    _bt_sortaddtup(npage, ItemIdGetLength(ii), oitup, P_FIRSTKEY);

       
                                                                         
                                                                     
                                                                           
                                                                        
                                                                           
                                                                           
                                     
       
    hii = PageGetItemId(opage, P_HIKEY);
    *hii = *ii;
    ItemIdSetUnused(ii);                
    ((PageHeader)opage)->pd_lower -= sizeof(ItemIdData);

    if (isleaf)
    {
      IndexTuple lastleft;
      IndexTuple truncated;
      Size truncsz;

         
                                                                     
                                                                       
                                                                      
                                                                     
                      
         
                                                                        
                                                                       
                                                                      
                                                                         
                                                              
                                                                  
                                                                       
                       
         
                                                                      
                                                                       
                                                                        
                                                                  
                 
         
                                                                         
                                                                         
                                                                         
                                                                       
                
         
      ii = PageGetItemId(opage, OffsetNumberPrev(last_off));
      lastleft = (IndexTuple)PageGetItem(opage, ii);

      truncated = _bt_truncate(wstate->index, lastleft, oitup, wstate->inskey);
      truncsz = IndexTupleSize(truncated);
      PageIndexTupleDelete(opage, P_HIKEY);
      _bt_sortaddtup(opage, truncsz, truncated, P_HIKEY);
      pfree(truncated);

                                                                 
      hii = PageGetItemId(opage, P_HIKEY);
      oitup = (IndexTuple)PageGetItem(opage, hii);
    }

       
                                                                       
                                                                         
              
       
    if (state->btps_next == NULL)
    {
      state->btps_next = _bt_pagestate(wstate, state->btps_level + 1);
    }

    Assert((BTreeTupleGetNAtts(state->btps_minkey, wstate->index) <= IndexRelationGetNumberOfKeyAttributes(wstate->index) && BTreeTupleGetNAtts(state->btps_minkey, wstate->index) > 0) || P_LEFTMOST((BTPageOpaque)PageGetSpecialPointer(opage)));
    Assert(BTreeTupleGetNAtts(state->btps_minkey, wstate->index) == 0 || !P_LEFTMOST((BTPageOpaque)PageGetSpecialPointer(opage)));
    BTreeInnerTupleSetDownLink(state->btps_minkey, oblkno);
    _bt_buildadd(wstate, state->btps_next, state->btps_minkey);
    pfree(state->btps_minkey);

       
                                                                          
                                         
       
    state->btps_minkey = CopyIndexTuple(oitup);

       
                                             
       
    {
      BTPageOpaque oopaque = (BTPageOpaque)PageGetSpecialPointer(opage);
      BTPageOpaque nopaque = (BTPageOpaque)PageGetSpecialPointer(npage);

      oopaque->btpo_next = nblkno;
      nopaque->btpo_prev = oblkno;
      nopaque->btpo_next = P_NONE;                
    }

       
                                                                           
                                     
       
    _bt_blwritepage(wstate, opage, oblkno);

       
                                           
       
    last_off = P_FIRSTKEY;
  }

     
                                                                            
                                                                             
                                        
     
                                                                             
                                                                          
                                                                         
                                                                        
                                                             
     
  if (last_off == P_HIKEY)
  {
    Assert(state->btps_minkey == NULL);
    state->btps_minkey = CopyIndexTuple(itup);
                                                             
    BTreeTupleSetNAtts(state->btps_minkey, 0);
  }

     
                                             
     
  last_off = OffsetNumberNext(last_off);
  _bt_sortaddtup(npage, itupsz, itup, last_off);

  state->btps_page = npage;
  state->btps_blkno = nblkno;
  state->btps_lastoff = last_off;
}

   
                                           
   
static void
_bt_uppershutdown(BTWriteState *wstate, BTPageState *state)
{
  BTPageState *s;
  BlockNumber rootblkno = P_NONE;
  uint32 rootlevel = 0;
  Page metapage;

     
                                                                       
     
  for (s = state; s != NULL; s = s->btps_next)
  {
    BlockNumber blkno;
    BTPageOpaque opaque;

    blkno = s->btps_blkno;
    opaque = (BTPageOpaque)PageGetSpecialPointer(s->btps_page);

       
                                                                 
       
                                                                         
                                                                           
                                                                      
                                                            
       
    if (s->btps_next == NULL)
    {
      opaque->btpo_flags |= BTP_ROOT;
      rootblkno = blkno;
      rootlevel = s->btps_level;
    }
    else
    {
      Assert((BTreeTupleGetNAtts(s->btps_minkey, wstate->index) <= IndexRelationGetNumberOfKeyAttributes(wstate->index) && BTreeTupleGetNAtts(s->btps_minkey, wstate->index) > 0) || P_LEFTMOST(opaque));
      Assert(BTreeTupleGetNAtts(s->btps_minkey, wstate->index) == 0 || !P_LEFTMOST(opaque));
      BTreeInnerTupleSetDownLink(s->btps_minkey, blkno);
      _bt_buildadd(wstate, s->btps_next, s->btps_minkey);
      pfree(s->btps_minkey);
      s->btps_minkey = NULL;
    }

       
                                                                        
                                                      
       
    _bt_slideleft(s->btps_page);
    _bt_blwritepage(wstate, s->btps_page, s->btps_blkno);
    s->btps_page = NULL;                                    
  }

     
                                                                         
                                                                             
                                                                             
                                                         
     
  metapage = (Page)palloc(BLCKSZ);
  _bt_initmetapage(metapage, rootblkno, rootlevel);
  _bt_blwritepage(wstate, metapage, BTREE_METAPAGE);
}

   
                                                                        
                 
   
static void
_bt_load(BTWriteState *wstate, BTSpool *btspool, BTSpool *btspool2)
{
  BTPageState *state = NULL;
  bool merge = (btspool2 != NULL);
  IndexTuple itup, itup2 = NULL;
  bool load1;
  TupleDesc tupdes = RelationGetDescr(wstate->index);
  int i, keysz = IndexRelationGetNumberOfKeyAttributes(wstate->index);
  SortSupport sortKeys;
  int64 tuples_done = 0;

  if (merge)
  {
       
                                                                    
                             
       

                                  
    itup = tuplesort_getindextuple(btspool->sortstate, true);
    itup2 = tuplesort_getindextuple(btspool2->sortstate, true);

                                                  
    sortKeys = (SortSupport)palloc0(keysz * sizeof(SortSupportData));

    for (i = 0; i < keysz; i++)
    {
      SortSupport sortKey = sortKeys + i;
      ScanKey scanKey = wstate->inskey->scankeys + i;
      int16 strategy;

      sortKey->ssup_cxt = CurrentMemoryContext;
      sortKey->ssup_collation = scanKey->sk_collation;
      sortKey->ssup_nulls_first = (scanKey->sk_flags & SK_BT_NULLS_FIRST) != 0;
      sortKey->ssup_attno = scanKey->sk_attno;
                                              
      sortKey->abbreviate = false;

      AssertState(sortKey->ssup_attno != 0);

      strategy = (scanKey->sk_flags & SK_BT_DESC) != 0 ? BTGreaterStrategyNumber : BTLessStrategyNumber;

      PrepareSortSupportFromIndexRel(wstate->index, strategy, sortKey);
    }

    for (;;)
    {
      load1 = true;                          
      if (itup2 == NULL)
      {
        if (itup == NULL)
        {
          break;
        }
      }
      else if (itup != NULL)
      {
        int32 compare = 0;

        for (i = 1; i <= keysz; i++)
        {
          SortSupport entry;
          Datum attrDatum1, attrDatum2;
          bool isNull1, isNull2;

          entry = sortKeys + i - 1;
          attrDatum1 = index_getattr(itup, i, tupdes, &isNull1);
          attrDatum2 = index_getattr(itup2, i, tupdes, &isNull2);

          compare = ApplySortComparator(attrDatum1, isNull1, attrDatum2, isNull2, entry);
          if (compare > 0)
          {
            load1 = false;
            break;
          }
          else if (compare < 0)
          {
            break;
          }
        }

           
                                                                     
                                                                       
                                                                   
                                                    
           
        if (compare == 0)
        {
          compare = ItemPointerCompare(&itup->t_tid, &itup2->t_tid);
          Assert(compare != 0);
          if (compare > 0)
          {
            load1 = false;
          }
        }
      }
      else
      {
        load1 = false;
      }

                                                            
      if (state == NULL)
      {
        state = _bt_pagestate(wstate, 0);
      }

      if (load1)
      {
        _bt_buildadd(wstate, state, itup);
        itup = tuplesort_getindextuple(btspool->sortstate, true);
      }
      else
      {
        _bt_buildadd(wstate, state, itup2);
        itup2 = tuplesort_getindextuple(btspool2->sortstate, true);
      }

                           
      pgstat_progress_update_param(PROGRESS_CREATEIDX_TUPLES_DONE, ++tuples_done);
    }
    pfree(sortKeys);
  }
  else
  {
                              
    while ((itup = tuplesort_getindextuple(btspool->sortstate, true)) != NULL)
    {
                                                            
      if (state == NULL)
      {
        state = _bt_pagestate(wstate, 0);
      }

      _bt_buildadd(wstate, state, itup);

                           
      pgstat_progress_update_param(PROGRESS_CREATEIDX_TUPLES_DONE, ++tuples_done);
    }
  }

                                                     
  _bt_uppershutdown(wstate, state);

     
                                                                           
                                                                           
                                                                       
     
                                                                            
                                                                         
                                                                             
                                                                     
                                                                          
                                                                      
                                                                        
                                                                            
             
     
  if (RelationNeedsWAL(wstate->index))
  {
    smgrimmedsync(RelationGetSmgr(wstate->index), MAIN_FORKNUM);
  }
}

   
                                                           
   
                                                                        
                                                                         
                                 
   
                                                                     
   
                                                                        
   
                                                                           
                                                                         
                                                                        
                                                                   
   
static void
_bt_begin_parallel(BTBuildState *buildstate, bool isconcurrent, int request)
{
  ParallelContext *pcxt;
  int scantuplesortstates;
  Snapshot snapshot;
  Size estbtshared;
  Size estsort;
  BTShared *btshared;
  Sharedsort *sharedsort;
  Sharedsort *sharedsort2;
  BTSpool *btspool = buildstate->spool;
  BTLeader *btleader = (BTLeader *)palloc0(sizeof(BTLeader));
  BufferUsage *bufferusage;
  bool leaderparticipates = true;
  int querylen;

#ifdef DISABLE_LEADER_PARTICIPATION
  leaderparticipates = false;
#endif

     
                                                                         
           
     
  EnterParallelMode();
  Assert(request > 0);
  pcxt = CreateParallelContext("postgres", "_bt_parallel_build_main", request);

  scantuplesortstates = leaderparticipates ? request + 1 : request;

     
                                                                             
                                                                         
                                                                        
                                                                            
                             
     
  if (!isconcurrent)
  {
    snapshot = SnapshotAny;
  }
  else
  {
    snapshot = RegisterSnapshot(GetTransactionSnapshot());
  }

     
                                                                        
                                                
     
  estbtshared = _bt_parallel_estimate_shared(btspool->heap, snapshot);
  shm_toc_estimate_chunk(&pcxt->estimator, estbtshared);
  estsort = tuplesort_estimate_shared(scantuplesortstates);
  shm_toc_estimate_chunk(&pcxt->estimator, estsort);

     
                                                                            
                                                                        
     
  if (!btspool->isunique)
  {
    shm_toc_estimate_keys(&pcxt->estimator, 2);
  }
  else
  {
    shm_toc_estimate_chunk(&pcxt->estimator, estsort);
    shm_toc_estimate_keys(&pcxt->estimator, 3);
  }

     
                                                                  
     
                                                                          
                                                                             
                         
     
  shm_toc_estimate_chunk(&pcxt->estimator, mul_size(sizeof(BufferUsage), pcxt->nworkers));
  shm_toc_estimate_keys(&pcxt->estimator, 1);

                                                       
  if (debug_query_string)
  {
    querylen = strlen(debug_query_string);
    shm_toc_estimate_chunk(&pcxt->estimator, querylen + 1);
    shm_toc_estimate_keys(&pcxt->estimator, 1);
  }
  else
  {
    querylen = 0;                          
  }

                                                                       
  InitializeParallelDSM(pcxt);

                                                                   
  if (pcxt->seg == NULL)
  {
    if (IsMVCCSnapshot(snapshot))
    {
      UnregisterSnapshot(snapshot);
    }
    DestroyParallelContext(pcxt);
    ExitParallelMode();
    return;
  }

                                                             
  btshared = (BTShared *)shm_toc_allocate(pcxt->toc, estbtshared);
                                  
  btshared->heaprelid = RelationGetRelid(btspool->heap);
  btshared->indexrelid = RelationGetRelid(btspool->index);
  btshared->isunique = btspool->isunique;
  btshared->isconcurrent = isconcurrent;
  btshared->scantuplesortstates = scantuplesortstates;
  ConditionVariableInit(&btshared->workersdonecv);
  SpinLockInit(&btshared->mutex);
                                
  btshared->nparticipantsdone = 0;
  btshared->reltuples = 0.0;
  btshared->havedead = false;
  btshared->indtuples = 0.0;
  btshared->brokenhotchain = false;
  table_parallelscan_initialize(btspool->heap, ParallelTableScanFromBTShared(btshared), snapshot);

     
                                                                        
                                                            
     
  sharedsort = (Sharedsort *)shm_toc_allocate(pcxt->toc, estsort);
  tuplesort_initialize_shared(sharedsort, scantuplesortstates, pcxt->seg);

  shm_toc_insert(pcxt->toc, PARALLEL_KEY_BTREE_SHARED, btshared);
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_TUPLESORT, sharedsort);

                                                                        
  if (!btspool->isunique)
  {
    sharedsort2 = NULL;
  }
  else
  {
       
                                                                     
                                                                      
                
       
    sharedsort2 = (Sharedsort *)shm_toc_allocate(pcxt->toc, estsort);
    tuplesort_initialize_shared(sharedsort2, scantuplesortstates, pcxt->seg);

    shm_toc_insert(pcxt->toc, PARALLEL_KEY_TUPLESORT_SPOOL2, sharedsort2);
  }

                                      
  if (debug_query_string)
  {
    char *sharedquery;

    sharedquery = (char *)shm_toc_allocate(pcxt->toc, querylen + 1);
    memcpy(sharedquery, debug_query_string, querylen + 1);
    shm_toc_insert(pcxt->toc, PARALLEL_KEY_QUERY_TEXT, sharedquery);
  }

                                                                           
  bufferusage = shm_toc_allocate(pcxt->toc, mul_size(sizeof(BufferUsage), pcxt->nworkers));
  shm_toc_insert(pcxt->toc, PARALLEL_KEY_BUFFER_USAGE, bufferusage);

                                                       
  LaunchParallelWorkers(pcxt);
  btleader->pcxt = pcxt;
  btleader->nparticipanttuplesorts = pcxt->nworkers_launched;
  if (leaderparticipates)
  {
    btleader->nparticipanttuplesorts++;
  }
  btleader->btshared = btshared;
  btleader->sharedsort = sharedsort;
  btleader->sharedsort2 = sharedsort2;
  btleader->snapshot = snapshot;
  btleader->bufferusage = bufferusage;

                                                                            
  if (pcxt->nworkers_launched == 0)
  {
    _bt_end_parallel(btleader);
    return;
  }

                                                                    
  buildstate->btleader = btleader;

                                
  if (leaderparticipates)
  {
    _bt_leader_participate_as_worker(buildstate);
  }

     
                                                                         
                                                                
     
  WaitForParallelWorkersToAttach(pcxt);
}

   
                                                                       
   
static void
_bt_end_parallel(BTLeader *btleader)
{
  int i;

                                 
  WaitForParallelWorkersToFinish(btleader->pcxt);

     
                                                                        
                                               
     
  for (i = 0; i < btleader->pcxt->nworkers_launched; i++)
  {
    InstrAccumParallelQuery(&btleader->bufferusage[i]);
  }

                                                             
  if (IsMVCCSnapshot(btleader->snapshot))
  {
    UnregisterSnapshot(btleader->snapshot);
  }
  DestroyParallelContext(btleader->pcxt);
  ExitParallelMode();
}

   
                                                                        
                                                                       
   
static Size
_bt_parallel_estimate_shared(Relation heap, Snapshot snapshot)
{
                                                           
  return add_size(BUFFERALIGN(sizeof(BTShared)), table_parallelscan_estimate(heap, snapshot));
}

   
                                             
   
                                                                        
                                                                         
                                                                      
   
                                                                      
                                                                     
   
                                                    
   
static double
_bt_parallel_heapscan(BTBuildState *buildstate, bool *brokenhotchain)
{
  BTShared *btshared = buildstate->btleader->btshared;
  int nparticipanttuplesorts;
  double reltuples;

  nparticipanttuplesorts = buildstate->btleader->nparticipanttuplesorts;
  for (;;)
  {
    SpinLockAcquire(&btshared->mutex);
    if (btshared->nparticipantsdone == nparticipanttuplesorts)
    {
      buildstate->havedead = btshared->havedead;
      buildstate->indtuples = btshared->indtuples;
      *brokenhotchain = btshared->brokenhotchain;
      reltuples = btshared->reltuples;
      SpinLockRelease(&btshared->mutex);
      break;
    }
    SpinLockRelease(&btshared->mutex);

    ConditionVariableSleep(&btshared->workersdonecv, WAIT_EVENT_PARALLEL_CREATE_INDEX_SCAN);
  }

  ConditionVariableCancelSleep();

  return reltuples;
}

   
                                                    
   
static void
_bt_leader_participate_as_worker(BTBuildState *buildstate)
{
  BTLeader *btleader = buildstate->btleader;
  BTSpool *leaderworker;
  BTSpool *leaderworker2;
  int sortmem;

                                                    
  leaderworker = (BTSpool *)palloc0(sizeof(BTSpool));
  leaderworker->heap = buildstate->spool->heap;
  leaderworker->index = buildstate->spool->index;
  leaderworker->isunique = buildstate->spool->isunique;

                                            
  if (!btleader->btshared->isunique)
  {
    leaderworker2 = NULL;
  }
  else
  {
                                                                  
    leaderworker2 = (BTSpool *)palloc0(sizeof(BTSpool));

                                                 
    leaderworker2->heap = leaderworker->heap;
    leaderworker2->index = leaderworker->index;
    leaderworker2->isunique = false;
  }

     
                                                                            
                                                                       
                                                    
     
  sortmem = maintenance_work_mem / btleader->nparticipanttuplesorts;

                                               
  _bt_parallel_scan_and_sort(leaderworker, leaderworker2, btleader->btshared, btleader->sharedsort, btleader->sharedsort2, sortmem, true);

#ifdef BTREE_BUILD_STATS
  if (log_btree_build_stats)
  {
    ShowUsage("BTREE BUILD (Leader Partial Spool) STATISTICS");
    ResetUsage();
  }
#endif                        
}

   
                                                    
   
void
_bt_parallel_build_main(dsm_segment *seg, shm_toc *toc)
{
  char *sharedquery;
  BTSpool *btspool;
  BTSpool *btspool2;
  BTShared *btshared;
  Sharedsort *sharedsort;
  Sharedsort *sharedsort2;
  Relation heapRel;
  Relation indexRel;
  LOCKMODE heapLockmode;
  LOCKMODE indexLockmode;
  BufferUsage *bufferusage;
  int sortmem;

#ifdef BTREE_BUILD_STATS
  if (log_btree_build_stats)
  {
    ResetUsage();
  }
#endif                        

                                                           
  sharedquery = shm_toc_lookup(toc, PARALLEL_KEY_QUERY_TEXT, true);
  debug_query_string = sharedquery;

                                           
  pgstat_report_activity(STATE_RUNNING, debug_query_string);

                                   
  btshared = shm_toc_lookup(toc, PARALLEL_KEY_BTREE_SHARED, false);

                                                                       
  if (!btshared->isconcurrent)
  {
    heapLockmode = ShareLock;
    indexLockmode = AccessExclusiveLock;
  }
  else
  {
    heapLockmode = ShareUpdateExclusiveLock;
    indexLockmode = RowExclusiveLock;
  }

                                    
  heapRel = table_open(btshared->heaprelid, heapLockmode);
  indexRel = index_open(btshared->indexrelid, indexLockmode);

                                     
  btspool = (BTSpool *)palloc0(sizeof(BTSpool));
  btspool->heap = heapRel;
  btspool->index = indexRel;
  btspool->isunique = btshared->isunique;

                                                   
  sharedsort = shm_toc_lookup(toc, PARALLEL_KEY_TUPLESORT, false);
  tuplesort_attach_shared(sharedsort, seg);
  if (!btshared->isunique)
  {
    btspool2 = NULL;
    sharedsort2 = NULL;
  }
  else
  {
                                                                  
    btspool2 = (BTSpool *)palloc0(sizeof(BTSpool));

                                                 
    btspool2->heap = btspool->heap;
    btspool2->index = btspool->index;
    btspool2->isunique = false;
                                                     
    sharedsort2 = shm_toc_lookup(toc, PARALLEL_KEY_TUPLESORT_SPOOL2, false);
    tuplesort_attach_shared(sharedsort2, seg);
  }

                                                               
  InstrStartParallelQuery();

                                                       
  sortmem = maintenance_work_mem / btshared->scantuplesortstates;
  _bt_parallel_scan_and_sort(btspool, btspool2, btshared, sharedsort, sharedsort2, sortmem, false);

                                                     
  bufferusage = shm_toc_lookup(toc, PARALLEL_KEY_BUFFER_USAGE, false);
  InstrEndParallelQuery(&bufferusage[ParallelWorkerNumber]);

#ifdef BTREE_BUILD_STATS
  if (log_btree_build_stats)
  {
    ShowUsage("BTREE BUILD (Worker Partial Spool) STATISTICS");
    ResetUsage();
  }
#endif                        

  index_close(indexRel, indexLockmode);
  table_close(heapRel, heapLockmode);
}

   
                                                  
   
                                                                         
                                                                          
                                                                 
   
                                                                      
                     
   
                                                                         
   
static void
_bt_parallel_scan_and_sort(BTSpool *btspool, BTSpool *btspool2, BTShared *btshared, Sharedsort *sharedsort, Sharedsort *sharedsort2, int sortmem, bool progress)
{
  SortCoordinate coordinate;
  BTBuildState buildstate;
  TableScanDesc scan;
  double reltuples;
  IndexInfo *indexInfo;

                                                     
  coordinate = palloc0(sizeof(SortCoordinateData));
  coordinate->isWorker = true;
  coordinate->nParticipants = -1;
  coordinate->sharedsort = sharedsort;

                                 
  btspool->sortstate = tuplesort_begin_index_btree(btspool->heap, btspool->index, btspool->isunique, sortmem, coordinate, false);

     
                                                                      
                                                             
     
  if (btspool2)
  {
    SortCoordinate coordinate2;

       
                                                                      
                                                                     
                                                                      
                               
       
    coordinate2 = palloc0(sizeof(SortCoordinateData));
    coordinate2->isWorker = true;
    coordinate2->nParticipants = -1;
    coordinate2->sharedsort = sharedsort2;
    btspool2->sortstate = tuplesort_begin_index_btree(btspool->heap, btspool->index, false, Min(sortmem, work_mem), coordinate2, false);
  }

                                                   
  buildstate.isunique = btshared->isunique;
  buildstate.havedead = false;
  buildstate.heap = btspool->heap;
  buildstate.spool = btspool;
  buildstate.spool2 = btspool2;
  buildstate.indtuples = 0;
  buildstate.btleader = NULL;

                          
  indexInfo = BuildIndexInfo(btspool->index);
  indexInfo->ii_Concurrent = btshared->isconcurrent;
  scan = table_beginscan_parallel(btspool->heap, ParallelTableScanFromBTShared(btshared));
  reltuples = table_index_build_scan(btspool->heap, btspool->index, indexInfo, true, progress, _bt_build_callback, (void *)&buildstate, scan);

                                              
  if (progress)
  {
    pgstat_progress_update_param(PROGRESS_CREATEIDX_SUBPHASE, PROGRESS_BTREE_PHASE_PERFORMSORT_1);
  }
  tuplesort_performsort(btspool->sortstate);
  if (btspool2)
  {
    if (progress)
    {
      pgstat_progress_update_param(PROGRESS_CREATEIDX_SUBPHASE, PROGRESS_BTREE_PHASE_PERFORMSORT_2);
    }
    tuplesort_performsort(btspool2->sortstate);
  }

     
                                                                           
                
     
  SpinLockAcquire(&btshared->mutex);
  btshared->nparticipantsdone++;
  btshared->reltuples += reltuples;
  if (buildstate.havedead)
  {
    btshared->havedead = true;
  }
  btshared->indtuples += buildstate.indtuples;
  if (indexInfo->ii_BrokenHotChain)
  {
    btshared->brokenhotchain = true;
  }
  SpinLockRelease(&btshared->mutex);

                     
  ConditionVariableSignal(&btshared->workersdonecv);

                                         
  tuplesort_end(btspool->sortstate);
  if (btspool2)
  {
    tuplesort_end(btspool2->sortstate);
  }
}
