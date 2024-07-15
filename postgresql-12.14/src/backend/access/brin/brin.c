   
          
                                                
   
                                                   
   
                                                                         
                                                                        
   
                  
                                    
   
        
                                                          
   
#include "postgres.h"

#include "access/brin.h"
#include "access/brin_page.h"
#include "access/brin_pageops.h"
#include "access/brin_xlog.h"
#include "access/relation.h"
#include "access/reloptions.h"
#include "access/relscan.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/xloginsert.h"
#include "catalog/index.h"
#include "catalog/pg_am.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/memutils.h"
#include "utils/rel.h"

   
                                                                        
                                                
   
typedef struct BrinBuildState
{
  Relation bs_irel;
  int bs_numtuples;
  Buffer bs_currentInsertBuf;
  BlockNumber bs_pagesPerRange;
  BlockNumber bs_currRangeStart;
  BrinRevmap *bs_rmAccess;
  BrinDesc *bs_bdesc;
  BrinMemTuple *bs_dtuple;
} BrinBuildState;

   
                                              
   
typedef struct BrinOpaque
{
  BlockNumber bo_pagesPerRange;
  BrinRevmap *bo_rmAccess;
  BrinDesc *bo_bdesc;
} BrinOpaque;

#define BRIN_ALL_BLOCKRANGES InvalidBlockNumber

static BrinBuildState *
initialize_brin_buildstate(Relation idxRel, BrinRevmap *revmap, BlockNumber pagesPerRange);
static void
terminate_brin_buildstate(BrinBuildState *state);
static void
brinsummarize(Relation index, Relation heapRel, BlockNumber pageRange, bool include_partial, double *numSummarized, double *numExisting);
static void
form_and_insert_tuple(BrinBuildState *state);
static void
union_tuples(BrinDesc *bdesc, BrinMemTuple *a, BrinTuple *b);
static void
brin_vacuum_scan(Relation idxrel, BufferAccessStrategy strategy);

   
                                                                              
                  
   
Datum
brinhandler(PG_FUNCTION_ARGS)
{
  IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

  amroutine->amstrategies = 0;
  amroutine->amsupport = BRIN_LAST_OPTIONAL_PROCNUM;
  amroutine->amcanorder = false;
  amroutine->amcanorderbyop = false;
  amroutine->amcanbackward = false;
  amroutine->amcanunique = false;
  amroutine->amcanmulticol = true;
  amroutine->amoptionalkey = true;
  amroutine->amsearcharray = false;
  amroutine->amsearchnulls = true;
  amroutine->amstorage = true;
  amroutine->amclusterable = false;
  amroutine->ampredlocks = false;
  amroutine->amcanparallel = false;
  amroutine->amcaninclude = false;
  amroutine->amkeytype = InvalidOid;

  amroutine->ambuild = brinbuild;
  amroutine->ambuildempty = brinbuildempty;
  amroutine->aminsert = brininsert;
  amroutine->ambulkdelete = brinbulkdelete;
  amroutine->amvacuumcleanup = brinvacuumcleanup;
  amroutine->amcanreturn = NULL;
  amroutine->amcostestimate = brincostestimate;
  amroutine->amoptions = brinoptions;
  amroutine->amproperty = NULL;
  amroutine->ambuildphasename = NULL;
  amroutine->amvalidate = brinvalidate;
  amroutine->ambeginscan = brinbeginscan;
  amroutine->amrescan = brinrescan;
  amroutine->amgettuple = NULL;
  amroutine->amgetbitmap = bringetbitmap;
  amroutine->amendscan = brinendscan;
  amroutine->ammarkpos = NULL;
  amroutine->amrestrpos = NULL;
  amroutine->amestimateparallelscan = NULL;
  amroutine->aminitparallelscan = NULL;
  amroutine->amparallelrescan = NULL;

  PG_RETURN_POINTER(amroutine);
}

   
                                                                            
                                                                            
                                                                             
                                                         
   
                                                                               
               
   
                                                                              
                                              
   
bool
brininsert(Relation idxRel, Datum *values, bool *nulls, ItemPointer heaptid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{
  BlockNumber pagesPerRange;
  BlockNumber origHeapBlk;
  BlockNumber heapBlk;
  BrinDesc *bdesc = (BrinDesc *)indexInfo->ii_AmCache;
  BrinRevmap *revmap;
  Buffer buf = InvalidBuffer;
  MemoryContext tupcxt = NULL;
  MemoryContext oldcxt = CurrentMemoryContext;
  bool autosummarize = BrinGetAutoSummarize(idxRel);

  revmap = brinRevmapInitialize(idxRel, &pagesPerRange, NULL);

     
                                                                            
                                                         
     
  origHeapBlk = ItemPointerGetBlockNumber(heaptid);
  heapBlk = (origHeapBlk / pagesPerRange) * pagesPerRange;

  for (;;)
  {
    bool need_insert = false;
    OffsetNumber off;
    BrinTuple *brtup;
    BrinMemTuple *dtup;
    int keyno;

    CHECK_FOR_INTERRUPTS();

       
                                                                       
                                                                           
                                                
       
    if (autosummarize && heapBlk > 0 && heapBlk == origHeapBlk && ItemPointerGetOffsetNumber(heaptid) == FirstOffsetNumber)
    {
      BlockNumber lastPageRange = heapBlk - 1;
      BrinTuple *lastPageTuple;

      lastPageTuple = brinGetTupleForHeapBlock(revmap, lastPageRange, &buf, &off, NULL, BUFFER_LOCK_SHARE, NULL);
      if (!lastPageTuple)
      {
        bool recorded;

        recorded = AutoVacuumRequestWork(AVW_BRINSummarizeRange, RelationGetRelid(idxRel), lastPageRange);
        if (!recorded)
        {
          ereport(LOG, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("request for BRIN range summarization for index \"%s\" page %u was not recorded", RelationGetRelationName(idxRel), lastPageRange)));
        }
      }
      else
      {
        LockBuffer(buf, BUFFER_LOCK_UNLOCK);
      }
    }

    brtup = brinGetTupleForHeapBlock(revmap, heapBlk, &buf, &off, NULL, BUFFER_LOCK_SHARE, NULL);

                                                         
    if (!brtup)
    {
      break;
    }

                                               
    if (bdesc == NULL)
    {
      MemoryContextSwitchTo(indexInfo->ii_Context);
      bdesc = brin_build_desc(idxRel);
      indexInfo->ii_AmCache = (void *)bdesc;
      MemoryContextSwitchTo(oldcxt);
    }
                                                     
    if (tupcxt == NULL)
    {
      tupcxt = AllocSetContextCreate(CurrentMemoryContext, "brininsert cxt", ALLOCSET_DEFAULT_SIZES);
      MemoryContextSwitchTo(tupcxt);
    }

    dtup = brin_deform_tuple(bdesc, brtup, NULL);

       
                                                                           
                                                                        
                                                                          
                                                                       
                                        
       
    for (keyno = 0; keyno < bdesc->bd_tupdesc->natts; keyno++)
    {
      Datum result;
      BrinValues *bval;
      FmgrInfo *addValue;

      bval = &dtup->bt_columns[keyno];
      addValue = index_getprocinfo(idxRel, keyno + 1, BRIN_PROCNUM_ADDVALUE);
      result = FunctionCall4Coll(addValue, idxRel->rd_indcollation[keyno], PointerGetDatum(bdesc), PointerGetDatum(bval), values[keyno], nulls[keyno]);
                                                                      
      need_insert |= DatumGetBool(result);
    }

    if (!need_insert)
    {
         
                                                                         
                
         
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    }
    else
    {
      Page page = BufferGetPage(buf);
      ItemId lp = PageGetItemId(page, off);
      Size origsz;
      BrinTuple *origtup;
      Size newsz;
      BrinTuple *newtup;
      bool samepage;

         
                                                                       
                                
         
      origsz = ItemIdGetLength(lp);
      origtup = brin_copy_tuple(brtup, origsz, NULL, NULL);

         
                                                                        
                                                                       
                                                                         
                                                         
         
      newtup = brin_form_tuple(bdesc, heapBlk, dtup, &newsz);
      samepage = brin_can_do_samepage_update(buf, origsz, newsz);
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);

         
                                                                     
                                                                      
                                                                         
                                                                   
                                                                         
                                         
         
      if (!brin_doupdate(idxRel, pagesPerRange, revmap, heapBlk, buf, off, origtup, origsz, newtup, newsz, samepage))
      {
                                 
        MemoryContextResetAndDeleteChildren(tupcxt);
        continue;
      }
    }

                  
    break;
  }

  brinRevmapTerminate(revmap);
  if (BufferIsValid(buf))
  {
    ReleaseBuffer(buf);
  }
  MemoryContextSwitchTo(oldcxt);
  if (tupcxt != NULL)
  {
    MemoryContextDelete(tupcxt);
  }

  return false;
}

   
                                           
   
                                                                               
                                                                             
                                                                                
   
IndexScanDesc
brinbeginscan(Relation r, int nkeys, int norderbys)
{
  IndexScanDesc scan;
  BrinOpaque *opaque;

  scan = RelationGetIndexScan(r, nkeys, norderbys);

  opaque = (BrinOpaque *)palloc(sizeof(BrinOpaque));
  opaque->bo_rmAccess = brinRevmapInitialize(r, &opaque->bo_pagesPerRange, scan->xs_snapshot);
  opaque->bo_bdesc = brin_build_desc(r);
  scan->opaque = opaque;

  return scan;
}

   
                           
   
                                                                             
                                                                         
                                                                              
                                                                  
   
                                                                         
                                                                               
         
   
int64
bringetbitmap(IndexScanDesc scan, TIDBitmap *tbm)
{
  Relation idxRel = scan->indexRelation;
  Buffer buf = InvalidBuffer;
  BrinDesc *bdesc;
  Oid heapOid;
  Relation heapRel;
  BrinOpaque *opaque;
  BlockNumber nblocks;
  BlockNumber heapBlk;
  int totalpages = 0;
  FmgrInfo *consistentFn;
  MemoryContext oldcxt;
  MemoryContext perRangeCxt;
  BrinMemTuple *dtup;
  BrinTuple *btup = NULL;
  Size btupsz = 0;

  opaque = (BrinOpaque *)scan->opaque;
  bdesc = opaque->bo_bdesc;
  pgstat_count_index_scan(idxRel);

     
                                                                       
                            
     
  heapOid = IndexGetRelation(RelationGetRelid(idxRel), false);
  heapRel = table_open(heapOid, AccessShareLock);
  nblocks = RelationGetNumberOfBlocks(heapRel);
  table_close(heapRel, AccessShareLock);

     
                                                                             
                                                                             
                                                                           
     
  consistentFn = palloc0(sizeof(FmgrInfo) * bdesc->bd_tupdesc->natts);

                                                                        
  dtup = brin_new_memtuple(bdesc);

     
                                                                            
                                                                         
     
  perRangeCxt = AllocSetContextCreate(CurrentMemoryContext, "bringetbitmap cxt", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(perRangeCxt);

     
                                                                 
                                                                         
                        
     
  for (heapBlk = 0; heapBlk < nblocks; heapBlk += opaque->bo_pagesPerRange)
  {
    bool addrange;
    bool gottuple = false;
    BrinTuple *tup;
    OffsetNumber off;
    Size size;

    CHECK_FOR_INTERRUPTS();

    MemoryContextResetAndDeleteChildren(perRangeCxt);

    tup = brinGetTupleForHeapBlock(opaque->bo_rmAccess, heapBlk, &buf, &off, &size, BUFFER_LOCK_SHARE, scan->xs_snapshot);
    if (tup)
    {
      gottuple = true;
      btup = brin_copy_tuple(tup, size, btup, &btupsz);
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    }

       
                                                                       
                                                      
       
    if (!gottuple)
    {
      addrange = true;
    }
    else
    {
      dtup = brin_deform_tuple(bdesc, btup, dtup);
      if (dtup->bt_placeholder)
      {
           
                                                                     
                                  
           
        addrange = true;
      }
      else
      {
        int keyno;

           
                                                                       
                                                                     
                                                                  
                                                                     
                    
           
        addrange = true;
        for (keyno = 0; keyno < scan->numberOfKeys; keyno++)
        {
          ScanKey key = &scan->keyData[keyno];
          AttrNumber keyattno = key->sk_attno;
          BrinValues *bval = &dtup->bt_columns[keyattno - 1];
          Datum add;

             
                                                                    
                                                                     
                                                                     
                            
             
          Assert((key->sk_flags & SK_ISNULL) || (key->sk_collation == TupleDescAttr(bdesc->bd_tupdesc, keyattno - 1)->attcollation));

                                                                   
          if (consistentFn[keyattno - 1].fn_oid == InvalidOid)
          {
            FmgrInfo *tmp;

            tmp = index_getprocinfo(idxRel, keyattno, BRIN_PROCNUM_CONSISTENT);
            fmgr_info_copy(&consistentFn[keyattno - 1], tmp, CurrentMemoryContext);
          }

             
                                                                    
                                                                    
                                   
             
                                                                    
                                                                    
                                                                    
                                                  
             
          add = FunctionCall3Coll(&consistentFn[keyattno - 1], key->sk_collation, PointerGetDatum(bdesc), PointerGetDatum(bval), PointerGetDatum(key));
          addrange = DatumGetBool(add);
          if (!addrange)
          {
            break;
          }
        }
      }
    }

                                                                    
    if (addrange)
    {
      BlockNumber pageno;

      for (pageno = heapBlk; pageno <= Min(nblocks, heapBlk + opaque->bo_pagesPerRange) - 1; pageno++)
      {
        MemoryContextSwitchTo(oldcxt);
        tbm_add_page(tbm, pageno);
        totalpages++;
        MemoryContextSwitchTo(perRangeCxt);
      }
    }
  }

  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(perRangeCxt);

  if (buf != InvalidBuffer)
  {
    ReleaseBuffer(buf);
  }

     
                                                                         
                                                                            
               
     
  return totalpages * 10;
}

   
                                             
   
void
brinrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{
     
                                                                         
                                                                          
                                                                           
                                                                             
                        
     

  if (scankey && scan->numberOfKeys > 0)
  {
    memmove(scan->keyData, scankey, scan->numberOfKeys * sizeof(ScanKeyData));
  }
}

   
                                
   
void
brinendscan(IndexScanDesc scan)
{
  BrinOpaque *opaque = (BrinOpaque *)scan->opaque;

  brinRevmapTerminate(opaque->bo_rmAccess);
  brin_free_desc(opaque->bo_bdesc);
  pfree(opaque);
}

   
                                                       
   
                                                                                
                                                                               
                                                                          
   
static void
brinbuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *brstate)
{
  BrinBuildState *state = (BrinBuildState *)brstate;
  BlockNumber thisblock;
  int i;

  thisblock = ItemPointerGetBlockNumber(&htup->t_self);

     
                                                                        
                                                                        
                                                                          
                           
     
  while (thisblock > state->bs_currRangeStart + state->bs_pagesPerRange - 1)
  {

    BRIN_elog((DEBUG2, "brinbuildCallback: completed a range: %u--%u", state->bs_currRangeStart, state->bs_currRangeStart + state->bs_pagesPerRange));

                                              
    form_and_insert_tuple(state);

                                                   
    state->bs_currRangeStart += state->bs_pagesPerRange;

                                    
    brin_memtuple_initialize(state->bs_dtuple, state->bs_bdesc);
  }

                                                           
  for (i = 0; i < state->bs_bdesc->bd_tupdesc->natts; i++)
  {
    FmgrInfo *addValue;
    BrinValues *col;
    Form_pg_attribute attr = TupleDescAttr(state->bs_bdesc->bd_tupdesc, i);

    col = &state->bs_dtuple->bt_columns[i];
    addValue = index_getprocinfo(index, i + 1, BRIN_PROCNUM_ADDVALUE);

       
                                                 
       
    FunctionCall4Coll(addValue, attr->attcollation, PointerGetDatum(state->bs_bdesc), PointerGetDatum(col), values[i], isnull[i]);
  }
}

   
                                          
   
IndexBuildResult *
brinbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{
  IndexBuildResult *result;
  double reltuples;
  double idxtuples;
  BrinRevmap *revmap;
  BrinBuildState *state;
  Buffer meta;
  BlockNumber pagesPerRange;

     
                                                                 
     
  if (RelationGetNumberOfBlocks(index) != 0)
  {
    elog(ERROR, "index \"%s\" already contains data", RelationGetRelationName(index));
  }

     
                                                                         
                                         
     

  meta = ReadBuffer(index, P_NEW);
  Assert(BufferGetBlockNumber(meta) == BRIN_METAPAGE_BLKNO);
  LockBuffer(meta, BUFFER_LOCK_EXCLUSIVE);

  brin_metapage_init(BufferGetPage(meta), BrinGetPagesPerRange(index), BRIN_CURRENT_VERSION);
  MarkBufferDirty(meta);

  if (RelationNeedsWAL(index))
  {
    xl_brin_createidx xlrec;
    XLogRecPtr recptr;
    Page page;

    xlrec.version = BRIN_CURRENT_VERSION;
    xlrec.pagesPerRange = BrinGetPagesPerRange(index);

    XLogBeginInsert();
    XLogRegisterData((char *)&xlrec, SizeOfBrinCreateIdx);
    XLogRegisterBuffer(0, meta, REGBUF_WILL_INIT | REGBUF_STANDARD);

    recptr = XLogInsert(RM_BRIN_ID, XLOG_BRIN_CREATE_INDEX);

    page = BufferGetPage(meta);
    PageSetLSN(page, recptr);
  }

  UnlockReleaseBuffer(meta);

     
                                                               
     
  revmap = brinRevmapInitialize(index, &pagesPerRange, NULL);
  state = initialize_brin_buildstate(index, revmap, pagesPerRange);

     
                                                                          
                                    
     
  reltuples = table_index_build_scan(heap, index, indexInfo, false, true, brinbuildCallback, (void *)state, NULL);

                               
  form_and_insert_tuple(state);

                         
  idxtuples = state->bs_numtuples;
  brinRevmapTerminate(state->bs_rmAccess);
  terminate_brin_buildstate(state);

     
                       
     
  result = (IndexBuildResult *)palloc(sizeof(IndexBuildResult));

  result->heap_tuples = reltuples;
  result->index_tuples = idxtuples;

  return result;
}

void
brinbuildempty(Relation index)
{
  Buffer metabuf;

                                                
  metabuf = ReadBufferExtended(index, INIT_FORKNUM, P_NEW, RBM_NORMAL, NULL);
  LockBuffer(metabuf, BUFFER_LOCK_EXCLUSIVE);

                                       
  START_CRIT_SECTION();
  brin_metapage_init(BufferGetPage(metabuf), BrinGetPagesPerRange(index), BRIN_CURRENT_VERSION);
  MarkBufferDirty(metabuf);
  log_newpage_buffer(metabuf, true);
  END_CRIT_SECTION();

  UnlockReleaseBuffer(metabuf);
}

   
                  
                                                                    
                                      
   
                                                                            
                                                                               
                                                                   
   
IndexBulkDeleteResult *
brinbulkdelete(IndexVacuumInfo *info, IndexBulkDeleteResult *stats, IndexBulkDeleteCallback callback, void *callback_state)
{
                                                                         
  if (stats == NULL)
  {
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
  }

  return stats;
}

   
                                                                            
                                           
   
IndexBulkDeleteResult *
brinvacuumcleanup(IndexVacuumInfo *info, IndexBulkDeleteResult *stats)
{
  Relation heapRel;

                                  
  if (info->analyze_only)
  {
    return stats;
  }

  if (!stats)
  {
    stats = (IndexBulkDeleteResult *)palloc0(sizeof(IndexBulkDeleteResult));
  }
  stats->num_pages = RelationGetNumberOfBlocks(info->index);
                                               

  heapRel = table_open(IndexGetRelation(RelationGetRelid(info->index), false), AccessShareLock);

  brin_vacuum_scan(info->index, info->strategy);

  brinsummarize(info->index, heapRel, BRIN_ALL_BLOCKRANGES, false, &stats->num_index_tuples, &stats->num_index_tuples);

  table_close(heapRel, AccessShareLock);

  return stats;
}

   
                                         
   
bytea *
brinoptions(Datum reloptions, bool validate)
{
  relopt_value *options;
  BrinOptions *rdopts;
  int numoptions;
  static const relopt_parse_elt tab[] = {{"pages_per_range", RELOPT_TYPE_INT, offsetof(BrinOptions, pagesPerRange)}, {"autosummarize", RELOPT_TYPE_BOOL, offsetof(BrinOptions, autosummarize)}};

  options = parseRelOptions(reloptions, validate, RELOPT_KIND_BRIN, &numoptions);

                               
  if (numoptions == 0)
  {
    return NULL;
  }

  rdopts = allocateReloptStruct(sizeof(BrinOptions), options, numoptions);

  fillRelOptions((void *)rdopts, sizeof(BrinOptions), options, numoptions, validate, tab, lengthof(tab));

  pfree(options);

  return (bytea *)rdopts;
}

   
                                                                           
                                      
   
Datum
brin_summarize_new_values(PG_FUNCTION_ARGS)
{
  Datum relation = PG_GETARG_DATUM(0);

  return DirectFunctionCall2(brin_summarize_range, relation, Int64GetDatum((int64)BRIN_ALL_BLOCKRANGES));
}

   
                                                                               
                                                                    
                                       
   
Datum
brin_summarize_range(PG_FUNCTION_ARGS)
{
  Oid indexoid = PG_GETARG_OID(0);
  int64 heapBlk64 = PG_GETARG_INT64(1);
  BlockNumber heapBlk;
  Oid heapoid;
  Relation indexRel;
  Relation heapRel;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;
  double numSummarized = 0;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("BRIN control functions cannot be executed during recovery.")));
  }

  if (heapBlk64 > BRIN_ALL_BLOCKRANGES || heapBlk64 < 0)
  {
    char *blk = psprintf(INT64_FORMAT, heapBlk64);

    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("block number out of range: %s", blk)));
  }
  heapBlk = (BlockNumber)heapBlk64;

     
                                                                          
                                                                       
                                                                     
                                                                          
     
  heapoid = IndexGetRelation(indexoid, true);
  if (OidIsValid(heapoid))
  {
    heapRel = table_open(heapoid, ShareUpdateExclusiveLock);

       
                                                                          
                                                                       
                                                                        
                                                                         
                                                                         
                                    
       
    GetUserIdAndSecContext(&save_userid, &save_sec_context);
    SetUserIdAndSecContext(heapRel->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
    save_nestlevel = NewGUCNestLevel();
  }
  else
  {
    heapRel = NULL;
                                                                      
    save_userid = InvalidOid;
    save_sec_context = -1;
    save_nestlevel = -1;
  }

  indexRel = index_open(indexoid, ShareUpdateExclusiveLock);

                            
  if (indexRel->rd_rel->relkind != RELKIND_INDEX || indexRel->rd_rel->relam != BRIN_AM_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a BRIN index", RelationGetRelationName(indexRel))));
  }

                                                                            
  if (heapRel != NULL && !pg_class_ownercheck(indexoid, save_userid))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_INDEX, RelationGetRelationName(indexRel));
  }

     
                                                                         
                                                                             
                                          
     
  if (heapRel == NULL || heapoid != IndexGetRelation(indexoid, false))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("could not open parent table of index %s", RelationGetRelationName(indexRel))));
  }

                 
  brinsummarize(indexRel, heapRel, heapBlk, true, &numSummarized, NULL);

                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

  relation_close(indexRel, ShareUpdateExclusiveLock);
  relation_close(heapRel, ShareUpdateExclusiveLock);

  PG_RETURN_INT32((int32)numSummarized);
}

   
                                                                  
   
Datum
brin_desummarize_range(PG_FUNCTION_ARGS)
{
  Oid indexoid = PG_GETARG_OID(0);
  int64 heapBlk64 = PG_GETARG_INT64(1);
  BlockNumber heapBlk;
  Oid heapoid;
  Relation heapRel;
  Relation indexRel;
  bool done;

  if (RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("recovery is in progress"), errhint("BRIN control functions cannot be executed during recovery.")));
  }

  if (heapBlk64 > MaxBlockNumber || heapBlk64 < 0)
  {
    char *blk = psprintf(INT64_FORMAT, heapBlk64);

    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("block number out of range: %s", blk)));
  }
  heapBlk = (BlockNumber)heapBlk64;

     
                                                                          
                                                                       
                                                                     
                                                                          
     
                                                                            
                          
     
  heapoid = IndexGetRelation(indexoid, true);
  if (OidIsValid(heapoid))
  {
    heapRel = table_open(heapoid, ShareUpdateExclusiveLock);
  }
  else
  {
    heapRel = NULL;
  }

  indexRel = index_open(indexoid, ShareUpdateExclusiveLock);

                            
  if (indexRel->rd_rel->relkind != RELKIND_INDEX || indexRel->rd_rel->relam != BRIN_AM_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a BRIN index", RelationGetRelationName(indexRel))));
  }

                                                                            
  if (!pg_class_ownercheck(indexoid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_INDEX, RelationGetRelationName(indexRel));
  }

     
                                                                         
                                                                             
                                          
     
  if (heapRel == NULL || heapoid != IndexGetRelation(indexoid, false))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("could not open parent table of index %s", RelationGetRelationName(indexRel))));
  }

                                     
  do
  {
    done = brinRevmapDesummarizeRange(indexRel, heapBlk);
  } while (!done);

  relation_close(indexRel, ShareUpdateExclusiveLock);
  relation_close(heapRel, ShareUpdateExclusiveLock);

  PG_RETURN_VOID();
}

   
                                                        
   
BrinDesc *
brin_build_desc(Relation rel)
{
  BrinOpcInfo **opcinfo;
  BrinDesc *bdesc;
  TupleDesc tupdesc;
  int totalstored = 0;
  int keyno;
  long totalsize;
  MemoryContext cxt;
  MemoryContext oldcxt;

  cxt = AllocSetContextCreate(CurrentMemoryContext, "brin desc cxt", ALLOCSET_SMALL_SIZES);
  oldcxt = MemoryContextSwitchTo(cxt);
  tupdesc = RelationGetDescr(rel);

     
                                                                          
                                                                        
     
  opcinfo = (BrinOpcInfo **)palloc(sizeof(BrinOpcInfo *) * tupdesc->natts);
  for (keyno = 0; keyno < tupdesc->natts; keyno++)
  {
    FmgrInfo *opcInfoFn;
    Form_pg_attribute attr = TupleDescAttr(tupdesc, keyno);

    opcInfoFn = index_getprocinfo(rel, keyno + 1, BRIN_PROCNUM_OPCINFO);

    opcinfo[keyno] = (BrinOpcInfo *)DatumGetPointer(FunctionCall1(opcInfoFn, attr->atttypid));
    totalstored += opcinfo[keyno]->oi_nstored;
  }

                                                 
  totalsize = offsetof(BrinDesc, bd_info) + sizeof(BrinOpcInfo *) * tupdesc->natts;

  bdesc = palloc(totalsize);
  bdesc->bd_context = cxt;
  bdesc->bd_index = rel;
  bdesc->bd_tupdesc = tupdesc;
  bdesc->bd_disktdesc = NULL;                       
  bdesc->bd_totalstored = totalstored;

  for (keyno = 0; keyno < tupdesc->natts; keyno++)
  {
    bdesc->bd_info[keyno] = opcinfo[keyno];
  }
  pfree(opcinfo);

  MemoryContextSwitchTo(oldcxt);

  return bdesc;
}

void
brin_free_desc(BrinDesc *bdesc)
{
                                            
  Assert(bdesc->bd_tupdesc->tdrefcount >= 1);
                                
  MemoryContextDelete(bdesc->bd_context);
}

   
                                              
   
void
brinGetStats(Relation index, BrinStatsData *stats)
{
  Buffer metabuffer;
  Page metapage;
  BrinMetaPageData *metadata;

  metabuffer = ReadBuffer(index, BRIN_METAPAGE_BLKNO);
  LockBuffer(metabuffer, BUFFER_LOCK_SHARE);
  metapage = BufferGetPage(metabuffer);
  metadata = (BrinMetaPageData *)PageGetContents(metapage);

  stats->pagesPerRange = metadata->pagesPerRange;
  stats->revmapNumPages = metadata->lastRevmapPage - 1;

  UnlockReleaseBuffer(metabuffer);
}

   
                                                                                
   
static BrinBuildState *
initialize_brin_buildstate(Relation idxRel, BrinRevmap *revmap, BlockNumber pagesPerRange)
{
  BrinBuildState *state;

  state = palloc(sizeof(BrinBuildState));

  state->bs_irel = idxRel;
  state->bs_numtuples = 0;
  state->bs_currentInsertBuf = InvalidBuffer;
  state->bs_pagesPerRange = pagesPerRange;
  state->bs_currRangeStart = 0;
  state->bs_rmAccess = revmap;
  state->bs_bdesc = brin_build_desc(idxRel);
  state->bs_dtuple = brin_new_memtuple(state->bs_bdesc);

  brin_memtuple_initialize(state->bs_dtuple, state->bs_bdesc);

  return state;
}

   
                                                       
   
static void
terminate_brin_buildstate(BrinBuildState *state)
{
     
                                                                       
                                                                        
     
  if (!BufferIsInvalid(state->bs_currentInsertBuf))
  {
    Page page;
    Size freespace;
    BlockNumber blk;

    page = BufferGetPage(state->bs_currentInsertBuf);
    freespace = PageGetFreeSpace(page);
    blk = BufferGetBlockNumber(state->bs_currentInsertBuf);
    ReleaseBuffer(state->bs_currentInsertBuf);
    RecordPageWithFreeSpace(state->bs_irel, blk, freespace);
    FreeSpaceMapVacuumRange(state->bs_irel, blk, blk + 1);
  }

  brin_free_desc(state->bs_bdesc);
  pfree(state->bs_dtuple);
  pfree(state);
}

   
                                                                           
                                   
   
                                                                             
                                                                              
                                                                        
                                                                              
                                                                           
                                                                            
                                                                             
                                                                    
   
                                                                              
                                                                            
                                                                            
                                                                          
                                                    
   
static void
summarize_range(IndexInfo *indexInfo, BrinBuildState *state, Relation heapRel, BlockNumber heapBlk, BlockNumber heapNumBlks)
{
  Buffer phbuf;
  BrinTuple *phtup;
  Size phsz;
  OffsetNumber offset;
  BlockNumber scanNumBlks;

     
                                  
     
  phbuf = InvalidBuffer;
  phtup = brin_form_placeholder_tuple(state->bs_bdesc, heapBlk, &phsz);
  offset = brin_doinsert(state->bs_irel, state->bs_pagesPerRange, state->bs_rmAccess, &phbuf, heapBlk, phtup, phsz);

     
                                                                           
                                                   
     
  Assert(heapBlk % state->bs_pagesPerRange == 0);
  if (heapBlk + state->bs_pagesPerRange > heapNumBlks)
  {
       
                                                                           
                                                                           
                                                                       
                                                                 
                                                                        
                                                                     
                                                      
       
                                                    
       
    scanNumBlks = Min(RelationGetNumberOfBlocks(heapRel) - heapBlk, state->bs_pagesPerRange);
  }
  else
  {
                                                  
    scanNumBlks = state->bs_pagesPerRange;
  }

     
                                                                             
                                                                          
                                                              
     
                                                               
                                                                        
                                                                             
            
     
  state->bs_currRangeStart = heapBlk;
  table_index_build_range_scan(heapRel, state->bs_irel, indexInfo, false, true, false, heapBlk, scanNumBlks, brinbuildCallback, (void *)state, NULL);

     
                                                                        
                                                                         
                                                                          
                                                                    
     
  for (;;)
  {
    BrinTuple *newtup;
    Size newsize;
    bool didupdate;
    bool samepage;

    CHECK_FOR_INTERRUPTS();

       
                                                   
       
    newtup = brin_form_tuple(state->bs_bdesc, heapBlk, state->bs_dtuple, &newsize);
    samepage = brin_can_do_samepage_update(phbuf, phsz, newsize);
    didupdate = brin_doupdate(state->bs_irel, state->bs_pagesPerRange, state->bs_rmAccess, heapBlk, phbuf, offset, phtup, phsz, newtup, newsize, samepage);
    brin_free_tuple(phtup);
    brin_free_tuple(newtup);

                                              
    if (didupdate)
    {
      break;
    }

       
                                                                           
                                                                          
                                                                          
                                                                           
                  
       
    phtup = brinGetTupleForHeapBlock(state->bs_rmAccess, heapBlk, &phbuf, &offset, &phsz, BUFFER_LOCK_SHARE, NULL);
                                          
    if (phtup == NULL)
    {
      elog(ERROR, "missing placeholder tuple");
    }
    phtup = brin_copy_tuple(phtup, phsz, NULL, NULL);
    LockBuffer(phbuf, BUFFER_LOCK_UNLOCK);

                                                    
    union_tuples(state->bs_bdesc, state->bs_dtuple, phtup);
  }

  ReleaseBuffer(phbuf);
}

   
                                                                           
                                                                             
                                                                
                                                                              
                                 
   
                                                                      
                                                                       
                
   
static void
brinsummarize(Relation index, Relation heapRel, BlockNumber pageRange, bool include_partial, double *numSummarized, double *numExisting)
{
  BrinRevmap *revmap;
  BrinBuildState *state = NULL;
  IndexInfo *indexInfo = NULL;
  BlockNumber heapNumBlocks;
  BlockNumber pagesPerRange;
  Buffer buf;
  BlockNumber startBlk;

  revmap = brinRevmapInitialize(index, &pagesPerRange, NULL);

                                           
  heapNumBlocks = RelationGetNumberOfBlocks(heapRel);
  if (pageRange == BRIN_ALL_BLOCKRANGES)
  {
    startBlk = 0;
  }
  else
  {
    startBlk = (pageRange / pagesPerRange) * pagesPerRange;
    heapNumBlocks = Min(heapNumBlocks, startBlk + pagesPerRange);
  }
  if (startBlk > heapNumBlocks)
  {
                                                             
    brinRevmapTerminate(revmap);
    return;
  }

     
                                                 
     
  buf = InvalidBuffer;
  for (; startBlk < heapNumBlocks; startBlk += pagesPerRange)
  {
    BrinTuple *tup;
    OffsetNumber off;

       
                                                                          
                                                                           
                                                       
                                                                       
                                                                        
       
    if (!include_partial && (startBlk + pagesPerRange > heapNumBlocks))
    {
      break;
    }

    CHECK_FOR_INTERRUPTS();

    tup = brinGetTupleForHeapBlock(revmap, startBlk, &buf, &off, NULL, BUFFER_LOCK_SHARE, NULL);
    if (tup == NULL)
    {
                                                              
      if (state == NULL)
      {
                                
        Assert(!indexInfo);
        state = initialize_brin_buildstate(index, revmap, pagesPerRange);
        indexInfo = BuildIndexInfo(index);
      }
      summarize_range(indexInfo, state, heapRel, startBlk, heapNumBlocks);

                                                      
      brin_memtuple_initialize(state->bs_dtuple, state->bs_bdesc);

      if (numSummarized)
      {
        *numSummarized += 1.0;
      }
    }
    else
    {
      if (numExisting)
      {
        *numExisting += 1.0;
      }
      LockBuffer(buf, BUFFER_LOCK_UNLOCK);
    }
  }

  if (BufferIsValid(buf))
  {
    ReleaseBuffer(buf);
  }

                      
  brinRevmapTerminate(revmap);
  if (state)
  {
    terminate_brin_buildstate(state);
    pfree(indexInfo);
  }
}

   
                                                                          
                                                                       
   
static void
form_and_insert_tuple(BrinBuildState *state)
{
  BrinTuple *tup;
  Size size;

  tup = brin_form_tuple(state->bs_bdesc, state->bs_currRangeStart, state->bs_dtuple, &size);
  brin_doinsert(state->bs_irel, state->bs_pagesPerRange, state->bs_rmAccess, &state->bs_currentInsertBuf, state->bs_currRangeStart, tup, size);
  state->bs_numtuples++;

  pfree(tup);
}

   
                                                                           
                                    
   
static void
union_tuples(BrinDesc *bdesc, BrinMemTuple *a, BrinTuple *b)
{
  int keyno;
  BrinMemTuple *db;
  MemoryContext cxt;
  MemoryContext oldcxt;

                                                        
  cxt = AllocSetContextCreate(CurrentMemoryContext, "brin union", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(cxt);
  db = brin_deform_tuple(bdesc, b, NULL);
  MemoryContextSwitchTo(oldcxt);

  for (keyno = 0; keyno < bdesc->bd_tupdesc->natts; keyno++)
  {
    FmgrInfo *unionFn;
    BrinValues *col_a = &a->bt_columns[keyno];
    BrinValues *col_b = &db->bt_columns[keyno];

    unionFn = index_getprocinfo(bdesc->bd_index, keyno + 1, BRIN_PROCNUM_UNION);
    FunctionCall3Coll(unionFn, bdesc->bd_index->rd_indcollation[keyno], PointerGetDatum(bdesc), PointerGetDatum(col_a), PointerGetDatum(col_b));
  }

  MemoryContextDelete(cxt);
}

   
                    
                                                   
   
                                                                               
                                                                             
             
   
static void
brin_vacuum_scan(Relation idxrel, BufferAccessStrategy strategy)
{
  BlockNumber nblocks;
  BlockNumber blkno;

     
                                                                         
                
     
  nblocks = RelationGetNumberOfBlocks(idxrel);
  for (blkno = 0; blkno < nblocks; blkno++)
  {
    Buffer buf;

    CHECK_FOR_INTERRUPTS();

    buf = ReadBufferExtended(idxrel, MAIN_FORKNUM, blkno, RBM_NORMAL, strategy);

    brin_page_cleanup(idxrel, buf);

    ReleaseBuffer(buf);
  }

     
                                                                           
                                                                             
                                                                           
     
  FreeSpaceMapVacuum(idxrel);
}
