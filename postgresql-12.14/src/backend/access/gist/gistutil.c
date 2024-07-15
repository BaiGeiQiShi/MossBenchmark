                                                                            
   
              
                                                                   
   
   
                                                                         
                                                                        
   
                  
                                        
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/gist_private.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "catalog/pg_opclass.h"
#include "storage/indexfsm.h"
#include "storage/lmgr.h"
#include "utils/float.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"
#include "utils/lsyscache.h"

   
                                                            
   
void
gistfillbuffer(Page page, IndexTuple *itup, int len, OffsetNumber off)
{
  OffsetNumber l = InvalidOffsetNumber;
  int i;

  if (off == InvalidOffsetNumber)
  {
    off = (PageIsEmpty(page)) ? FirstOffsetNumber : OffsetNumberNext(PageGetMaxOffsetNumber(page));
  }

  for (i = 0; i < len; i++)
  {
    Size sz = IndexTupleSize(itup[i]);

    l = PageAddItem(page, (Item)itup[i], sz, off, false, false);
    if (l == InvalidOffsetNumber)
    {
      elog(ERROR, "failed to add item to GiST index page, item %d out of %d, size %d bytes", i, len, (int)sz);
    }
    off++;
  }
}

   
                                       
   
bool
gistnospace(Page page, IndexTuple *itvec, int len, OffsetNumber todelete, Size freespace)
{
  unsigned int size = freespace, deleted = 0;
  int i;

  for (i = 0; i < len; i++)
  {
    size += IndexTupleSize(itvec[i]) + sizeof(ItemIdData);
  }

  if (todelete != InvalidOffsetNumber)
  {
    IndexTuple itup = (IndexTuple)PageGetItem(page, PageGetItemId(page, todelete));

    deleted = IndexTupleSize(itup) + sizeof(ItemIdData);
  }

  return (PageGetFreeSpace(page) + deleted < size);
}

bool
gistfitpage(IndexTuple *itvec, int len)
{
  int i;
  Size size = 0;

  for (i = 0; i < len; i++)
  {
    size += IndexTupleSize(itvec[i]) + sizeof(ItemIdData);
  }

                                 
  return (size <= GiSTPageSize);
}

   
                                
   
IndexTuple *
gistextractpage(Page page, int *len          )
{
  OffsetNumber i, maxoff;
  IndexTuple *itvec;

  maxoff = PageGetMaxOffsetNumber(page);
  *len = maxoff;
  itvec = palloc(sizeof(IndexTuple) * maxoff);
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    itvec[i - FirstOffsetNumber] = (IndexTuple)PageGetItem(page, PageGetItemId(page, i));
  }

  return itvec;
}

   
                             
   
IndexTuple *
gistjoinvector(IndexTuple *itvec, int *len, IndexTuple *additvec, int addlen)
{
  itvec = (IndexTuple *)repalloc((void *)itvec, sizeof(IndexTuple) * ((*len) + addlen));
  memmove(&itvec[*len], additvec, sizeof(IndexTuple) * addlen);
  *len += addlen;
  return itvec;
}

   
                               
   

IndexTupleData *
gistfillitupvec(IndexTuple *vec, int veclen, int *memlen)
{
  char *ptr, *ret;
  int i;

  *memlen = 0;

  for (i = 0; i < veclen; i++)
  {
    *memlen += IndexTupleSize(vec[i]);
  }

  ptr = ret = palloc(*memlen);

  for (i = 0; i < veclen; i++)
  {
    memcpy(ptr, vec[i], IndexTupleSize(vec[i]));
    ptr += IndexTupleSize(vec[i]);
  }

  return (IndexTupleData *)ret;
}

   
                                                                                
                                                          
                                       
   
void
gistMakeUnionItVec(GISTSTATE *giststate, IndexTuple *itvec, int len, Datum *attr, bool *isnull)
{
  int i;
  GistEntryVector *evec;
  int attrsize;

  evec = (GistEntryVector *)palloc((len + 2) * sizeof(GISTENTRY) + GEVHDRSZ);

  for (i = 0; i < giststate->nonLeafTupdesc->natts; i++)
  {
    int j;

                                                 
    evec->n = 0;
    for (j = 0; j < len; j++)
    {
      Datum datum;
      bool IsNull;

      datum = index_getattr(itvec[j], i + 1, giststate->leafTupdesc, &IsNull);
      if (IsNull)
      {
        continue;
      }

      gistdentryinit(giststate, i, evec->vector + evec->n, datum, NULL, NULL, (OffsetNumber)0, false, IsNull);
      evec->n++;
    }

                                                         
    if (evec->n == 0)
    {
      attr[i] = (Datum)0;
      isnull[i] = true;
    }
    else
    {
      if (evec->n == 1)
      {
                                                    
        evec->n = 2;
        evec->vector[1] = evec->vector[0];
      }

                                              
      attr[i] = FunctionCall2Coll(&giststate->unionFn[i], giststate->supportCollation[i], PointerGetDatum(evec), PointerGetDatum(&attrsize));

      isnull[i] = false;
    }
  }
}

   
                                                                      
                                              
   
IndexTuple
gistunion(Relation r, IndexTuple *itvec, int len, GISTSTATE *giststate)
{
  Datum attr[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];

  gistMakeUnionItVec(giststate, itvec, len, attr, isnull);

  return gistFormTuple(giststate, r, attr, isnull, false);
}

   
                          
   
void
gistMakeUnionKey(GISTSTATE *giststate, int attno, GISTENTRY *entry1, bool isnull1, GISTENTRY *entry2, bool isnull2, Datum *dst, bool *dstisnull)
{
                                                                  
  union
  {
    GistEntryVector gev;
    char padding[2 * sizeof(GISTENTRY) + GEVHDRSZ];
  } storage;
  GistEntryVector *evec = &storage.gev;
  int dstsize;

  evec->n = 2;

  if (isnull1 && isnull2)
  {
    *dstisnull = true;
    *dst = (Datum)0;
  }
  else
  {
    if (isnull1 == false && isnull2 == false)
    {
      evec->vector[0] = *entry1;
      evec->vector[1] = *entry2;
    }
    else if (isnull1 == false)
    {
      evec->vector[0] = *entry1;
      evec->vector[1] = *entry1;
    }
    else
    {
      evec->vector[0] = *entry2;
      evec->vector[1] = *entry2;
    }

    *dstisnull = false;
    *dst = FunctionCall2Coll(&giststate->unionFn[attno], giststate->supportCollation[attno], PointerGetDatum(evec), PointerGetDatum(&dstsize));
  }
}

bool
gistKeyIsEQ(GISTSTATE *giststate, int attno, Datum a, Datum b)
{
  bool result;

  FunctionCall3Coll(&giststate->equalFn[attno], giststate->supportCollation[attno], a, b, PointerGetDatum(&result));
  return result;
}

   
                                
   
void
gistDeCompressAtt(GISTSTATE *giststate, Relation r, IndexTuple tuple, Page p, OffsetNumber o, GISTENTRY *attdata, bool *isnull)
{
  int i;

  for (i = 0; i < IndexRelationGetNumberOfKeyAttributes(r); i++)
  {
    Datum datum;

    datum = index_getattr(tuple, i + 1, giststate->leafTupdesc, &isnull[i]);
    gistdentryinit(giststate, i, &attdata[i], datum, r, p, o, false, isnull[i]);
  }
}

   
                                                                         
   
IndexTuple
gistgetadjusted(Relation r, IndexTuple oldtup, IndexTuple addtup, GISTSTATE *giststate)
{
  bool neednew = false;
  GISTENTRY oldentries[INDEX_MAX_KEYS], addentries[INDEX_MAX_KEYS];
  bool oldisnull[INDEX_MAX_KEYS], addisnull[INDEX_MAX_KEYS];
  Datum attr[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  IndexTuple newtup = NULL;
  int i;

  gistDeCompressAtt(giststate, r, oldtup, NULL, (OffsetNumber)0, oldentries, oldisnull);

  gistDeCompressAtt(giststate, r, addtup, NULL, (OffsetNumber)0, addentries, addisnull);

  for (i = 0; i < IndexRelationGetNumberOfKeyAttributes(r); i++)
  {
    gistMakeUnionKey(giststate, i, oldentries + i, oldisnull[i], addentries + i, addisnull[i], attr + i, isnull + i);

    if (neednew)
    {
                                                         
      continue;
    }

    if (isnull[i])
    {
                                                                      
      continue;
    }

    if (!addisnull[i])
    {
      if (oldisnull[i] || !gistKeyIsEQ(giststate, i, oldentries[i].key, attr[i]))
      {
        neednew = true;
      }
    }
  }

  if (neednew)
  {
                            
    newtup = gistFormTuple(giststate, r, attr, isnull, false);
    newtup->t_tid = oldtup->t_tid;
  }

  return newtup;
}

   
                                                                              
                                           
   
                                                       
   
OffsetNumber
gistchoose(Relation r, Page p, IndexTuple it,                              
    GISTSTATE *giststate)
{
  OffsetNumber result;
  OffsetNumber maxoff;
  OffsetNumber i;
  float best_penalty[INDEX_MAX_KEYS];
  GISTENTRY entry, identry[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  int keep_current_best;

  Assert(!GistPageIsLeaf(p));

  gistDeCompressAtt(giststate, r, it, NULL, (OffsetNumber)0, identry, isnull);

                                                                          
  result = FirstOffsetNumber;

     
                                                                          
                                                                             
                                                                            
                                                          
     
                                                                           
                                                                        
                                          
     
  best_penalty[0] = -1;

     
                                                                             
                                                                            
                                                                      
                                                                         
                                                                            
                                                                           
                                                                             
                                                                      
                                                                             
                                                                            
                                                                          
                                                                             
                                                                           
                                                        
     
                                                                           
                                                                      
                                                                     
                                                                          
                                                                             
                                                            
     
  keep_current_best = -1;

     
                               
     
  maxoff = PageGetMaxOffsetNumber(p);
  Assert(maxoff >= FirstOffsetNumber);

  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    IndexTuple itup = (IndexTuple)PageGetItem(p, PageGetItemId(p, i));
    bool zero_penalty;
    int j;

    zero_penalty = true;

                                     
    for (j = 0; j < IndexRelationGetNumberOfKeyAttributes(r); j++)
    {
      Datum datum;
      float usize;
      bool IsNull;

                                            
      datum = index_getattr(itup, j + 1, giststate->leafTupdesc, &IsNull);
      gistdentryinit(giststate, j, &entry, datum, r, p, i, false, IsNull);
      usize = gistpenalty(giststate, j, &entry, IsNull, &identry[j], isnull[j]);
      if (usize > 0)
      {
        zero_penalty = false;
      }

      if (best_penalty[j] < 0 || usize < best_penalty[j])
      {
           
                                                                       
                                                                       
                                                                   
                                                                       
                                                                     
                                                                
           
        result = i;
        best_penalty[j] = usize;

        if (j < IndexRelationGetNumberOfKeyAttributes(r) - 1)
        {
          best_penalty[j + 1] = -1;
        }

                                                         
        keep_current_best = -1;
      }
      else if (best_penalty[j] == usize)
      {
           
                                                                       
                                                                    
                                         
           
      }
      else
      {
           
                                                                    
                                                                      
                                      
           
        zero_penalty = false;                               
        break;
      }
    }

       
                                                                       
                                                                   
       
    if (j == IndexRelationGetNumberOfKeyAttributes(r) && result != i)
    {
      if (keep_current_best == -1)
      {
                                                                    
        keep_current_best = (random() <= (MAX_RANDOM_VALUE / 2)) ? 1 : 0;
      }
      if (keep_current_best == 0)
      {
                                            
        result = i;
                                                                      
        keep_current_best = -1;
      }
    }

       
                                                                       
                                                                    
                                                                        
                                      
       
    if (zero_penalty)
    {
      if (keep_current_best == -1)
      {
                                                                    
        keep_current_best = (random() <= (MAX_RANDOM_VALUE / 2)) ? 1 : 0;
      }
      if (keep_current_best == 1)
      {
        break;
      }
    }
  }

  return result;
}

   
                                                              
   
void
gistdentryinit(GISTSTATE *giststate, int nkey, GISTENTRY *e, Datum k, Relation r, Page pg, OffsetNumber o, bool l, bool isNull)
{
  if (!isNull)
  {
    GISTENTRY *dep;

    gistentryinit(*e, k, r, pg, o, l);

                                                           
    if (!OidIsValid(giststate->decompressFn[nkey].fn_oid))
    {
      return;
    }

    dep = (GISTENTRY *)DatumGetPointer(FunctionCall1Coll(&giststate->decompressFn[nkey], giststate->supportCollation[nkey], PointerGetDatum(e)));
                                                        
    if (dep != e)
    {
      gistentryinit(*e, dep->key, dep->rel, dep->page, dep->offset, dep->leafkey);
    }
  }
  else
  {
    gistentryinit(*e, (Datum)0, r, pg, o, l);
  }
}

IndexTuple
gistFormTuple(GISTSTATE *giststate, Relation r, Datum attdata[], bool isnull[], bool isleaf)
{
  Datum compatt[INDEX_MAX_KEYS];
  int i;
  IndexTuple res;

     
                                                 
     
  for (i = 0; i < IndexRelationGetNumberOfKeyAttributes(r); i++)
  {
    if (isnull[i])
    {
      compatt[i] = (Datum)0;
    }
    else
    {
      GISTENTRY centry;
      GISTENTRY *cep;

      gistentryinit(centry, attdata[i], r, NULL, (OffsetNumber)0, isleaf);
                                                           
      if (OidIsValid(giststate->compressFn[i].fn_oid))
      {
        cep = (GISTENTRY *)DatumGetPointer(FunctionCall1Coll(&giststate->compressFn[i], giststate->supportCollation[i], PointerGetDatum(&centry)));
      }
      else
      {
        cep = &centry;
      }
      compatt[i] = cep->key;
    }
  }

  if (isleaf)
  {
       
                                               
       
    for (; i < r->rd_att->natts; i++)
    {
      if (isnull[i])
      {
        compatt[i] = (Datum)0;
      }
      else
      {
        compatt[i] = attdata[i];
      }
    }
  }

  res = index_form_tuple(isleaf ? giststate->leafTupdesc : giststate->nonLeafTupdesc, compatt, isnull);

     
                                                                             
                                   
     
  ItemPointerSetOffsetNumber(&(res->t_tid), 0xffff);
  return res;
}

   
                                                           
   
static Datum
gistFetchAtt(GISTSTATE *giststate, int nkey, Datum k, Relation r)
{
  GISTENTRY fentry;
  GISTENTRY *fep;

  gistentryinit(fentry, k, r, NULL, (OffsetNumber)0, false);

  fep = (GISTENTRY *)DatumGetPointer(FunctionCall1Coll(&giststate->fetchFn[nkey], giststate->supportCollation[nkey], PointerGetDatum(&fentry)));

                                                  
  return fep->key;
}

   
                            
                                                                   
   
HeapTuple
gistFetchTuple(GISTSTATE *giststate, Relation r, IndexTuple tuple)
{
  MemoryContext oldcxt = MemoryContextSwitchTo(giststate->tempCxt);
  Datum fetchatt[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  int i;

  for (i = 0; i < IndexRelationGetNumberOfKeyAttributes(r); i++)
  {
    Datum datum;

    datum = index_getattr(tuple, i + 1, giststate->leafTupdesc, &isnull[i]);

    if (giststate->fetchFn[i].fn_oid != InvalidOid)
    {
      if (!isnull[i])
      {
        fetchatt[i] = gistFetchAtt(giststate, i, datum, r);
      }
      else
      {
        fetchatt[i] = (Datum)0;
      }
    }
    else if (giststate->compressFn[i].fn_oid == InvalidOid)
    {
         
                                                                       
                                                                     
         
      if (!isnull[i])
      {
        fetchatt[i] = datum;
      }
      else
      {
        fetchatt[i] = (Datum)0;
      }
    }
    else
    {
         
                                                                   
                                                                       
                                                            
         
      isnull[i] = true;
      fetchatt[i] = (Datum)0;
    }
  }

     
                                  
     
  for (; i < r->rd_att->natts; i++)
  {
    fetchatt[i] = index_getattr(tuple, i + 1, giststate->leafTupdesc, &isnull[i]);
  }
  MemoryContextSwitchTo(oldcxt);

  return heap_form_tuple(giststate->fetchTupdesc, fetchatt, isnull);
}

float
gistpenalty(GISTSTATE *giststate, int attno, GISTENTRY *orig, bool isNullOrig, GISTENTRY *add, bool isNullAdd)
{
  float penalty = 0.0;

  if (giststate->penaltyFn[attno].fn_strict == false || (isNullOrig == false && isNullAdd == false))
  {
    FunctionCall3Coll(&giststate->penaltyFn[attno], giststate->supportCollation[attno], PointerGetDatum(orig), PointerGetDatum(add), PointerGetDatum(&penalty));
                                          
    if (isnan(penalty) || penalty < 0.0)
    {
      penalty = 0.0;
    }
  }
  else if (isNullOrig && isNullAdd)
  {
    penalty = 0.0;
  }
  else
  {
                                                        
    penalty = get_float4_infinity();
  }

  return penalty;
}

   
                               
   
void
GISTInitBuffer(Buffer b, uint32 f)
{
  GISTPageOpaque opaque;
  Page page;
  Size pageSize;

  pageSize = BufferGetPageSize(b);
  page = BufferGetPage(b);
  PageInit(page, pageSize, sizeof(GISTPageOpaqueData));

  opaque = GistPageGetOpaque(page);
                                                                   
                                                   
  opaque->rightlink = InvalidBlockNumber;
  opaque->flags = f;
  opaque->gist_page_id = GIST_PAGE_ID;
}

   
                                               
   
void
gistcheckpage(Relation rel, Buffer buf)
{
  Page page = BufferGetPage(buf);

     
                                                           
                                                                         
                                                                         
                    
     
  if (PageIsNew(page))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains unexpected zero page at block %u", RelationGetRelationName(rel), BufferGetBlockNumber(buf)), errhint("Please REINDEX it.")));
  }

     
                                                          
     
  if (PageGetSpecialSize(page) != MAXALIGN(sizeof(GISTPageOpaqueData)))
  {
    ereport(ERROR, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("index \"%s\" contains corrupted page at block %u", RelationGetRelationName(rel), BufferGetBlockNumber(buf)), errhint("Please REINDEX it.")));
  }
}

   
                                                                             
   
                                                              
   
                                                                             
   
Buffer
gistNewBuffer(Relation r)
{
  Buffer buffer;
  bool needLock;

                                         
  for (;;)
  {
    BlockNumber blkno = GetFreeIndexPage(r);

    if (blkno == InvalidBlockNumber)
    {
      break;                          
    }

    buffer = ReadBuffer(r, blkno);

       
                                                                          
                                                           
       
    if (ConditionalLockBuffer(buffer))
    {
      Page page = BufferGetPage(buffer);

         
                                                            
         
      if (PageIsNew(page))
      {
        return buffer;
      }

      gistcheckpage(r, buffer);

         
                                                                   
                                     
         
      if (gistPageRecyclable(page))
      {
           
                                                                      
                                                                      
                                                                  
                             
           
        if (XLogStandbyInfoActive() && RelationNeedsWAL(r))
        {
          gistXLogPageReuse(r, blkno, GistPageGetDeleteXid(page));
        }

        return buffer;
      }

      LockBuffer(buffer, GIST_UNLOCK);
    }

                                                       
    ReleaseBuffer(buffer);
  }

                            
  needLock = !RELATION_IS_LOCAL(r);

  if (needLock)
  {
    LockRelationForExtension(r, ExclusiveLock);
  }

  buffer = ReadBuffer(r, P_NEW);
  LockBuffer(buffer, GIST_EXCLUSIVE);

  if (needLock)
  {
    UnlockRelationForExtension(r, ExclusiveLock);
  }

  return buffer;
}

                                    
bool
gistPageRecyclable(Page page)
{
  if (PageIsNew(page))
  {
    return true;
  }
  if (GistPageIsDeleted(page))
  {
       
                                                                      
                                                                         
                                                                           
                    
       
                                                                      
                                                                          
                                                 
       
    FullTransactionId deletexid_full = GistPageGetDeleteXid(page);
    FullTransactionId recentxmin_full = GetFullRecentGlobalXmin();

    if (FullTransactionIdPrecedes(deletexid_full, recentxmin_full))
    {
      return true;
    }
  }
  return false;
}

bytea *
gistoptions(Datum reloptions, bool validate)
{
  relopt_value *options;
  GiSTOptions *rdopts;
  int numoptions;
  static const relopt_parse_elt tab[] = {{"fillfactor", RELOPT_TYPE_INT, offsetof(GiSTOptions, fillfactor)}, {"buffering", RELOPT_TYPE_STRING, offsetof(GiSTOptions, bufferingModeOffset)}};

  options = parseRelOptions(reloptions, validate, RELOPT_KIND_GIST, &numoptions);

                               
  if (numoptions == 0)
  {
    return NULL;
  }

  rdopts = allocateReloptStruct(sizeof(GiSTOptions), options, numoptions);

  fillRelOptions((void *)rdopts, sizeof(GiSTOptions), options, numoptions, validate, tab, lengthof(tab));

  pfree(options);

  return (bytea *)rdopts;
}

   
                                                          
   
                                                                            
                                                                            
                                                                         
   
bool
gistproperty(Oid index_oid, int attno, IndexAMProperty prop, const char *propname, bool *res, bool *isnull)
{
  Oid opclass, opfamily, opcintype;
  int16 procno;

                                          
  if (attno == 0)
  {
    return false;
  }

     
                                                                             
                                                                         
                                                                             
                                                                           
                                                                   
     
                                                                         
                                                                          
     

  switch (prop)
  {
  case AMPROP_DISTANCE_ORDERABLE:
    procno = GIST_DISTANCE_PROC;
    break;
  case AMPROP_RETURNABLE:
    procno = GIST_FETCH_PROC;
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

                                                              

  *res = SearchSysCacheExists4(AMPROCNUM, ObjectIdGetDatum(opfamily), ObjectIdGetDatum(opcintype), ObjectIdGetDatum(opcintype), Int16GetDatum(procno));

     
                                                                            
                                              
     
  if (prop == AMPROP_RETURNABLE && !*res)
  {
    *res = !SearchSysCacheExists4(AMPROCNUM, ObjectIdGetDatum(opfamily), ObjectIdGetDatum(opcintype), ObjectIdGetDatum(opcintype), Int16GetDatum(GIST_COMPRESS_PROC));
  }

  *isnull = false;

  return true;
}

   
                                                                            
                                                                          
                                      
   
XLogRecPtr
gistGetFakeLSN(Relation rel)
{
  static XLogRecPtr counter = FirstNormalUnloggedLSN;

  if (rel->rd_rel->relpersistence == RELPERSISTENCE_TEMP)
  {
       
                                                                           
                                      
       
    return counter++;
  }
  else
  {
       
                                                                          
                                                                         
       
    Assert(rel->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED);
    return GetFakeLSNForUnloggedRel();
  }
}
