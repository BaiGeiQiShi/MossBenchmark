                                                                            
   
             
                                                                     
   
   
                                                                         
                                                                        
   
                  
                                      
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/reloptions.h"
#include "access/xloginsert.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/builtins.h"
#include "utils/index_selfuncs.h"
#include "utils/typcache.h"

   
                                                                             
                  
   
Datum
ginhandler(PG_FUNCTION_ARGS)
{
  IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

  amroutine->amstrategies = 0;
  amroutine->amsupport = GINNProcs;
  amroutine->amcanorder = false;
  amroutine->amcanorderbyop = false;
  amroutine->amcanbackward = false;
  amroutine->amcanunique = false;
  amroutine->amcanmulticol = true;
  amroutine->amoptionalkey = true;
  amroutine->amsearcharray = false;
  amroutine->amsearchnulls = false;
  amroutine->amstorage = true;
  amroutine->amclusterable = false;
  amroutine->ampredlocks = true;
  amroutine->amcanparallel = false;
  amroutine->amcaninclude = false;
  amroutine->amkeytype = InvalidOid;

  amroutine->ambuild = ginbuild;
  amroutine->ambuildempty = ginbuildempty;
  amroutine->aminsert = gininsert;
  amroutine->ambulkdelete = ginbulkdelete;
  amroutine->amvacuumcleanup = ginvacuumcleanup;
  amroutine->amcanreturn = NULL;
  amroutine->amcostestimate = gincostestimate;
  amroutine->amoptions = ginoptions;
  amroutine->amproperty = NULL;
  amroutine->ambuildphasename = NULL;
  amroutine->amvalidate = ginvalidate;
  amroutine->ambeginscan = ginbeginscan;
  amroutine->amrescan = ginrescan;
  amroutine->amgettuple = NULL;
  amroutine->amgetbitmap = gingetbitmap;
  amroutine->amendscan = ginendscan;
  amroutine->ammarkpos = NULL;
  amroutine->amrestrpos = NULL;
  amroutine->amestimateparallelscan = NULL;
  amroutine->aminitparallelscan = NULL;
  amroutine->amparallelrescan = NULL;

  PG_RETURN_POINTER(amroutine);
}

   
                                                                        
   
                                                                            
   
void
initGinState(GinState *state, Relation index)
{
  TupleDesc origTupdesc = RelationGetDescr(index);
  int i;

  MemSet(state, 0, sizeof(GinState));

  state->index = index;
  state->oneCol = (origTupdesc->natts == 1) ? true : false;
  state->origTupdesc = origTupdesc;

  for (i = 0; i < origTupdesc->natts; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(origTupdesc, i);

    if (state->oneCol)
    {
      state->tupdesc[i] = state->origTupdesc;
    }
    else
    {
      state->tupdesc[i] = CreateTemplateTupleDesc(2);

      TupleDescInitEntry(state->tupdesc[i], (AttrNumber)1, NULL, INT2OID, -1, 0);
      TupleDescInitEntry(state->tupdesc[i], (AttrNumber)2, NULL, attr->atttypid, attr->atttypmod, attr->attndims);
      TupleDescInitEntryCollation(state->tupdesc[i], (AttrNumber)2, attr->attcollation);
    }

       
                                                                           
                                                         
       
    if (index_getprocid(index, i + 1, GIN_COMPARE_PROC) != InvalidOid)
    {
      fmgr_info_copy(&(state->compareFn[i]), index_getprocinfo(index, i + 1, GIN_COMPARE_PROC), CurrentMemoryContext);
    }
    else
    {
      TypeCacheEntry *typentry;

      typentry = lookup_type_cache(attr->atttypid, TYPECACHE_CMP_PROC_FINFO);
      if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify a comparison function for type %s", format_type_be(attr->atttypid))));
      }
      fmgr_info_copy(&(state->compareFn[i]), &(typentry->cmp_proc_finfo), CurrentMemoryContext);
    }

                                                   
    fmgr_info_copy(&(state->extractValueFn[i]), index_getprocinfo(index, i + 1, GIN_EXTRACTVALUE_PROC), CurrentMemoryContext);
    fmgr_info_copy(&(state->extractQueryFn[i]), index_getprocinfo(index, i + 1, GIN_EXTRACTQUERY_PROC), CurrentMemoryContext);

       
                                                                           
              
       
    if (index_getprocid(index, i + 1, GIN_TRICONSISTENT_PROC) != InvalidOid)
    {
      fmgr_info_copy(&(state->triConsistentFn[i]), index_getprocinfo(index, i + 1, GIN_TRICONSISTENT_PROC), CurrentMemoryContext);
    }

    if (index_getprocid(index, i + 1, GIN_CONSISTENT_PROC) != InvalidOid)
    {
      fmgr_info_copy(&(state->consistentFn[i]), index_getprocinfo(index, i + 1, GIN_CONSISTENT_PROC), CurrentMemoryContext);
    }

    if (state->consistentFn[i].fn_oid == InvalidOid && state->triConsistentFn[i].fn_oid == InvalidOid)
    {
      elog(ERROR, "missing GIN support function (%d or %d) for attribute %d of index \"%s\"", GIN_CONSISTENT_PROC, GIN_TRICONSISTENT_PROC, i + 1, RelationGetRelationName(index));
    }

       
                                                     
       
    if (index_getprocid(index, i + 1, GIN_COMPARE_PARTIAL_PROC) != InvalidOid)
    {
      fmgr_info_copy(&(state->comparePartialFn[i]), index_getprocinfo(index, i + 1, GIN_COMPARE_PARTIAL_PROC), CurrentMemoryContext);
      state->canPartialMatch[i] = true;
    }
    else
    {
      state->canPartialMatch[i] = false;
    }

       
                                                                           
                                                                           
                                                                        
                                                                     
                                                                    
                                                                        
                                                                     
                                                                          
                                                                           
                                         
       
    if (OidIsValid(index->rd_indcollation[i]))
    {
      state->supportCollation[i] = index->rd_indcollation[i];
    }
    else
    {
      state->supportCollation[i] = DEFAULT_COLLATION_OID;
    }
  }
}

   
                                                                    
   
OffsetNumber
gintuple_get_attrnum(GinState *ginstate, IndexTuple tuple)
{
  OffsetNumber colN;

  if (ginstate->oneCol)
  {
                                                
    colN = FirstOffsetNumber;
  }
  else
  {
    Datum res;
    bool isnull;

       
                                                                       
                                                     
       
    res = index_getattr(tuple, FirstOffsetNumber, ginstate->tupdesc[0], &isnull);
    Assert(!isnull);

    colN = DatumGetUInt16(res);
    Assert(colN >= FirstOffsetNumber && colN <= ginstate->origTupdesc->natts);
  }

  return colN;
}

   
                                                                    
   
Datum
gintuple_get_key(GinState *ginstate, IndexTuple tuple, GinNullCategory *category)
{
  Datum res;
  bool isnull;

  if (ginstate->oneCol)
  {
       
                                                                     
       
    res = index_getattr(tuple, FirstOffsetNumber, ginstate->origTupdesc, &isnull);
  }
  else
  {
       
                                                                        
                                                               
       
    OffsetNumber colN = gintuple_get_attrnum(ginstate, tuple);

    res = index_getattr(tuple, OffsetNumberNext(FirstOffsetNumber), ginstate->tupdesc[colN - 1], &isnull);
  }

  if (isnull)
  {
    *category = GinGetNullCategory(tuple, ginstate);
  }
  else
  {
    *category = GIN_CAT_NORM_KEY;
  }

  return res;
}

   
                                                                             
                                                              
                                                                            
   
Buffer
GinNewBuffer(Relation index)
{
  Buffer buffer;
  bool needLock;

                                         
  for (;;)
  {
    BlockNumber blkno = GetFreeIndexPage(index);

    if (blkno == InvalidBlockNumber)
    {
      break;
    }

    buffer = ReadBuffer(index, blkno);

       
                                                                          
                                                           
       
    if (ConditionalLockBuffer(buffer))
    {
      if (GinPageIsRecyclable(BufferGetPage(buffer)))
      {
        return buffer;                
      }

      LockBuffer(buffer, GIN_UNLOCK);
    }

                                                       
    ReleaseBuffer(buffer);
  }

                            
  needLock = !RELATION_IS_LOCAL(index);
  if (needLock)
  {
    LockRelationForExtension(index, ExclusiveLock);
  }

  buffer = ReadBuffer(index, P_NEW);
  LockBuffer(buffer, GIN_EXCLUSIVE);

  if (needLock)
  {
    UnlockRelationForExtension(index, ExclusiveLock);
  }

  return buffer;
}

void
GinInitPage(Page page, uint32 f, Size pageSize)
{
  GinPageOpaque opaque;

  PageInit(page, pageSize, sizeof(GinPageOpaqueData));

  opaque = GinPageGetOpaque(page);
  memset(opaque, 0, sizeof(GinPageOpaqueData));
  opaque->flags = f;
  opaque->rightlink = InvalidBlockNumber;
}

void
GinInitBuffer(Buffer b, uint32 f)
{
  GinInitPage(BufferGetPage(b), f, BufferGetPageSize(b));
}

void
GinInitMetabuffer(Buffer b)
{
  GinMetaPageData *metadata;
  Page page = BufferGetPage(b);

  GinInitPage(page, GIN_META, BufferGetPageSize(b));

  metadata = GinPageGetMeta(page);

  metadata->head = metadata->tail = InvalidBlockNumber;
  metadata->tailFreeSize = 0;
  metadata->nPendingPages = 0;
  metadata->nPendingHeapTuples = 0;
  metadata->nTotalPages = 0;
  metadata->nEntryPages = 0;
  metadata->nDataPages = 0;
  metadata->nEntries = 0;
  metadata->ginVersion = GIN_CURRENT_VERSION;

     
                                                                         
                                                                          
               
     
  ((PageHeader)page)->pd_lower = ((char *)metadata + sizeof(GinMetaPageData)) - (char *)page;
}

   
                                             
   
int
ginCompareEntries(GinState *ginstate, OffsetNumber attnum, Datum a, GinNullCategory categorya, Datum b, GinNullCategory categoryb)
{
                                                        
  if (categorya != categoryb)
  {
    return (categorya < categoryb) ? -1 : 1;
  }

                                                 
  if (categorya != GIN_CAT_NORM_KEY)
  {
    return 0;
  }

                                                    
  return DatumGetInt32(FunctionCall2Coll(&ginstate->compareFn[attnum - 1], ginstate->supportCollation[attnum - 1], a, b));
}

   
                                                        
   
int
ginCompareAttEntries(GinState *ginstate, OffsetNumber attnuma, Datum a, GinNullCategory categorya, OffsetNumber attnumb, Datum b, GinNullCategory categoryb)
{
                                              
  if (attnuma != attnumb)
  {
    return (attnuma < attnumb) ? -1 : 1;
  }

  return ginCompareEntries(ginstate, attnuma, a, categorya, b, categoryb);
}

   
                                                       
   
                                                                  
                                                                     
                                     
   
typedef struct
{
  Datum datum;
  bool isnull;
} keyEntryData;

typedef struct
{
  FmgrInfo *cmpDatumFunc;
  Oid collation;
  bool haveDups;
} cmpEntriesArg;

static int
cmpEntries(const void *a, const void *b, void *arg)
{
  const keyEntryData *aa = (const keyEntryData *)a;
  const keyEntryData *bb = (const keyEntryData *)b;
  cmpEntriesArg *data = (cmpEntriesArg *)arg;
  int res;

  if (aa->isnull)
  {
    if (bb->isnull)
    {
      res = 0;                    
    }
    else
    {
      res = 1;                        
    }
  }
  else if (bb->isnull)
  {
    res = -1;                        
  }
  else
  {
    res = DatumGetInt32(FunctionCall2Coll(data->cmpDatumFunc, data->collation, aa->datum, bb->datum));
  }

     
                                                                            
                                                                             
                                
     
  if (res == 0)
  {
    data->haveDups = true;
  }

  return res;
}

   
                                                       
   
                                                                        
                                                   
   
Datum *
ginExtractEntries(GinState *ginstate, OffsetNumber attnum, Datum value, bool isNull, int32 *nentries, GinNullCategory **categories)
{
  Datum *entries;
  bool *nullFlags;
  int32 i;

     
                                                                          
                  
     
  if (isNull)
  {
    *nentries = 1;
    entries = (Datum *)palloc(sizeof(Datum));
    entries[0] = (Datum)0;
    *categories = (GinNullCategory *)palloc(sizeof(GinNullCategory));
    (*categories)[0] = GIN_CAT_NULL_ITEM;
    return entries;
  }

                                             
  nullFlags = NULL;                                          
  entries = (Datum *)DatumGetPointer(FunctionCall3Coll(&ginstate->extractValueFn[attnum - 1], ginstate->supportCollation[attnum - 1], value, PointerGetDatum(nentries), PointerGetDatum(&nullFlags)));

     
                                                           
     
  if (entries == NULL || *nentries <= 0)
  {
    *nentries = 1;
    entries = (Datum *)palloc(sizeof(Datum));
    entries[0] = (Datum)0;
    *categories = (GinNullCategory *)palloc(sizeof(GinNullCategory));
    (*categories)[0] = GIN_CAT_EMPTY_ITEM;
    return entries;
  }

     
                                                                        
                                          
     
  if (nullFlags == NULL)
  {
    nullFlags = (bool *)palloc0(*nentries * sizeof(bool));
  }

     
                                                        
     
                                                                       
                                                                             
                              
     
  if (*nentries > 1)
  {
    keyEntryData *keydata;
    cmpEntriesArg arg;

    keydata = (keyEntryData *)palloc(*nentries * sizeof(keyEntryData));
    for (i = 0; i < *nentries; i++)
    {
      keydata[i].datum = entries[i];
      keydata[i].isnull = nullFlags[i];
    }

    arg.cmpDatumFunc = &ginstate->compareFn[attnum - 1];
    arg.collation = ginstate->supportCollation[attnum - 1];
    arg.haveDups = false;
    qsort_arg(keydata, *nentries, sizeof(keyEntryData), cmpEntries, (void *)&arg);

    if (arg.haveDups)
    {
                                                     
      int32 j;

      entries[0] = keydata[0].datum;
      nullFlags[0] = keydata[0].isnull;
      j = 1;
      for (i = 1; i < *nentries; i++)
      {
        if (cmpEntries(&keydata[i - 1], &keydata[i], &arg) != 0)
        {
          entries[j] = keydata[i].datum;
          nullFlags[j] = keydata[i].isnull;
          j++;
        }
      }
      *nentries = j;
    }
    else
    {
                               
      for (i = 0; i < *nentries; i++)
      {
        entries[i] = keydata[i].datum;
        nullFlags[i] = keydata[i].isnull;
      }
    }

    pfree(keydata);
  }

     
                                                           
     
  *categories = (GinNullCategory *)palloc0(*nentries * sizeof(GinNullCategory));
  for (i = 0; i < *nentries; i++)
  {
    (*categories)[i] = (nullFlags[i] ? GIN_CAT_NULL_KEY : GIN_CAT_NORM_KEY);
  }

  return entries;
}

bytea *
ginoptions(Datum reloptions, bool validate)
{
  relopt_value *options;
  GinOptions *rdopts;
  int numoptions;
  static const relopt_parse_elt tab[] = {{"fastupdate", RELOPT_TYPE_BOOL, offsetof(GinOptions, useFastUpdate)}, {"gin_pending_list_limit", RELOPT_TYPE_INT, offsetof(GinOptions, pendingListCleanupSize)}};

  options = parseRelOptions(reloptions, validate, RELOPT_KIND_GIN, &numoptions);

                               
  if (numoptions == 0)
  {
    return NULL;
  }

  rdopts = allocateReloptStruct(sizeof(GinOptions), options, numoptions);

  fillRelOptions((void *)rdopts, sizeof(GinOptions), options, numoptions, validate, tab, lengthof(tab));

  pfree(options);

  return (bytea *)rdopts;
}

   
                                              
   
                                                                       
                                                                      
   
void
ginGetStats(Relation index, GinStatsData *stats)
{
  Buffer metabuffer;
  Page metapage;
  GinMetaPageData *metadata;

  metabuffer = ReadBuffer(index, GIN_METAPAGE_BLKNO);
  LockBuffer(metabuffer, GIN_SHARE);
  metapage = BufferGetPage(metabuffer);
  metadata = GinPageGetMeta(metapage);

  stats->nPendingPages = metadata->nPendingPages;
  stats->nTotalPages = metadata->nTotalPages;
  stats->nEntryPages = metadata->nEntryPages;
  stats->nDataPages = metadata->nDataPages;
  stats->nEntries = metadata->nEntries;
  stats->ginVersion = metadata->ginVersion;

  UnlockReleaseBuffer(metabuffer);
}

   
                                                      
   
                                                            
   
void
ginUpdateStats(Relation index, const GinStatsData *stats, bool is_build)
{
  Buffer metabuffer;
  Page metapage;
  GinMetaPageData *metadata;

  metabuffer = ReadBuffer(index, GIN_METAPAGE_BLKNO);
  LockBuffer(metabuffer, GIN_EXCLUSIVE);
  metapage = BufferGetPage(metabuffer);
  metadata = GinPageGetMeta(metapage);

  START_CRIT_SECTION();

  metadata->nTotalPages = stats->nTotalPages;
  metadata->nEntryPages = stats->nEntryPages;
  metadata->nDataPages = stats->nDataPages;
  metadata->nEntries = stats->nEntries;

     
                                                                         
                                                                          
                                                                             
                                                                         
                               
     
  ((PageHeader)metapage)->pd_lower = ((char *)metadata + sizeof(GinMetaPageData)) - (char *)metapage;

  MarkBufferDirty(metabuffer);

  if (RelationNeedsWAL(index) && !is_build)
  {
    XLogRecPtr recptr;
    ginxlogUpdateMeta data;

    data.node = index->rd_node;
    data.ntuples = 0;
    data.newRightlink = data.prevTail = InvalidBlockNumber;
    memcpy(&data.metadata, metadata, sizeof(GinMetaPageData));

    XLogBeginInsert();
    XLogRegisterData((char *)&data, sizeof(ginxlogUpdateMeta));
    XLogRegisterBuffer(0, metabuffer, REGBUF_WILL_INIT | REGBUF_STANDARD);

    recptr = XLogInsert(RM_GIN_ID, XLOG_GIN_UPDATE_META_PAGE);
    PageSetLSN(metapage, recptr);
  }

  UnlockReleaseBuffer(metabuffer);

  END_CRIT_SECTION();
}
