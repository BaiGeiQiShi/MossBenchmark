                                                                            
   
               
                                                          
   
                                                       
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/spgist_private.h"
#include "access/spgxlog.h"
#include "access/tableam.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef struct
{
  SpGistState spgstate;                             
  int64 indtuples;                                          
  MemoryContext tmpCtx;                                  
} SpGistBuildState;

                                                                      
static void
spgistBuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{
  SpGistBuildState *buildstate = (SpGistBuildState *)state;
  MemoryContext oldCtx;

                                                           
  oldCtx = MemoryContextSwitchTo(buildstate->tmpCtx);

     
                                                                           
                                                                           
                                                                            
                                  
     
  while (!spgdoinsert(index, &buildstate->spgstate, &htup->t_self, *values, *isnull))
  {
    MemoryContextReset(buildstate->tmpCtx);
  }

                                
  buildstate->indtuples += 1;

  MemoryContextSwitchTo(oldCtx);
  MemoryContextReset(buildstate->tmpCtx);
}

   
                           
   
IndexBuildResult *
spgbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{
  IndexBuildResult *result;
  double reltuples;
  SpGistBuildState buildstate;
  Buffer metabuffer, rootbuffer, nullbuffer;

  if (RelationGetNumberOfBlocks(index) != 0)
  {
    elog(ERROR, "index \"%s\" already contains data", RelationGetRelationName(index));
  }

     
                                             
     
  metabuffer = SpGistNewBuffer(index);
  rootbuffer = SpGistNewBuffer(index);
  nullbuffer = SpGistNewBuffer(index);

  Assert(BufferGetBlockNumber(metabuffer) == SPGIST_METAPAGE_BLKNO);
  Assert(BufferGetBlockNumber(rootbuffer) == SPGIST_ROOT_BLKNO);
  Assert(BufferGetBlockNumber(nullbuffer) == SPGIST_NULL_BLKNO);

  START_CRIT_SECTION();

  SpGistInitMetapage(BufferGetPage(metabuffer));
  MarkBufferDirty(metabuffer);
  SpGistInitBuffer(rootbuffer, SPGIST_LEAF);
  MarkBufferDirty(rootbuffer);
  SpGistInitBuffer(nullbuffer, SPGIST_LEAF | SPGIST_NULLS);
  MarkBufferDirty(nullbuffer);

  END_CRIT_SECTION();

  UnlockReleaseBuffer(metabuffer);
  UnlockReleaseBuffer(rootbuffer);
  UnlockReleaseBuffer(nullbuffer);

     
                                                 
     
  initSpGistState(&buildstate.spgstate, index);
  buildstate.spgstate.isBuild = true;
  buildstate.indtuples = 0;

  buildstate.tmpCtx = AllocSetContextCreate(CurrentMemoryContext, "SP-GiST build temporary context", ALLOCSET_DEFAULT_SIZES);

  reltuples = table_index_build_scan(heap, index, indexInfo, true, true, spgistBuildCallback, (void *)&buildstate, NULL);

  MemoryContextDelete(buildstate.tmpCtx);

  SpGistUpdateMetaPage(index);

     
                                                                             
                                               
     
  if (RelationNeedsWAL(index))
  {
    log_newpage_range(index, MAIN_FORKNUM, 0, RelationGetNumberOfBlocks(index), true);
  }

  result = (IndexBuildResult *)palloc0(sizeof(IndexBuildResult));
  result->heap_tuples = reltuples;
  result->index_tuples = buildstate.indtuples;

  return result;
}

   
                                                          
   
void
spgbuildempty(Relation index)
{
  Page page;

                           
  page = (Page)palloc(BLCKSZ);
  SpGistInitMetapage(page);

     
                                                                   
                                                                         
                                                                           
                                                                         
               
     
  PageSetChecksumInplace(page, SPGIST_METAPAGE_BLKNO);
  smgrwrite(RelationGetSmgr(index), INIT_FORKNUM, SPGIST_METAPAGE_BLKNO, (char *)page, true);
  log_newpage(&(RelationGetSmgr(index))->smgr_rnode.node, INIT_FORKNUM, SPGIST_METAPAGE_BLKNO, page, true);

                                   
  SpGistInitPage(page, SPGIST_LEAF);

  PageSetChecksumInplace(page, SPGIST_ROOT_BLKNO);
  smgrwrite(RelationGetSmgr(index), INIT_FORKNUM, SPGIST_ROOT_BLKNO, (char *)page, true);
  log_newpage(&(RelationGetSmgr(index))->smgr_rnode.node, INIT_FORKNUM, SPGIST_ROOT_BLKNO, page, true);

                                               
  SpGistInitPage(page, SPGIST_LEAF | SPGIST_NULLS);

  PageSetChecksumInplace(page, SPGIST_NULL_BLKNO);
  smgrwrite(RelationGetSmgr(index), INIT_FORKNUM, SPGIST_NULL_BLKNO, (char *)page, true);
  log_newpage(&(RelationGetSmgr(index))->smgr_rnode.node, INIT_FORKNUM, SPGIST_NULL_BLKNO, page, true);

     
                                                                            
                                                                         
                                                                      
     
  smgrimmedsync(RelationGetSmgr(index), INIT_FORKNUM);
}

   
                                              
   
bool
spginsert(Relation index, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{
  SpGistState spgstate;
  MemoryContext oldCtx;
  MemoryContext insertCtx;

  insertCtx = AllocSetContextCreate(CurrentMemoryContext, "SP-GiST insert temporary context", ALLOCSET_DEFAULT_SIZES);
  oldCtx = MemoryContextSwitchTo(insertCtx);

  initSpGistState(&spgstate, index);

     
                                                                        
                                                                             
                                                                         
                                                                  
     
  while (!spgdoinsert(index, &spgstate, ht_ctid, *values, *isnull))
  {
    MemoryContextReset(insertCtx);
    initSpGistState(&spgstate, index);
  }

  SpGistUpdateMetaPage(index);

  MemoryContextSwitchTo(oldCtx);
  MemoryContextDelete(insertCtx);

                                                          
  return false;
}
