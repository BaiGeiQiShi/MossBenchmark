                                                                            
   
             
                                                          
   
   
                                                                         
                                                                        
   
                  
                                      
                                                                            
   

#include "postgres.h"

#include "access/gin_private.h"
#include "access/relscan.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/rel.h"

IndexScanDesc
ginbeginscan(Relation rel, int nkeys, int norderbys)
{
  IndexScanDesc scan;
  GinScanOpaque so;

                                     
  Assert(norderbys == 0);

  scan = RelationGetIndexScan(rel, nkeys, norderbys);

                                  
  so = (GinScanOpaque)palloc(sizeof(GinScanOpaqueData));
  so->keys = NULL;
  so->nkeys = 0;
  so->tempCtx = AllocSetContextCreate(CurrentMemoryContext, "Gin scan temporary context", ALLOCSET_DEFAULT_SIZES);
  so->keyCtx = AllocSetContextCreate(CurrentMemoryContext, "Gin scan key context", ALLOCSET_DEFAULT_SIZES);
  initGinState(&so->ginstate, scan->indexRelation);

  scan->opaque = so;

  return scan;
}

   
                                                                       
                                
   
static GinScanEntry
ginFillScanEntry(GinScanOpaque so, OffsetNumber attnum, StrategyNumber strategy, int32 searchMode, Datum queryKey, GinNullCategory queryCategory, bool isPartialMatch, Pointer extra_data)
{
  GinState *ginstate = &so->ginstate;
  GinScanEntry scanEntry;
  uint32 i;

     
                                            
     
                                                                            
                                                                      
     
  if (extra_data == NULL)
  {
    for (i = 0; i < so->totalentries; i++)
    {
      GinScanEntry prevEntry = so->entries[i];

      if (prevEntry->extra_data == NULL && prevEntry->isPartialMatch == isPartialMatch && prevEntry->strategy == strategy && prevEntry->searchMode == searchMode && prevEntry->attnum == attnum && ginCompareEntries(ginstate, attnum, prevEntry->queryKey, prevEntry->queryCategory, queryKey, queryCategory) == 0)
      {
                              
        return prevEntry;
      }
    }
  }

                                
  scanEntry = (GinScanEntry)palloc(sizeof(GinScanEntryData));
  scanEntry->queryKey = queryKey;
  scanEntry->queryCategory = queryCategory;
  scanEntry->isPartialMatch = isPartialMatch;
  scanEntry->extra_data = extra_data;
  scanEntry->strategy = strategy;
  scanEntry->searchMode = searchMode;
  scanEntry->attnum = attnum;

  scanEntry->buffer = InvalidBuffer;
  ItemPointerSetMin(&scanEntry->curItem);
  scanEntry->matchBitmap = NULL;
  scanEntry->matchIterator = NULL;
  scanEntry->matchResult = NULL;
  scanEntry->list = NULL;
  scanEntry->nlist = 0;
  scanEntry->offset = InvalidOffsetNumber;
  scanEntry->isFinished = false;
  scanEntry->reduceResult = false;

                            
  if (so->totalentries >= so->allocentries)
  {
    so->allocentries *= 2;
    so->entries = (GinScanEntry *)repalloc(so->entries, so->allocentries * sizeof(GinScanEntry));
  }
  so->entries[so->totalentries++] = scanEntry;

  return scanEntry;
}

   
                                                                           
   
static void
ginFillScanKey(GinScanOpaque so, OffsetNumber attnum, StrategyNumber strategy, int32 searchMode, Datum query, uint32 nQueryValues, Datum *queryValues, GinNullCategory *queryCategories, bool *partial_matches, Pointer *extra_data)
{
  GinScanKey key = &(so->keys[so->nkeys++]);
  GinState *ginstate = &so->ginstate;
  uint32 nUserQueryValues = nQueryValues;
  uint32 i;

                                                                   
  if (searchMode != GIN_SEARCH_MODE_DEFAULT)
  {
    nQueryValues++;
  }
  key->nentries = nQueryValues;
  key->nuserentries = nUserQueryValues;

  key->scanEntry = (GinScanEntry *)palloc(sizeof(GinScanEntry) * nQueryValues);
  key->entryRes = (GinTernaryValue *)palloc0(sizeof(GinTernaryValue) * nQueryValues);

  key->query = query;
  key->queryValues = queryValues;
  key->queryCategories = queryCategories;
  key->extra_data = extra_data;
  key->strategy = strategy;
  key->searchMode = searchMode;
  key->attnum = attnum;

  ItemPointerSetMin(&key->curItem);
  key->curItemMatches = false;
  key->recheckCurItem = false;
  key->isFinished = false;
  key->nrequired = 0;
  key->nadditional = 0;
  key->requiredEntries = NULL;
  key->additionalEntries = NULL;

  ginInitConsistentFunction(ginstate, key);

  for (i = 0; i < nQueryValues; i++)
  {
    Datum queryKey;
    GinNullCategory queryCategory;
    bool isPartialMatch;
    Pointer this_extra;

    if (i < nUserQueryValues)
    {
                                                              
      queryKey = queryValues[i];
      queryCategory = queryCategories[i];
      isPartialMatch = (ginstate->canPartialMatch[attnum - 1] && partial_matches) ? partial_matches[i] : false;
      this_extra = (extra_data) ? extra_data[i] : NULL;
    }
    else
    {
                               
      queryKey = (Datum)0;
      switch (searchMode)
      {
      case GIN_SEARCH_MODE_INCLUDE_EMPTY:
        queryCategory = GIN_CAT_EMPTY_ITEM;
        break;
      case GIN_SEARCH_MODE_ALL:
        queryCategory = GIN_CAT_EMPTY_QUERY;
        break;
      case GIN_SEARCH_MODE_EVERYTHING:
        queryCategory = GIN_CAT_EMPTY_QUERY;
        break;
      default:
        elog(ERROR, "unexpected searchMode: %d", searchMode);
        queryCategory = 0;                          
        break;
      }
      isPartialMatch = false;
      this_extra = NULL;

         
                                                                       
                                                                     
                                                                     
                                                                       
                                                                
         
      strategy = InvalidStrategy;
    }

    key->scanEntry[i] = ginFillScanEntry(so, attnum, strategy, searchMode, queryKey, queryCategory, isPartialMatch, this_extra);
  }
}

   
                                      
   
void
ginFreeScanKeys(GinScanOpaque so)
{
  uint32 i;

  if (so->keys == NULL)
  {
    return;
  }

  for (i = 0; i < so->totalentries; i++)
  {
    GinScanEntry entry = so->entries[i];

    if (entry->buffer != InvalidBuffer)
    {
      ReleaseBuffer(entry->buffer);
    }
    if (entry->list)
    {
      pfree(entry->list);
    }
    if (entry->matchIterator)
    {
      tbm_end_iterate(entry->matchIterator);
    }
    if (entry->matchBitmap)
    {
      tbm_free(entry->matchBitmap);
    }
  }

  MemoryContextResetAndDeleteChildren(so->keyCtx);

  so->keys = NULL;
  so->nkeys = 0;
  so->entries = NULL;
  so->totalentries = 0;
}

void
ginNewScanKey(IndexScanDesc scan)
{
  ScanKey scankey = scan->keyData;
  GinScanOpaque so = (GinScanOpaque)scan->opaque;
  int i;
  bool hasNullQuery = false;
  MemoryContext oldCtx;

     
                                                                   
                                                                           
                                     
     
  oldCtx = MemoryContextSwitchTo(so->keyCtx);

                                                                      
  so->keys = (GinScanKey)palloc(Max(scan->numberOfKeys, 1) * sizeof(GinScanKeyData));
  so->nkeys = 0;

                                                            
  so->totalentries = 0;
  so->allocentries = 32;
  so->entries = (GinScanEntry *)palloc(so->allocentries * sizeof(GinScanEntry));

  so->isVoidRes = false;

  for (i = 0; i < scan->numberOfKeys; i++)
  {
    ScanKey skey = &scankey[i];
    Datum *queryValues;
    int32 nQueryValues = 0;
    bool *partial_matches = NULL;
    Pointer *extra_data = NULL;
    bool *nullFlags = NULL;
    GinNullCategory *categories;
    int32 searchMode = GIN_SEARCH_MODE_DEFAULT;

       
                                                                          
                                              
       
    if (skey->sk_flags & SK_ISNULL)
    {
      so->isVoidRes = true;
      break;
    }

                                       
    queryValues = (Datum *)DatumGetPointer(FunctionCall7Coll(&so->ginstate.extractQueryFn[skey->sk_attno - 1], so->ginstate.supportCollation[skey->sk_attno - 1], skey->sk_argument, PointerGetDatum(&nQueryValues), UInt16GetDatum(skey->sk_strategy), PointerGetDatum(&partial_matches), PointerGetDatum(&extra_data), PointerGetDatum(&nullFlags), PointerGetDatum(&searchMode)));

       
                                                                           
                                                             
                                   
       
    if (searchMode < GIN_SEARCH_MODE_DEFAULT || searchMode > GIN_SEARCH_MODE_ALL)
    {
      searchMode = GIN_SEARCH_MODE_ALL;
    }

                                                                  
    if (searchMode != GIN_SEARCH_MODE_DEFAULT)
    {
      hasNullQuery = true;
    }

       
                                                              
       
    if (queryValues == NULL || nQueryValues <= 0)
    {
      if (searchMode == GIN_SEARCH_MODE_DEFAULT)
      {
        so->isVoidRes = true;
        break;
      }
      nQueryValues = 0;                        
    }

       
                                                                     
                                                                          
                                                              
       
    categories = (GinNullCategory *)palloc0(nQueryValues * sizeof(GinNullCategory));
    if (nullFlags)
    {
      int32 j;

      for (j = 0; j < nQueryValues; j++)
      {
        if (nullFlags[j])
        {
          categories[j] = GIN_CAT_NULL_KEY;
          hasNullQuery = true;
        }
      }
    }

    ginFillScanKey(so, skey->sk_attno, skey->sk_strategy, searchMode, skey->sk_argument, nQueryValues, queryValues, categories, partial_matches, extra_data);
  }

     
                                                                          
                              
     
  if (so->nkeys == 0 && !so->isVoidRes)
  {
    hasNullQuery = true;
    ginFillScanKey(so, FirstOffsetNumber, InvalidStrategy, GIN_SEARCH_MODE_EVERYTHING, (Datum)0, 0, NULL, NULL, NULL, NULL);
  }

     
                                                                       
                                                                         
                                        
     
  if (hasNullQuery && !so->isVoidRes)
  {
    GinStatsData ginStats;

    ginGetStats(scan->indexRelation, &ginStats);
    if (ginStats.ginVersion < 1)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("old GIN indexes do not support whole-index scans nor searches for nulls"), errhint("To fix this, do REINDEX INDEX \"%s\".", RelationGetRelationName(scan->indexRelation))));
    }
  }

  MemoryContextSwitchTo(oldCtx);

  pgstat_count_index_scan(scan->indexRelation);
}

void
ginrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{
  GinScanOpaque so = (GinScanOpaque)scan->opaque;

  ginFreeScanKeys(so);

  if (scankey && scan->numberOfKeys > 0)
  {
    memmove(scan->keyData, scankey, scan->numberOfKeys * sizeof(ScanKeyData));
  }
}

void
ginendscan(IndexScanDesc scan)
{
  GinScanOpaque so = (GinScanOpaque)scan->opaque;

  ginFreeScanKeys(so);

  MemoryContextDelete(so->tempCtx);
  MemoryContextDelete(so->keyCtx);

  pfree(so);
}
