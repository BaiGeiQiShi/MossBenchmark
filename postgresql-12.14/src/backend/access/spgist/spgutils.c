                                                                            
   
              
                                           
   
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include "access/amvalidate.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/spgist_private.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/pg_amop.h"
#include "storage/bufmgr.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/index_selfuncs.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

   
                                                                                 
                  
   
Datum
spghandler(PG_FUNCTION_ARGS)
{
  IndexAmRoutine *amroutine = makeNode(IndexAmRoutine);

  amroutine->amstrategies = 0;
  amroutine->amsupport = SPGISTNProc;
  amroutine->amcanorder = false;
  amroutine->amcanorderbyop = true;
  amroutine->amcanbackward = false;
  amroutine->amcanunique = false;
  amroutine->amcanmulticol = false;
  amroutine->amoptionalkey = true;
  amroutine->amsearcharray = false;
  amroutine->amsearchnulls = true;
  amroutine->amstorage = false;
  amroutine->amclusterable = false;
  amroutine->ampredlocks = false;
  amroutine->amcanparallel = false;
  amroutine->amcaninclude = false;
  amroutine->amkeytype = InvalidOid;

  amroutine->ambuild = spgbuild;
  amroutine->ambuildempty = spgbuildempty;
  amroutine->aminsert = spginsert;
  amroutine->ambulkdelete = spgbulkdelete;
  amroutine->amvacuumcleanup = spgvacuumcleanup;
  amroutine->amcanreturn = spgcanreturn;
  amroutine->amcostestimate = spgcostestimate;
  amroutine->amoptions = spgoptions;
  amroutine->amproperty = spgproperty;
  amroutine->ambuildphasename = NULL;
  amroutine->amvalidate = spgvalidate;
  amroutine->ambeginscan = spgbeginscan;
  amroutine->amrescan = spgrescan;
  amroutine->amgettuple = spggettuple;
  amroutine->amgetbitmap = spggetbitmap;
  amroutine->amendscan = spgendscan;
  amroutine->ammarkpos = NULL;
  amroutine->amrestrpos = NULL;
  amroutine->amestimateparallelscan = NULL;
  amroutine->aminitparallelscan = NULL;
  amroutine->amparallelrescan = NULL;

  PG_RETURN_POINTER(amroutine);
}

                                                                             
static void
fillTypeDesc(SpGistTypeDesc *desc, Oid type)
{
  desc->type = type;
  get_typlenbyval(type, &desc->attlen, &desc->attbyval);
}

   
                                                                          
                
   
SpGistCache *
spgGetCache(Relation index)
{
  SpGistCache *cache;

  if (index->rd_amcache == NULL)
  {
    Oid atttype;
    spgConfigIn in;
    FmgrInfo *procinfo;
    Buffer metabuffer;
    SpGistMetaPageData *metadata;

    cache = MemoryContextAllocZero(index->rd_indexcxt, sizeof(SpGistCache));

                                                     
    Assert(index->rd_att->natts == 1);

       
                                                                     
                                                                     
                                           
       
    atttype = TupleDescAttr(index->rd_att, 0)->atttypid;

                                                                     
    in.attType = atttype;

    procinfo = index_getprocinfo(index, 1, SPGIST_CONFIG_PROC);
    FunctionCall2Coll(procinfo, index->rd_indcollation[0], PointerGetDatum(&in), PointerGetDatum(&cache->config));

                                                                  
    fillTypeDesc(&cache->attType, atttype);

    if (OidIsValid(cache->config.leafType) && cache->config.leafType != atttype)
    {
      if (!OidIsValid(index_getprocid(index, 1, SPGIST_COMPRESS_PROC)))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("compress method must be defined when leaf type is different from input type")));
      }

      fillTypeDesc(&cache->attLeafType, cache->config.leafType);
    }
    else
    {
      cache->attLeafType = cache->attType;
    }

    fillTypeDesc(&cache->attPrefixType, cache->config.prefixType);
    fillTypeDesc(&cache->attLabelType, cache->config.labelType);

                                                            
    metabuffer = ReadBuffer(index, SPGIST_METAPAGE_BLKNO);
    LockBuffer(metabuffer, BUFFER_LOCK_SHARE);

    metadata = SpGistPageGetMeta(BufferGetPage(metabuffer));

    if (metadata->magicNumber != SPGIST_MAGIC_NUMBER)
    {
      elog(ERROR, "index \"%s\" is not an SP-GiST index", RelationGetRelationName(index));
    }

    cache->lastUsedPages = metadata->lastUsedPages;

    UnlockReleaseBuffer(metabuffer);

    index->rd_amcache = (void *)cache;
  }
  else
  {
                                
    cache = (SpGistCache *)index->rd_amcache;
  }

  return cache;
}

                                                             
void
initSpGistState(SpGistState *state, Relation index)
{
  SpGistCache *cache;

                                                 
  cache = spgGetCache(index);

  state->config = cache->config;
  state->attType = cache->attType;
  state->attLeafType = cache->attLeafType;
  state->attPrefixType = cache->attPrefixType;
  state->attLabelType = cache->attLabelType;

                                                   
  state->deadTupleStorage = palloc0(SGDTSIZE);

                                            
  state->myXid = GetTopTransactionIdIfAny();

                                                                   
  state->isBuild = false;
}

   
                                                                              
   
                                                               
                                                                                
   
Buffer
SpGistNewBuffer(Relation index)
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

       
                                                                         
                          
       
    if (SpGistBlockIsFixed(blkno))
    {
      continue;
    }

    buffer = ReadBuffer(index, blkno);

       
                                                                          
                                                           
       
    if (ConditionalLockBuffer(buffer))
    {
      Page page = BufferGetPage(buffer);

      if (PageIsNew(page))
      {
        return buffer;                                      
      }

      if (SpGistPageIsDeleted(page) || PageIsEmpty(page))
      {
        return buffer;                
      }

      LockBuffer(buffer, BUFFER_LOCK_UNLOCK);
    }

                                                       
    ReleaseBuffer(buffer);
  }

                            
  needLock = !RELATION_IS_LOCAL(index);
  if (needLock)
  {
    LockRelationForExtension(index, ExclusiveLock);
  }

  buffer = ReadBuffer(index, P_NEW);
  LockBuffer(buffer, BUFFER_LOCK_EXCLUSIVE);

  if (needLock)
  {
    UnlockRelationForExtension(index, ExclusiveLock);
  }

  return buffer;
}

   
                                                                            
   
                                                           
                                                      
                                                              
   
void
SpGistUpdateMetaPage(Relation index)
{
  SpGistCache *cache = (SpGistCache *)index->rd_amcache;

  if (cache != NULL)
  {
    Buffer metabuffer;

    metabuffer = ReadBuffer(index, SPGIST_METAPAGE_BLKNO);

    if (ConditionalLockBuffer(metabuffer))
    {
      Page metapage = BufferGetPage(metabuffer);
      SpGistMetaPageData *metadata = SpGistPageGetMeta(metapage);

      metadata->lastUsedPages = cache->lastUsedPages;

         
                                                                  
                                                                       
                                                                    
                                                                    
                                                                   
                 
         
      ((PageHeader)metapage)->pd_lower = ((char *)metadata + sizeof(SpGistMetaPageData)) - (char *)metapage;

      MarkBufferDirty(metabuffer);
      UnlockReleaseBuffer(metabuffer);
    }
    else
    {
      ReleaseBuffer(metabuffer);
    }
  }
}

                                                                              
                                                                        
#define GET_LUP(c, f) (&(c)->lastUsedPages.cachedPage[((unsigned int)(f)) % SPGIST_CACHED_PAGES])

   
                                                                            
                                                                       
   
                                                                       
                                                                           
                                                                             
                                                                        
                                                                           
                                                                              
                                                                             
                                                                            
                      
   
                                                                         
                                                                             
                                                                             
                                                                               
                                                 
   
static Buffer
allocNewBuffer(Relation index, int flags)
{
  SpGistCache *cache = spgGetCache(index);
  uint16 pageflags = 0;

  if (GBUF_REQ_LEAF(flags))
  {
    pageflags |= SPGIST_LEAF;
  }
  if (GBUF_REQ_NULLS(flags))
  {
    pageflags |= SPGIST_NULLS;
  }

  for (;;)
  {
    Buffer buffer;

    buffer = SpGistNewBuffer(index);
    SpGistInitBuffer(buffer, pageflags);

    if (pageflags & SPGIST_LEAF)
    {
                                                              
      return buffer;
    }
    else
    {
      BlockNumber blkno = BufferGetBlockNumber(buffer);
      int blkFlags = GBUF_INNER_PARITY(blkno);

      if ((flags & GBUF_PARITY_MASK) == blkFlags)
      {
                                           
        return buffer;
      }
      else
      {
                                                                     
        if (pageflags & SPGIST_NULLS)
        {
          blkFlags |= GBUF_NULLS;
        }
        cache->lastUsedPages.cachedPage[blkFlags].blkno = blkno;
        cache->lastUsedPages.cachedPage[blkFlags].freeSpace = PageGetExactFreeSpace(BufferGetPage(buffer));
        UnlockReleaseBuffer(buffer);
      }
    }
  }
}

   
                                                                           
                                                                           
                                                                       
                                                               
   
                                                                        
                  
   
Buffer
SpGistGetBuffer(Relation index, int flags, int needSpace, bool *isNew)
{
  SpGistCache *cache = spgGetCache(index);
  SpGistLastUsedPage *lup;

                                                               
  if (needSpace > SPGIST_PAGE_CAPACITY)
  {
    elog(ERROR, "desired SPGiST tuple size is too big");
  }

     
                                                                   
                                                                            
                                                                         
                                                                         
                                                       
     
  needSpace += RelationGetTargetPageFreeSpace(index, SPGIST_DEFAULT_FILLFACTOR);
  needSpace = Min(needSpace, SPGIST_PAGE_CAPACITY);

                                                  
  lup = GET_LUP(cache, flags);

                                                                      
  if (lup->blkno == InvalidBlockNumber)
  {
    *isNew = true;
    return allocNewBuffer(index, flags);
  }

                                            
  Assert(!SpGistBlockIsFixed(lup->blkno));

                                                                          
  if (lup->freeSpace >= needSpace)
  {
    Buffer buffer;
    Page page;

    buffer = ReadBuffer(index, lup->blkno);

    if (!ConditionalLockBuffer(buffer))
    {
         
                                                                     
         
      ReleaseBuffer(buffer);
      *isNew = true;
      return allocNewBuffer(index, flags);
    }

    page = BufferGetPage(buffer);

    if (PageIsNew(page) || SpGistPageIsDeleted(page) || PageIsEmpty(page))
    {
                                     
      uint16 pageflags = 0;

      if (GBUF_REQ_LEAF(flags))
      {
        pageflags |= SPGIST_LEAF;
      }
      if (GBUF_REQ_NULLS(flags))
      {
        pageflags |= SPGIST_NULLS;
      }
      SpGistInitBuffer(buffer, pageflags);
      lup->freeSpace = PageGetExactFreeSpace(page) - needSpace;
      *isNew = true;
      return buffer;
    }

       
                                                                       
                                                                  
       
    if ((GBUF_REQ_LEAF(flags) ? SpGistPageIsLeaf(page) : !SpGistPageIsLeaf(page)) && (GBUF_REQ_NULLS(flags) ? SpGistPageStoresNulls(page) : !SpGistPageStoresNulls(page)))
    {
      int freeSpace = PageGetExactFreeSpace(page);

      if (freeSpace >= needSpace)
      {
                                                                  
        lup->freeSpace = freeSpace - needSpace;
        *isNew = false;
        return buffer;
      }
    }

       
                                            
       
    UnlockReleaseBuffer(buffer);
  }

                                                     
  *isNew = true;
  return allocNewBuffer(index, flags);
}

   
                                                          
   
                                                                           
                                                                           
                           
   
void
SpGistSetLastUsedPage(Relation index, Buffer buffer)
{
  SpGistCache *cache = spgGetCache(index);
  SpGistLastUsedPage *lup;
  int freeSpace;
  Page page = BufferGetPage(buffer);
  BlockNumber blkno = BufferGetBlockNumber(buffer);
  int flags;

                                                             
  if (SpGistBlockIsFixed(blkno))
  {
    return;
  }

  if (SpGistPageIsLeaf(page))
  {
    flags = GBUF_LEAF;
  }
  else
  {
    flags = GBUF_INNER_PARITY(blkno);
  }
  if (SpGistPageStoresNulls(page))
  {
    flags |= GBUF_NULLS;
  }

  lup = GET_LUP(cache, flags);

  freeSpace = PageGetExactFreeSpace(page);
  if (lup->blkno == InvalidBlockNumber || lup->blkno == blkno || lup->freeSpace < freeSpace)
  {
    lup->blkno = blkno;
    lup->freeSpace = freeSpace;
  }
}

   
                                                            
   
void
SpGistInitPage(Page page, uint16 f)
{
  SpGistPageOpaque opaque;

  PageInit(page, BLCKSZ, MAXALIGN(sizeof(SpGistPageOpaqueData)));
  opaque = SpGistPageGetOpaque(page);
  memset(opaque, 0, sizeof(SpGistPageOpaqueData));
  opaque->flags = f;
  opaque->spgist_page_id = SPGIST_PAGE_ID;
}

   
                                                             
   
void
SpGistInitBuffer(Buffer b, uint16 f)
{
  Assert(BufferGetPageSize(b) == BLCKSZ);
  SpGistInitPage(BufferGetPage(b), f);
}

   
                            
   
void
SpGistInitMetapage(Page page)
{
  SpGistMetaPageData *metadata;
  int i;

  SpGistInitPage(page, SPGIST_META);
  metadata = SpGistPageGetMeta(page);
  memset(metadata, 0, sizeof(SpGistMetaPageData));
  metadata->magicNumber = SPGIST_MAGIC_NUMBER;

                                                
  for (i = 0; i < SPGIST_CACHED_PAGES; i++)
  {
    metadata->lastUsedPages.cachedPage[i].blkno = InvalidBlockNumber;
  }

     
                                                                         
                                                                          
               
     
  ((PageHeader)page)->pd_lower = ((char *)metadata + sizeof(SpGistMetaPageData)) - (char *)page;
}

   
                                    
   
bytea *
spgoptions(Datum reloptions, bool validate)
{
  return default_reloptions(reloptions, validate, RELOPT_KIND_SPGIST);
}

   
                                                                         
                                                                 
                                                                    
                                                                    
   
unsigned int
SpGistGetTypeSize(SpGistTypeDesc *att, Datum datum)
{
  unsigned int size;

  if (att->attbyval)
  {
    size = sizeof(Datum);
  }
  else if (att->attlen > 0)
  {
    size = att->attlen;
  }
  else
  {
    size = VARSIZE_ANY(datum);
  }

  return MAXALIGN(size);
}

   
                                            
   
static void
memcpyDatum(void *target, SpGistTypeDesc *att, Datum datum)
{
  unsigned int size;

  if (att->attbyval)
  {
    memcpy(target, &datum, sizeof(Datum));
  }
  else
  {
    size = (att->attlen > 0) ? att->attlen : VARSIZE_ANY(datum);
    memcpy(target, DatumGetPointer(datum), size);
  }
}

   
                                                                        
   
SpGistLeafTuple
spgFormLeafTuple(SpGistState *state, ItemPointer heapPtr, Datum datum, bool isnull)
{
  SpGistLeafTuple tup;
  unsigned int size;

                                                                
  size = SGLTHDRSZ;
  if (!isnull)
  {
    size += SpGistGetTypeSize(&state->attLeafType, datum);
  }

     
                                                                         
                                                          
     
  if (size < SGDTSIZE)
  {
    size = SGDTSIZE;
  }

                          
  tup = (SpGistLeafTuple)palloc0(size);

  tup->size = size;
  tup->nextOffset = InvalidOffsetNumber;
  tup->heapPtr = *heapPtr;
  if (!isnull)
  {
    memcpyDatum(SGLTDATAPTR(tup), &state->attLeafType, datum);
  }

  return tup;
}

   
                                                                           
   
                                                                             
                
   
SpGistNodeTuple
spgFormNodeTuple(SpGistState *state, Datum label, bool isnull)
{
  SpGistNodeTuple tup;
  unsigned int size;
  unsigned short infomask = 0;

                                                                
  size = SGNTHDRSZ;
  if (!isnull)
  {
    size += SpGistGetTypeSize(&state->attLabelType, label);
  }

     
                                                                           
                
     
  if ((size & INDEX_SIZE_MASK) != size)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("index row requires %zu bytes, maximum size is %zu", (Size)size, (Size)INDEX_SIZE_MASK)));
  }

  tup = (SpGistNodeTuple)palloc0(size);

  if (isnull)
  {
    infomask |= INDEX_NULL_MASK;
  }
                                                      
  infomask |= size;
  tup->t_info = infomask;

                                             
  ItemPointerSetInvalid(&tup->t_tid);

  if (!isnull)
  {
    memcpyDatum(SGNTDATAPTR(tup), &state->attLabelType, label);
  }

  return tup;
}

   
                                                                       
   
SpGistInnerTuple
spgFormInnerTuple(SpGistState *state, bool hasPrefix, Datum prefix, int nNodes, SpGistNodeTuple *nodes)
{
  SpGistInnerTuple tup;
  unsigned int size;
  unsigned int prefixSize;
  int i;
  char *ptr;

                           
  if (hasPrefix)
  {
    prefixSize = SpGistGetTypeSize(&state->attPrefixType, prefix);
  }
  else
  {
    prefixSize = 0;
  }

  size = SGITHDRSZ + prefixSize;

                                                                  
  for (i = 0; i < nNodes; i++)
  {
    size += IndexTupleSize(nodes[i]);
  }

     
                                                                         
                                                                         
     
  if (size < SGDTSIZE)
  {
    size = SGDTSIZE;
  }

     
                                                         
     
  if (size > SPGIST_PAGE_CAPACITY - sizeof(ItemIdData))
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("SP-GiST inner tuple size %zu exceeds maximum %zu", (Size)size, SPGIST_PAGE_CAPACITY - sizeof(ItemIdData)), errhint("Values larger than a buffer page cannot be indexed.")));
  }

     
                                                                        
                                            
     
  if (size > SGITMAXSIZE || prefixSize > SGITMAXPREFIXSIZE || nNodes > SGITMAXNNODES)
  {
    elog(ERROR, "SPGiST inner tuple header field is too small");
  }

                          
  tup = (SpGistInnerTuple)palloc0(size);

  tup->nNodes = nNodes;
  tup->prefixSize = prefixSize;
  tup->size = size;

  if (hasPrefix)
  {
    memcpyDatum(SGITDATAPTR(tup), &state->attPrefixType, prefix);
  }

  ptr = (char *)SGITNODEPTR(tup);

  for (i = 0; i < nNodes; i++)
  {
    SpGistNodeTuple node = nodes[i];

    memcpy(ptr, node, IndexTupleSize(node));
    ptr += IndexTupleSize(node);
  }

  return tup;
}

   
                                                              
   
                                                                         
                                                                        
                                             
   
                                                                          
                                                                         
                                             
   
SpGistDeadTuple
spgFormDeadTuple(SpGistState *state, int tupstate, BlockNumber blkno, OffsetNumber offnum)
{
  SpGistDeadTuple tuple = (SpGistDeadTuple)state->deadTupleStorage;

  tuple->tupstate = tupstate;
  tuple->size = SGDTSIZE;
  tuple->nextOffset = InvalidOffsetNumber;

  if (tupstate == SPGIST_REDIRECT)
  {
    ItemPointerSet(&tuple->pointer, blkno, offnum);
    Assert(TransactionIdIsValid(state->myXid));
    tuple->xid = state->myXid;
  }
  else
  {
    ItemPointerSetInvalid(&tuple->pointer);
    tuple->xid = InvalidTransactionId;
  }

  return tuple;
}

   
                                                           
   
                                          
   
Datum *
spgExtractNodeLabels(SpGistState *state, SpGistInnerTuple innerTuple)
{
  Datum *nodeLabels;
  int i;
  SpGistNodeTuple node;

                                                    
  node = SGITNODEPTR(innerTuple);
  if (IndexTupleHasNulls(node))
  {
    SGITITERATE(innerTuple, i, node)
    {
      if (!IndexTupleHasNulls(node))
      {
        elog(ERROR, "some but not all node labels are null in SPGiST inner tuple");
      }
    }
                                               
    return NULL;
  }
  else
  {
    nodeLabels = (Datum *)palloc(sizeof(Datum) * innerTuple->nNodes);
    SGITITERATE(innerTuple, i, node)
    {
      if (IndexTupleHasNulls(node))
      {
        elog(ERROR, "some but not all node labels are null in SPGiST inner tuple");
      }
      nodeLabels[i] = SGNTDATUM(node, state);
    }
    return nodeLabels;
  }
}

   
                                                                         
                                                                            
   
                                                                     
                                                                            
                                            
   
                                                                     
                                              
   
OffsetNumber
SpGistPageAddNewItem(SpGistState *state, Page page, Item item, Size size, OffsetNumber *startOffset, bool errorOK)
{
  SpGistPageOpaque opaque = SpGistPageGetOpaque(page);
  OffsetNumber i, maxoff, offnum;

  if (opaque->nPlaceholder > 0 && PageGetExactFreeSpace(page) + SGDTSIZE >= MAXALIGN(size))
  {
                                      
    maxoff = PageGetMaxOffsetNumber(page);
    offnum = InvalidOffsetNumber;

    for (;;)
    {
      if (startOffset && *startOffset != InvalidOffsetNumber)
      {
        i = *startOffset;
      }
      else
      {
        i = FirstOffsetNumber;
      }
      for (; i <= maxoff; i++)
      {
        SpGistDeadTuple it = (SpGistDeadTuple)PageGetItem(page, PageGetItemId(page, i));

        if (it->tupstate == SPGIST_PLACEHOLDER)
        {
          offnum = i;
          break;
        }
      }

                                          
      if (offnum != InvalidOffsetNumber)
      {
        break;
      }

      if (startOffset && *startOffset != InvalidOffsetNumber)
      {
                                                        
        *startOffset = InvalidOffsetNumber;
        continue;
      }

                                      
      opaque->nPlaceholder = 0;
      break;
    }

    if (offnum != InvalidOffsetNumber)
    {
                                         
      PageIndexTupleDelete(page, offnum);

      offnum = PageAddItem(page, item, size, offnum, false, false);

         
                                                                      
                                                                       
                                                                  
                                                                       
         
      if (offnum != InvalidOffsetNumber)
      {
        Assert(opaque->nPlaceholder > 0);
        opaque->nPlaceholder--;
        if (startOffset)
        {
          *startOffset = offnum + 1;
        }
      }
      else
      {
        elog(PANIC, "failed to add item of size %u to SPGiST index page", (int)size);
      }

      return offnum;
    }
  }

                                                                      
  offnum = PageAddItem(page, item, size, InvalidOffsetNumber, false, false);

  if (offnum == InvalidOffsetNumber && !errorOK)
  {
    elog(ERROR, "failed to add item of size %u to SPGiST index page", (int)size);
  }

  return offnum;
}

   
                                                         
   
                                                                               
                                                            
   
bool
spgproperty(Oid index_oid, int attno, IndexAMProperty prop, const char *propname, bool *res, bool *isnull)
{
  Oid opclass, opfamily, opcintype;
  CatCList *catlist;
  int i;

                                          
  if (attno == 0)
  {
    return false;
  }

  switch (prop)
  {
  case AMPROP_DISTANCE_ORDERABLE:
    break;
  default:
    return false;
  }

     
                                                                       
                                                                           
                                                                    
     

                                                   
  opclass = get_index_column_opclass(index_oid, attno);
  if (!OidIsValid(opclass))
  {
    *isnull = true;
    return true;
  }

                                                          
  if (!get_opclass_opfamily_and_input_type(opclass, &opfamily, &opcintype))
  {
    *isnull = true;
    return true;
  }

                                                              
  catlist = SearchSysCacheList1(AMOPSTRATEGY, ObjectIdGetDatum(opfamily));

  *res = false;

  for (i = 0; i < catlist->n_members; i++)
  {
    HeapTuple amoptup = &catlist->members[i]->tuple;
    Form_pg_amop amopform = (Form_pg_amop)GETSTRUCT(amoptup);

    if (amopform->amoppurpose == AMOP_ORDER && (amopform->amoplefttype == opcintype || amopform->amoprighttype == opcintype) && opfamily_can_sort_type(amopform->amopsortfamily, get_op_rettype(amopform->amopopr)))
    {
      *res = true;
      break;
    }
  }

  ReleaseSysCacheList(catlist);

  *isnull = false;

  return true;
}
