                                                                            
   
              
                                                     
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tuptoaster.h"
#include "access/valid.h"
#include "access/xact.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#ifdef CATCACHE_STATS
#include "storage/ipc.h"                       
#endif
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/hashutils.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/resowner_private.h"
#include "utils/syscache.h"

                         /* turns DEBUG elogs on */

   
                                                                      
                                                                      
                                                              
   
#define HASH_INDEX(h, sz) ((Index)((h) & ((sz)-1)))

   
                                      
   

#ifdef CACHEDEBUG
#define CACHE_elog(...) elog(__VA_ARGS__)
#else
#define CACHE_elog(...)
#endif

                                                               
static CatCacheHeader *CacheHdr = NULL;

static inline HeapTuple
SearchCatCacheInternal(CatCache *cache, int nkeys, Datum v1, Datum v2, Datum v3, Datum v4);

static pg_noinline HeapTuple
SearchCatCacheMiss(CatCache *cache, int nkeys, uint32 hashValue, Index hashIndex, Datum v1, Datum v2, Datum v3, Datum v4);

static uint32
CatalogCacheComputeHashValue(CatCache *cache, int nkeys, Datum v1, Datum v2, Datum v3, Datum v4);
static uint32
CatalogCacheComputeTupleHashValue(CatCache *cache, int nkeys, HeapTuple tuple);
static inline bool
CatalogCacheCompareTuple(const CatCache *cache, int nkeys, const Datum *cachekeys, const Datum *searchkeys);

#ifdef CATCACHE_STATS
static void
CatCachePrintStats(int code, Datum arg);
#endif
static void
CatCacheRemoveCTup(CatCache *cache, CatCTup *ct);
static void
CatCacheRemoveCList(CatCache *cache, CatCList *cl);
static void
CatalogCacheInitializeCache(CatCache *cache);
static CatCTup *
CatalogCacheCreateEntry(CatCache *cache, HeapTuple ntp, Datum *arguments, uint32 hashValue, Index hashIndex, bool negative);

static void
CatCacheFreeKeys(TupleDesc tupdesc, int nkeys, int *attnos, Datum *keys);
static void
CatCacheCopyKeys(TupleDesc tupdesc, int nkeys, int *attnos, Datum *srckeys, Datum *dstkeys);

   
                                  
   

   
                                                                           
                                                                               
                                                                          
                                                                          
                                                                              
                                                                               
                                                                              
   

static bool
chareqfast(Datum a, Datum b)
{
  return DatumGetChar(a) == DatumGetChar(b);
}

static uint32
charhashfast(Datum datum)
{
  return murmurhash32((int32)DatumGetChar(datum));
}

static bool
nameeqfast(Datum a, Datum b)
{
  char *ca = NameStr(*DatumGetName(a));
  char *cb = NameStr(*DatumGetName(b));

  return strncmp(ca, cb, NAMEDATALEN) == 0;
}

static uint32
namehashfast(Datum datum)
{
  char *key = NameStr(*DatumGetName(datum));

  return hash_any((unsigned char *)key, strlen(key));
}

static bool
int2eqfast(Datum a, Datum b)
{
  return DatumGetInt16(a) == DatumGetInt16(b);
}

static uint32
int2hashfast(Datum datum)
{
  return murmurhash32((int32)DatumGetInt16(datum));
}

static bool
int4eqfast(Datum a, Datum b)
{
  return DatumGetInt32(a) == DatumGetInt32(b);
}

static uint32
int4hashfast(Datum datum)
{
  return murmurhash32((int32)DatumGetInt32(datum));
}

static bool
texteqfast(Datum a, Datum b)
{
     
                                                                         
                                                             
     
  return DatumGetBool(DirectFunctionCall2Coll(texteq, DEFAULT_COLLATION_OID, a, b));
}

static uint32
texthashfast(Datum datum)
{
                                           
  return DatumGetInt32(DirectFunctionCall1Coll(hashtext, DEFAULT_COLLATION_OID, datum));
}

static bool
oidvectoreqfast(Datum a, Datum b)
{
  return DatumGetBool(DirectFunctionCall2(oidvectoreq, a, b));
}

static uint32
oidvectorhashfast(Datum datum)
{
  return DatumGetInt32(DirectFunctionCall1(hashoidvector, datum));
}

                                          
static void
GetCCHashEqFuncs(Oid keytype, CCHashFN *hashfunc, RegProcedure *eqfunc, CCFastEqualFN *fasteqfunc)
{
  switch (keytype)
  {
  case BOOLOID:
    *hashfunc = charhashfast;
    *fasteqfunc = chareqfast;
    *eqfunc = F_BOOLEQ;
    break;
  case CHAROID:
    *hashfunc = charhashfast;
    *fasteqfunc = chareqfast;
    *eqfunc = F_CHAREQ;
    break;
  case NAMEOID:
    *hashfunc = namehashfast;
    *fasteqfunc = nameeqfast;
    *eqfunc = F_NAMEEQ;
    break;
  case INT2OID:
    *hashfunc = int2hashfast;
    *fasteqfunc = int2eqfast;
    *eqfunc = F_INT2EQ;
    break;
  case INT4OID:
    *hashfunc = int4hashfast;
    *fasteqfunc = int4eqfast;
    *eqfunc = F_INT4EQ;
    break;
  case TEXTOID:
    *hashfunc = texthashfast;
    *fasteqfunc = texteqfast;
    *eqfunc = F_TEXTEQ;
    break;
  case OIDOID:
  case REGPROCOID:
  case REGPROCEDUREOID:
  case REGOPEROID:
  case REGOPERATOROID:
  case REGCLASSOID:
  case REGTYPEOID:
  case REGCONFIGOID:
  case REGDICTIONARYOID:
  case REGROLEOID:
  case REGNAMESPACEOID:
    *hashfunc = int4hashfast;
    *fasteqfunc = int4eqfast;
    *eqfunc = F_OIDEQ;
    break;
  case OIDVECTOROID:
    *hashfunc = oidvectorhashfast;
    *fasteqfunc = oidvectoreqfast;
    *eqfunc = F_OIDVECTOREQ;
    break;
  default:
    elog(FATAL, "type %u not supported as catcache key", keytype);
    *hashfunc = NULL;                          

    *eqfunc = InvalidOid;
    break;
  }
}

   
                                 
   
                                                                     
   
static uint32
CatalogCacheComputeHashValue(CatCache *cache, int nkeys, Datum v1, Datum v2, Datum v3, Datum v4)
{
  uint32 hashValue = 0;
  uint32 oneHash;
  CCHashFN *cc_hashfunc = cache->cc_hashfunc;

  CACHE_elog(DEBUG2, "CatalogCacheComputeHashValue %s %d %p", cache->cc_relname, nkeys, cache);

  switch (nkeys)
  {
  case 4:
    oneHash = (cc_hashfunc[3])(v4);

    hashValue ^= oneHash << 24;
    hashValue ^= oneHash >> 8;
                     
  case 3:
    oneHash = (cc_hashfunc[2])(v3);

    hashValue ^= oneHash << 16;
    hashValue ^= oneHash >> 16;
                     
  case 2:
    oneHash = (cc_hashfunc[1])(v2);

    hashValue ^= oneHash << 8;
    hashValue ^= oneHash >> 24;
                     
  case 1:
    oneHash = (cc_hashfunc[0])(v1);

    hashValue ^= oneHash;
    break;
  default:
    elog(FATAL, "wrong number of hash keys: %d", nkeys);
    break;
  }

  return hashValue;
}

   
                                      
   
                                                                     
   
static uint32
CatalogCacheComputeTupleHashValue(CatCache *cache, int nkeys, HeapTuple tuple)
{
  Datum v1 = 0, v2 = 0, v3 = 0, v4 = 0;
  bool isNull = false;
  int *cc_keyno = cache->cc_keyno;
  TupleDesc cc_tupdesc = cache->cc_tupdesc;

                                                              
  switch (nkeys)
  {
  case 4:
    v4 = fastgetattr(tuple, cc_keyno[3], cc_tupdesc, &isNull);
    Assert(!isNull);
                     
  case 3:
    v3 = fastgetattr(tuple, cc_keyno[2], cc_tupdesc, &isNull);
    Assert(!isNull);
                     
  case 2:
    v2 = fastgetattr(tuple, cc_keyno[1], cc_tupdesc, &isNull);
    Assert(!isNull);
                     
  case 1:
    v1 = fastgetattr(tuple, cc_keyno[0], cc_tupdesc, &isNull);
    Assert(!isNull);
    break;
  default:
    elog(FATAL, "wrong number of hash keys: %d", nkeys);
    break;
  }

  return CatalogCacheComputeHashValue(cache, nkeys, v1, v2, v3, v4);
}

   
                             
   
                                            
   
static inline bool
CatalogCacheCompareTuple(const CatCache *cache, int nkeys, const Datum *cachekeys, const Datum *searchkeys)
{
  const CCFastEqualFN *cc_fastequal = cache->cc_fastequal;
  int i;

  for (i = 0; i < nkeys; i++)
  {
    if (!(cc_fastequal[i])(cachekeys[i], searchkeys[i]))
    {
      return false;
    }
  }
  return true;
}

#ifdef CATCACHE_STATS

static void
CatCachePrintStats(int code, Datum arg)
{
  slist_iter iter;
  long cc_searches = 0;
  long cc_hits = 0;
  long cc_neg_hits = 0;
  long cc_newloads = 0;
  long cc_invals = 0;
  long cc_lsearches = 0;
  long cc_lhits = 0;

  slist_foreach(iter, &CacheHdr->ch_caches)
  {
    CatCache *cache = slist_container(CatCache, cc_next, iter.cur);

    if (cache->cc_ntup == 0 && cache->cc_searches == 0)
    {
      continue;                                
    }
    elog(DEBUG2, "catcache %s/%u: %d tup, %ld srch, %ld+%ld=%ld hits, %ld+%ld=%ld loads, %ld invals, %ld lsrch, %ld lhits", cache->cc_relname, cache->cc_indexoid, cache->cc_ntup, cache->cc_searches, cache->cc_hits, cache->cc_neg_hits, cache->cc_hits + cache->cc_neg_hits, cache->cc_newloads, cache->cc_searches - cache->cc_hits - cache->cc_neg_hits - cache->cc_newloads, cache->cc_searches - cache->cc_hits - cache->cc_neg_hits, cache->cc_invals, cache->cc_lsearches, cache->cc_lhits);
    cc_searches += cache->cc_searches;
    cc_hits += cache->cc_hits;
    cc_neg_hits += cache->cc_neg_hits;
    cc_newloads += cache->cc_newloads;
    cc_invals += cache->cc_invals;
    cc_lsearches += cache->cc_lsearches;
    cc_lhits += cache->cc_lhits;
  }
  elog(DEBUG2, "catcache totals: %d tup, %ld srch, %ld+%ld=%ld hits, %ld+%ld=%ld loads, %ld invals, %ld lsrch, %ld lhits", CacheHdr->ch_ntup, cc_searches, cc_hits, cc_neg_hits, cc_hits + cc_neg_hits, cc_newloads, cc_searches - cc_hits - cc_neg_hits - cc_newloads, cc_searches - cc_hits - cc_neg_hits, cc_invals, cc_lsearches, cc_lhits);
}
#endif                     

   
                       
   
                                           
   
                                                                     
                                                                    
   
static void
CatCacheRemoveCTup(CatCache *cache, CatCTup *ct)
{
  Assert(ct->refcount == 0);
  Assert(ct->my_cache == cache);

  if (ct->c_list)
  {
       
                                                                       
                                                                         
                                                                
       
    ct->dead = true;
    CatCacheRemoveCList(cache, ct->c_list);
    return;                         
  }

                               
  dlist_delete(&ct->cache_elem);

     
                                                                             
                                                            
     
  if (ct->negative)
  {
    CatCacheFreeKeys(cache->cc_tupdesc, cache->cc_nkeys, cache->cc_keyno, ct->keys);
  }

  pfree(ct);

  --cache->cc_ntup;
  --CacheHdr->ch_ntup;
}

   
                        
   
                                                
   
                                                                         
   
static void
CatCacheRemoveCList(CatCache *cache, CatCList *cl)
{
  int i;

  Assert(cl->refcount == 0);
  Assert(cl->my_cache == cache);

                                 
  for (i = cl->n_members; --i >= 0;)
  {
    CatCTup *ct = cl->members[i];

    Assert(ct->c_list == cl);
    ct->c_list = NULL;
                                                                    
    if (
#ifndef CATCACHE_FORCE_RELEASE
        ct->dead &&
#endif
        ct->refcount == 0)
      CatCacheRemoveCTup(cache, ct);
  }

                               
  dlist_delete(&cl->cache_elem);

                                   
  CatCacheFreeKeys(cache->cc_tupdesc, cl->nkeys, cache->cc_keyno, cl->keys);

  pfree(cl);
}

   
                      
   
                                                                  
   
                                                                       
                                                                      
                                       
   
                                                                      
                                                                        
                                                                         
                                                                       
                                                                           
                                                                            
   
                                                                         
   
void
CatCacheInvalidate(CatCache *cache, uint32 hashValue)
{
  Index hashIndex;
  dlist_mutable_iter iter;

  CACHE_elog(DEBUG2, "CatCacheInvalidate: called");

     
                                                                            
                                                                
     

     
                                                                           
                                                           
     
  dlist_foreach_modify(iter, &cache->cc_lists)
  {
    CatCList *cl = dlist_container(CatCList, cache_elem, iter.cur);

    if (cl->refcount > 0)
    {
      cl->dead = true;
    }
    else
    {
      CatCacheRemoveCList(cache, cl);
    }
  }

     
                                                      
     
  hashIndex = HASH_INDEX(hashValue, cache->cc_nbuckets);
  dlist_foreach_modify(iter, &cache->cc_bucket[hashIndex])
  {
    CatCTup *ct = dlist_container(CatCTup, cache_elem, iter.cur);

    if (hashValue == ct->hash_value)
    {
      if (ct->refcount > 0 || (ct->c_list && ct->c_list->refcount > 0))
      {
        ct->dead = true;
                                                 
        Assert(ct->c_list == NULL || ct->c_list->dead);
      }
      else
      {
        CatCacheRemoveCTup(cache, ct);
      }
      CACHE_elog(DEBUG2, "CatCacheInvalidate: invalidated");
#ifdef CATCACHE_STATS
      cache->cc_invals++;
#endif
                                                       
    }
  }
}

                                                                    
                           
                                                                    
   

   
                                                                       
   
                                                                           
                                                                       
                                                             
   
void
CreateCacheMemoryContext(void)
{
     
                                                                            
                     
     
  if (!CacheMemoryContext)
  {
    CacheMemoryContext = AllocSetContextCreate(TopMemoryContext, "CacheMemoryContext", ALLOCSET_DEFAULT_SIZES);
  }
}

   
                      
   
                                     
   
                                                                   
                                                                         
   
static void
ResetCatalogCache(CatCache *cache)
{
  dlist_mutable_iter iter;
  int i;

                                                                
  dlist_foreach_modify(iter, &cache->cc_lists)
  {
    CatCList *cl = dlist_container(CatCList, cache_elem, iter.cur);

    if (cl->refcount > 0)
    {
      cl->dead = true;
    }
    else
    {
      CatCacheRemoveCList(cache, cl);
    }
  }

                                                                 
  for (i = 0; i < cache->cc_nbuckets; i++)
  {
    dlist_head *bucket = &cache->cc_bucket[i];

    dlist_foreach_modify(iter, bucket)
    {
      CatCTup *ct = dlist_container(CatCTup, cache_elem, iter.cur);

      if (ct->refcount > 0 || (ct->c_list && ct->c_list->refcount > 0))
      {
        ct->dead = true;
                                                 
        Assert(ct->c_list == NULL || ct->c_list->dead);
      }
      else
      {
        CatCacheRemoveCTup(cache, ct);
      }
#ifdef CATCACHE_STATS
      cache->cc_invals++;
#endif
    }
  }
}

   
                       
   
                                                              
   
void
ResetCatalogCaches(void)
{
  slist_iter iter;

  CACHE_elog(DEBUG2, "ResetCatalogCaches called");

  slist_foreach(iter, &CacheHdr->ch_caches)
  {
    CatCache *cache = slist_container(CatCache, cc_next, iter.cur);

    ResetCatalogCache(cache);
  }

  CACHE_elog(DEBUG2, "end of ResetCatalogCaches call");
}

   
                             
   
                                                                           
                                                                      
                                                                          
                                                                          
                                                                           
                                                                         
                                                                             
                                                                            
                                                                           
   
void
CatalogCacheFlushCatalog(Oid catId)
{
  slist_iter iter;

  CACHE_elog(DEBUG2, "CatalogCacheFlushCatalog called for %u", catId);

  slist_foreach(iter, &CacheHdr->ch_caches)
  {
    CatCache *cache = slist_container(CatCache, cc_next, iter.cur);

                                                             
    if (cache->cc_reloid == catId)
    {
                                          
      ResetCatalogCache(cache);

                                                                  
      CallSyscacheCallbacks(cache->id, 0);
    }
  }

  CACHE_elog(DEBUG2, "end of CatalogCacheFlushCatalog call");
}

   
                 
   
                                                                         
                                                                          
                                                                    
                                              
   
#ifdef CACHEDEBUG
#define InitCatCache_DEBUG2                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    elog(DEBUG2, "InitCatCache: rel=%u ind=%u id=%d nkeys=%d size=%d", cp->cc_reloid, cp->cc_indexoid, cp->id, cp->cc_nkeys, cp->cc_nbuckets);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)
#else
#define InitCatCache_DEBUG2
#endif

CatCache *
InitCatCache(int id, Oid reloid, Oid indexoid, int nkeys, const int *key, int nbuckets)
{
  CatCache *cp;
  MemoryContext oldcxt;
  size_t sz;
  int i;

     
                                                                             
                                                       
     
                                                                            
                                                                          
             
     
                                                                   
                                     
     
  Assert(nbuckets > 0 && (nbuckets & -nbuckets) == nbuckets);

     
                                                                           
                              
     
  if (!CacheMemoryContext)
  {
    CreateCacheMemoryContext();
  }

  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

     
                                                              
     
  if (CacheHdr == NULL)
  {
    CacheHdr = (CatCacheHeader *)palloc(sizeof(CatCacheHeader));
    slist_init(&CacheHdr->ch_caches);
    CacheHdr->ch_ntup = 0;
#ifdef CATCACHE_STATS
                                              
    on_proc_exit(CatCachePrintStats, 0);
#endif
  }

     
                                                                      
     
                                                                            
     
  sz = sizeof(CatCache) + PG_CACHE_LINE_SIZE;
  cp = (CatCache *)CACHELINEALIGN(palloc0(sz));
  cp->cc_bucket = palloc0(nbuckets * sizeof(dlist_head));

     
                                                                  
                                                                         
                                                              
     
  cp->id = id;
  cp->cc_relname = "(not known yet)";
  cp->cc_reloid = reloid;
  cp->cc_indexoid = indexoid;
  cp->cc_relisshared = false;                
  cp->cc_tupdesc = (TupleDesc)NULL;
  cp->cc_ntup = 0;
  cp->cc_nbuckets = nbuckets;
  cp->cc_nkeys = nkeys;
  for (i = 0; i < nkeys; ++i)
  {
    cp->cc_keyno[i] = key[i];
  }

     
                                                                      
                                            
     
  InitCatCache_DEBUG2;

     
                                                       
     
  slist_push_head(&CacheHdr->ch_caches, &cp->cc_next);

     
                                                 
     
  MemoryContextSwitchTo(oldcxt);

  return cp;
}

   
                                                       
   
static void
RehashCatCache(CatCache *cp)
{
  dlist_head *newbucket;
  int newnbuckets;
  int i;

  elog(DEBUG1, "rehashing catalog cache id %d for %s; %d tups, %d buckets", cp->id, cp->cc_relname, cp->cc_ntup, cp->cc_nbuckets);

                                           
  newnbuckets = cp->cc_nbuckets * 2;
  newbucket = (dlist_head *)MemoryContextAllocZero(CacheMemoryContext, newnbuckets * sizeof(dlist_head));

                                                    
  for (i = 0; i < cp->cc_nbuckets; i++)
  {
    dlist_mutable_iter iter;

    dlist_foreach_modify(iter, &cp->cc_bucket[i])
    {
      CatCTup *ct = dlist_container(CatCTup, cache_elem, iter.cur);
      int hashIndex = HASH_INDEX(ct->hash_value, newnbuckets);

      dlist_delete(iter.cur);
      dlist_push_head(&newbucket[hashIndex], &ct->cache_elem);
    }
  }

                                
  pfree(cp->cc_bucket);
  cp->cc_nbuckets = newnbuckets;
  cp->cc_bucket = newbucket;
}

   
                                
   
                                                                           
                                                                          
                                                        
   
#ifdef CACHEDEBUG
#define CatalogCacheInitializeCache_DEBUG1 elog(DEBUG2, "CatalogCacheInitializeCache: cache @%p rel=%u", cache, cache->cc_reloid)

#define CatalogCacheInitializeCache_DEBUG2                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (cache->cc_keyno[i] > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      elog(DEBUG2, "CatalogCacheInitializeCache: load %d/%d w/%d, %u", i + 1, cache->cc_nkeys, cache->cc_keyno[i], TupleDescAttr(tupdesc, cache->cc_keyno[i] - 1)->atttypid);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      elog(DEBUG2, "CatalogCacheInitializeCache: load %d/%d w/%d", i + 1, cache->cc_nkeys, cache->cc_keyno[i]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)
#else
#define CatalogCacheInitializeCache_DEBUG1
#define CatalogCacheInitializeCache_DEBUG2
#endif

static void
CatalogCacheInitializeCache(CatCache *cache)
{
  Relation relation;
  MemoryContext oldcxt;
  TupleDesc tupdesc;
  int i;

  CatalogCacheInitializeCache_DEBUG1;

  relation = table_open(cache->cc_reloid, AccessShareLock);

     
                                                                             
                      
     
  Assert(CacheMemoryContext != NULL);

  oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

     
                                                                     
     
  tupdesc = CreateTupleDescCopyConstr(RelationGetDescr(relation));

     
                                                                            
                                  
     
  cache->cc_relname = pstrdup(RelationGetRelationName(relation));
  cache->cc_relisshared = RelationGetForm(relation)->relisshared;

     
                                                             
     
  MemoryContextSwitchTo(oldcxt);

  table_close(relation, AccessShareLock);

  CACHE_elog(DEBUG2, "CatalogCacheInitializeCache: %s, %d keys", cache->cc_relname, cache->cc_nkeys);

     
                                        
     
  for (i = 0; i < cache->cc_nkeys; ++i)
  {
    Oid keytype;
    RegProcedure eqfunc;

    CatalogCacheInitializeCache_DEBUG2;

    if (cache->cc_keyno[i] > 0)
    {
      Form_pg_attribute attr = TupleDescAttr(tupdesc, cache->cc_keyno[i] - 1);

      keytype = attr->atttypid;
                                                       
      Assert(attr->attnotnull);
    }
    else
    {
      if (cache->cc_keyno[i] < 0)
      {
        elog(FATAL, "sys attributes are not supported in caches");
      }
      keytype = OIDOID;
    }

    GetCCHashEqFuncs(keytype, &cache->cc_hashfunc[i], &eqfunc, &cache->cc_fastequal[i]);

       
                                                                        
                                      
       
    fmgr_info_cxt(eqfunc, &cache->cc_skey[i].sk_func, CacheMemoryContext);

                                                                       
    cache->cc_skey[i].sk_attno = cache->cc_keyno[i];

                                                                  
    cache->cc_skey[i].sk_strategy = BTEqualStrategyNumber;
    cache->cc_skey[i].sk_subtype = InvalidOid;
                                                                        
    cache->cc_skey[i].sk_collation = C_COLLATION_OID;

    CACHE_elog(DEBUG2, "CatalogCacheInitializeCache %s %d %p", cache->cc_relname, i, cache);
  }

     
                                       
     
  cache->cc_tupdesc = tupdesc;
}

   
                                                                            
   
                                                                      
                                                                             
                                                                        
                                                                           
                      
   
void
InitCatCachePhase2(CatCache *cache, bool touch_index)
{
  if (cache->cc_tupdesc == NULL)
  {
    CatalogCacheInitializeCache(cache);
  }

  if (touch_index && cache->id != AMOID && cache->id != AMNAME)
  {
    Relation idesc;

       
                                                                       
                                                                         
                                                                       
                                                            
       
    LockRelationOid(cache->cc_reloid, AccessShareLock);
    idesc = index_open(cache->cc_indexoid, AccessShareLock);

       
                                                                         
                                                                          
                                                                        
                                                   
       
    Assert(idesc->rd_index->indisunique && idesc->rd_index->indimmediate);

    index_close(idesc, AccessShareLock);
    UnlockRelationOid(cache->cc_reloid, AccessShareLock);
  }
}

   
                
   
                                                            
                                                                
                                                            
                                                                  
                                                               
                                                                
                                                             
   
                                                                    
                                                                       
                                                                    
   
static bool
IndexScanOK(CatCache *cache, ScanKey cur_skey)
{
  switch (cache->id)
  {
  case INDEXRELID:

       
                                                                    
                                                                       
                                                                     
                                     
       
    if (!criticalRelcachesBuilt)
    {
      return false;
    }
    break;

  case AMOID:
  case AMNAME:

       
                                                                    
                                                                      
                                                                     
                               
       
    return false;

  case AUTHNAME:
  case AUTHOID:
  case AUTHMEMMEMROLE:

       
                                                                    
                                             
       
    if (!criticalSharedRelcachesBuilt)
    {
      return false;
    }
    break;

  default:
    break;
  }

                                     
  return true;
}

   
                          
   
                                                                        
                                                              
   
                                                                    
                                                                    
                                         
   
                                                                           
                                                                        
                                                                          
                                                                     
                     
   
HeapTuple
SearchCatCache(CatCache *cache, Datum v1, Datum v2, Datum v3, Datum v4)
{
  return SearchCatCacheInternal(cache, cache->cc_nkeys, v1, v2, v3, v4);
}

   
                                                                            
                                                                               
                                     
   

HeapTuple
SearchCatCache1(CatCache *cache, Datum v1)
{
  return SearchCatCacheInternal(cache, 1, v1, 0, 0, 0);
}

HeapTuple
SearchCatCache2(CatCache *cache, Datum v1, Datum v2)
{
  return SearchCatCacheInternal(cache, 2, v1, v2, 0, 0);
}

HeapTuple
SearchCatCache3(CatCache *cache, Datum v1, Datum v2, Datum v3)
{
  return SearchCatCacheInternal(cache, 3, v1, v2, v3, 0);
}

HeapTuple
SearchCatCache4(CatCache *cache, Datum v1, Datum v2, Datum v3, Datum v4)
{
  return SearchCatCacheInternal(cache, 4, v1, v2, v3, v4);
}

   
                                                  
   
static inline HeapTuple
SearchCatCacheInternal(CatCache *cache, int nkeys, Datum v1, Datum v2, Datum v3, Datum v4)
{
  Datum arguments[CATCACHE_MAXKEYS];
  uint32 hashValue;
  Index hashIndex;
  dlist_iter iter;
  dlist_head *bucket;
  CatCTup *ct;

                                                                          
  Assert(IsTransactionState());

  Assert(cache->cc_nkeys == nkeys);

     
                                              
     
  if (unlikely(cache->cc_tupdesc == NULL))
  {
    CatalogCacheInitializeCache(cache);
  }

#ifdef CATCACHE_STATS
  cache->cc_searches++;
#endif

                                        
  arguments[0] = v1;
  arguments[1] = v2;
  arguments[2] = v3;
  arguments[3] = v4;

     
                                                         
     
  hashValue = CatalogCacheComputeHashValue(cache, nkeys, v1, v2, v3, v4);
  hashIndex = HASH_INDEX(hashValue, cache->cc_nbuckets);

     
                                                                      
     
                                                                          
                                                                           
     
  bucket = &cache->cc_bucket[hashIndex];
  dlist_foreach(iter, bucket)
  {
    ct = dlist_container(CatCTup, cache_elem, iter.cur);

    if (ct->dead)
    {
      continue;                          
    }

    if (ct->hash_value != hashValue)
    {
      continue;                                           
    }

    if (!CatalogCacheCompareTuple(cache, nkeys, ct->keys, arguments))
    {
      continue;
    }

       
                                                                        
                                                                        
                                                                           
                                                 
       
    dlist_move_head(bucket, &ct->cache_elem);

       
                                                                          
                                                      
       
    if (!ct->negative)
    {
      ResourceOwnerEnlargeCatCacheRefs(CurrentResourceOwner);
      ct->refcount++;
      ResourceOwnerRememberCatCacheRef(CurrentResourceOwner, &ct->tuple);

      CACHE_elog(DEBUG2, "SearchCatCache(%s): found in bucket %d", cache->cc_relname, hashIndex);

#ifdef CATCACHE_STATS
      cache->cc_hits++;
#endif

      return &ct->tuple;
    }
    else
    {
      CACHE_elog(DEBUG2, "SearchCatCache(%s): found neg entry in bucket %d", cache->cc_relname, hashIndex);

#ifdef CATCACHE_STATS
      cache->cc_neg_hits++;
#endif

      return NULL;
    }
  }

  return SearchCatCacheMiss(cache, nkeys, hashValue, hashIndex, v1, v2, v3, v4);
}

   
                                                      
   
                                                                             
                                                                         
                                                
   
static pg_noinline HeapTuple
SearchCatCacheMiss(CatCache *cache, int nkeys, uint32 hashValue, Index hashIndex, Datum v1, Datum v2, Datum v3, Datum v4)
{
  ScanKeyData cur_skey[CATCACHE_MAXKEYS];
  Relation relation;
  SysScanDesc scandesc;
  HeapTuple ntp;
  CatCTup *ct;
  Datum arguments[CATCACHE_MAXKEYS];

                                        
  arguments[0] = v1;
  arguments[1] = v2;
  arguments[2] = v3;
  arguments[3] = v4;

     
                                                                          
                              
     
  memcpy(cur_skey, cache->cc_skey, sizeof(ScanKeyData) * nkeys);
  cur_skey[0].sk_argument = v1;
  cur_skey[1].sk_argument = v2;
  cur_skey[2].sk_argument = v3;
  cur_skey[3].sk_argument = v4;

     
                                                                             
                                                                       
                                                        
     
                                                                             
                                                                            
                                                                             
                                                                             
                                                                            
                                                                         
                                                                             
                                                                            
             
     
  relation = table_open(cache->cc_reloid, AccessShareLock);

  scandesc = systable_beginscan(relation, cache->cc_indexoid, IndexScanOK(cache, cur_skey), NULL, nkeys, cur_skey);

  ct = NULL;

  while (HeapTupleIsValid(ntp = systable_getnext(scandesc)))
  {
    ct = CatalogCacheCreateEntry(cache, ntp, arguments, hashValue, hashIndex, false);
                                           
    ResourceOwnerEnlargeCatCacheRefs(CurrentResourceOwner);
    ct->refcount++;
    ResourceOwnerRememberCatCacheRef(CurrentResourceOwner, &ct->tuple);
    break;                            
  }

  systable_endscan(scandesc);

  table_close(relation, AccessShareLock);

     
                                                                     
                                                                           
                                
     
                                                                           
                                                                          
                                                                            
                            
     
  if (ct == NULL)
  {
    if (IsBootstrapProcessingMode())
    {
      return NULL;
    }

    ct = CatalogCacheCreateEntry(cache, NULL, arguments, hashValue, hashIndex, true);

    CACHE_elog(DEBUG2, "SearchCatCache(%s): Contains %d/%d tuples", cache->cc_relname, cache->cc_ntup, CacheHdr->ch_ntup);
    CACHE_elog(DEBUG2, "SearchCatCache(%s): put neg entry in bucket %d", cache->cc_relname, hashIndex);

       
                                                                           
                      
       

    return NULL;
  }

  CACHE_elog(DEBUG2, "SearchCatCache(%s): Contains %d/%d tuples", cache->cc_relname, cache->cc_ntup, CacheHdr->ch_ntup);
  CACHE_elog(DEBUG2, "SearchCatCache(%s): put in bucket %d", cache->cc_relname, hashIndex);

#ifdef CATCACHE_STATS
  cache->cc_newloads++;
#endif

  return &ct->tuple;
}

   
                   
   
                                                                    
                                                 
   
                                                                         
                                                                         
                                                                        
                                                             
   
void
ReleaseCatCache(HeapTuple tuple)
{
  CatCTup *ct = (CatCTup *)(((char *)tuple) - offsetof(CatCTup, tuple));

                                                            
  Assert(ct->ct_magic == CT_MAGIC);
  Assert(ct->refcount > 0);

  ct->refcount--;
  ResourceOwnerForgetCatCacheRef(CurrentResourceOwner, &ct->tuple);

  if (
#ifndef CATCACHE_FORCE_RELEASE
      ct->dead &&
#endif
      ct->refcount == 0 && (ct->c_list == NULL || ct->c_list->refcount == 0))
    CatCacheRemoveCTup(ct->my_cache, ct);
}

   
                        
   
                                                           
   
                                                                             
                                                                             
                                                                  
   
uint32
GetCatCacheHashValue(CatCache *cache, Datum v1, Datum v2, Datum v3, Datum v4)
{
     
                                              
     
  if (cache->cc_tupdesc == NULL)
  {
    CatalogCacheInitializeCache(cache);
  }

     
                              
     
  return CatalogCacheComputeHashValue(cache, cache->cc_nkeys, v1, v2, v3, v4);
}

   
                      
   
                                                                   
                                                                     
   
                                                                        
                                                                        
                                                                          
                                                        
   
                                                                         
                                                                 
   
CatCList *
SearchCatCacheList(CatCache *cache, int nkeys, Datum v1, Datum v2, Datum v3)
{
  Datum v4 = 0;                              
  Datum arguments[CATCACHE_MAXKEYS];
  uint32 lHashValue;
  dlist_iter iter;
  CatCList *cl;
  CatCTup *ct;
  List *volatile ctlist;
  ListCell *ctlist_item;
  int nmembers;
  bool ordered;
  HeapTuple ntp;
  MemoryContext oldcxt;
  int i;

     
                                              
     
  if (cache->cc_tupdesc == NULL)
  {
    CatalogCacheInitializeCache(cache);
  }

  Assert(nkeys > 0 && nkeys < cache->cc_nkeys);

#ifdef CATCACHE_STATS
  cache->cc_lsearches++;
#endif

                                        
  arguments[0] = v1;
  arguments[1] = v2;
  arguments[2] = v3;
  arguments[3] = v4;

     
                                                                         
                                                                           
                                                          
     
  lHashValue = CatalogCacheComputeHashValue(cache, nkeys, v1, v2, v3, v4);

     
                                                              
     
                                                                          
                                                                           
     
  dlist_foreach(iter, &cache->cc_lists)
  {
    cl = dlist_container(CatCList, cache_elem, iter.cur);

    if (cl->dead)
    {
      continue;                          
    }

    if (cl->hash_value != lHashValue)
    {
      continue;                                           
    }

       
                                               
       
    if (cl->nkeys != nkeys)
    {
      continue;
    }

    if (!CatalogCacheCompareTuple(cache, nkeys, cl->keys, arguments))
    {
      continue;
    }

       
                                                                    
                                                                        
                                                                          
                                                                   
                      
       
    dlist_move_head(&cache->cc_lists, &cl->cache_elem);

                                                
    ResourceOwnerEnlargeCatCacheListRefs(CurrentResourceOwner);
    cl->refcount++;
    ResourceOwnerRememberCatCacheListRef(CurrentResourceOwner, cl);

    CACHE_elog(DEBUG2, "SearchCatCacheList(%s): found list", cache->cc_relname);

#ifdef CATCACHE_STATS
    cache->cc_lhits++;
#endif

    return cl;
  }

     
                                                                        
                                                                      
                                                             
     
                                                                           
                                                                             
                                                                           
                                          
     
  ResourceOwnerEnlargeCatCacheListRefs(CurrentResourceOwner);

  ctlist = NIL;

  PG_TRY();
  {
    ScanKeyData cur_skey[CATCACHE_MAXKEYS];
    Relation relation;
    SysScanDesc scandesc;

       
                                                                       
                                     
       
    memcpy(cur_skey, cache->cc_skey, sizeof(ScanKeyData) * cache->cc_nkeys);
    cur_skey[0].sk_argument = v1;
    cur_skey[1].sk_argument = v2;
    cur_skey[2].sk_argument = v3;
    cur_skey[3].sk_argument = v4;

    relation = table_open(cache->cc_reloid, AccessShareLock);

    scandesc = systable_beginscan(relation, cache->cc_indexoid, IndexScanOK(cache, cur_skey), NULL, nkeys, cur_skey);

                                                                 
    ordered = (scandesc->irel != NULL);

    while (HeapTupleIsValid(ntp = systable_getnext(scandesc)))
    {
      uint32 hashValue;
      Index hashIndex;
      bool found = false;
      dlist_head *bucket;

         
                                                         
         
      ct = NULL;
      hashValue = CatalogCacheComputeTupleHashValue(cache, cache->cc_nkeys, ntp);
      hashIndex = HASH_INDEX(hashValue, cache->cc_nbuckets);

      bucket = &cache->cc_bucket[hashIndex];
      dlist_foreach(iter, bucket)
      {
        ct = dlist_container(CatCTup, cache_elem, iter.cur);

        if (ct->dead || ct->negative)
        {
          continue;                                       
        }

        if (ct->hash_value != hashValue)
        {
          continue;                                           
        }

        if (!ItemPointerEquals(&(ct->tuple.t_self), &(ntp->t_self)))
        {
          continue;                     
        }

           
                                                                    
                        
           
        if (ct->c_list)
        {
          continue;
        }

        found = true;
        break;           
      }

      if (!found)
      {
                                                              
        ct = CatalogCacheCreateEntry(cache, ntp, arguments, hashValue, hashIndex, false);
      }

                                                                     
                                                                       
      ctlist = lappend(ctlist, ct);
      ct->refcount++;
    }

    systable_endscan(scandesc);

    table_close(relation, AccessShareLock);

                                              
    oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
    nmembers = list_length(ctlist);
    cl = (CatCList *)palloc(offsetof(CatCList, members) + nmembers * sizeof(CatCTup *));

                            
    CatCacheCopyKeys(cache->cc_tupdesc, nkeys, cache->cc_keyno, arguments, cl->keys);
    MemoryContextSwitchTo(oldcxt);

       
                                                                           
                                                                     
                                                                         
                                                                          
                    
       
  }
  PG_CATCH();
  {
    foreach (ctlist_item, ctlist)
    {
      ct = (CatCTup *)lfirst(ctlist_item);
      Assert(ct->c_list == NULL);
      Assert(ct->refcount > 0);
      ct->refcount--;
      if (
#ifndef CATCACHE_FORCE_RELEASE
          ct->dead &&
#endif
          ct->refcount == 0 && (ct->c_list == NULL || ct->c_list->refcount == 0))
        CatCacheRemoveCTup(cache, ct);
    }

    PG_RE_THROW();
  }
  PG_END_TRY();

  cl->cl_magic = CL_MAGIC;
  cl->my_cache = cache;
  cl->refcount = 0;                     
  cl->dead = false;
  cl->ordered = ordered;
  cl->nkeys = nkeys;
  cl->hash_value = lHashValue;
  cl->n_members = nmembers;

  i = 0;
  foreach (ctlist_item, ctlist)
  {
    cl->members[i++] = ct = (CatCTup *)lfirst(ctlist_item);
    Assert(ct->c_list == NULL);
    ct->c_list = cl;
                                                      
    Assert(ct->refcount > 0);
    ct->refcount--;
                                                    
    if (ct->dead)
    {
      cl->dead = true;
    }
  }
  Assert(i == nmembers);

  dlist_push_head(&cache->cc_lists, &cl->cache_elem);

                                                       
  cl->refcount++;
  ResourceOwnerRememberCatCacheListRef(CurrentResourceOwner, cl);

  CACHE_elog(DEBUG2, "SearchCatCacheList(%s): made list of %d members", cache->cc_relname, nmembers);

  return cl;
}

   
                       
   
                                                     
   
void
ReleaseCatCacheList(CatCList *list)
{
                                                            
  Assert(list->cl_magic == CL_MAGIC);
  Assert(list->refcount > 0);
  list->refcount--;
  ResourceOwnerForgetCatCacheListRef(CurrentResourceOwner, list);

  if (
#ifndef CATCACHE_FORCE_RELEASE
      list->dead &&
#endif
      list->refcount == 0)
    CatCacheRemoveCList(list->my_cache, list);
}

   
                           
                                                                      
                                                                    
   
static CatCTup *
CatalogCacheCreateEntry(CatCache *cache, HeapTuple ntp, Datum *arguments, uint32 hashValue, Index hashIndex, bool negative)
{
  CatCTup *ct;
  HeapTuple dtp;
  MemoryContext oldcxt;

                                                 
  if (ntp)
  {
    int i;

    Assert(!negative);

       
                                                                        
                                                                         
                                                                        
                                                                      
                                                        
       
    if (HeapTupleHasExternal(ntp))
    {
      dtp = toast_flatten_tuple(ntp, cache->cc_tupdesc);
    }
    else
    {
      dtp = ntp;
    }

                                                                    
    oldcxt = MemoryContextSwitchTo(CacheMemoryContext);

    ct = (CatCTup *)palloc(sizeof(CatCTup) + MAXIMUM_ALIGNOF + dtp->t_len);
    ct->tuple.t_len = dtp->t_len;
    ct->tuple.t_self = dtp->t_self;
    ct->tuple.t_tableOid = dtp->t_tableOid;
    ct->tuple.t_data = (HeapTupleHeader)MAXALIGN(((char *)ct) + sizeof(CatCTup));
                             
    memcpy((char *)ct->tuple.t_data, (const char *)dtp->t_data, dtp->t_len);
    MemoryContextSwitchTo(oldcxt);

    if (dtp != ntp)
    {
      heap_freetuple(dtp);
    }

                                                                     
    for (i = 0; i < cache->cc_nkeys; i++)
    {
      Datum atp;
      bool isnull;

      atp = heap_getattr(&ct->tuple, cache->cc_keyno[i], cache->cc_tupdesc, &isnull);
      Assert(!isnull);
      ct->keys[i] = atp;
    }
  }
  else
  {
    Assert(negative);
    oldcxt = MemoryContextSwitchTo(CacheMemoryContext);
    ct = (CatCTup *)palloc(sizeof(CatCTup));

       
                                                                          
                 
       
    CatCacheCopyKeys(cache->cc_tupdesc, cache->cc_nkeys, cache->cc_keyno, arguments, ct->keys);
    MemoryContextSwitchTo(oldcxt);
  }

     
                                                                       
                             
     
  ct->ct_magic = CT_MAGIC;
  ct->my_cache = cache;
  ct->c_list = NULL;
  ct->refcount = 0;                     
  ct->dead = false;
  ct->negative = negative;
  ct->hash_value = hashValue;

  dlist_push_head(&cache->cc_bucket[hashIndex], &ct->cache_elem);

  cache->cc_ntup++;
  CacheHdr->ch_ntup++;

     
                                                                             
                                                   
     
  if (cache->cc_ntup > cache->cc_nbuckets * 2)
  {
    RehashCatCache(cache);
  }

  return ct;
}

   
                                                            
   
static void
CatCacheFreeKeys(TupleDesc tupdesc, int nkeys, int *attnos, Datum *keys)
{
  int i;

  for (i = 0; i < nkeys; i++)
  {
    int attnum = attnos[i];
    Form_pg_attribute att;

                                                      
    Assert(attnum > 0);

    att = TupleDescAttr(tupdesc, attnum - 1);

    if (!att->attbyval)
    {
      pfree(DatumGetPointer(keys[i]));
    }
  }
}

   
                                                                             
                                                                               
            
   
static void
CatCacheCopyKeys(TupleDesc tupdesc, int nkeys, int *attnos, Datum *srckeys, Datum *dstkeys)
{
  int i;

     
                                                                      
                                         
     

  for (i = 0; i < nkeys; i++)
  {
    int attnum = attnos[i];
    Form_pg_attribute att = TupleDescAttr(tupdesc, attnum - 1);
    Datum src = srckeys[i];
    NameData srcname;

       
                                                                         
                                                                         
                                                                         
                  
       
    if (att->atttypid == NAMEOID)
    {
      namestrcpy(&srcname, DatumGetCString(src));
      src = NameGetDatum(&srcname);
    }

    dstkeys[i] = datumCopy(src, att->attbyval, att->attlen);
  }
}

   
                                   
   
                                                                      
   
                                                                      
                                                                             
                                                                            
                                                                             
   
                                                                          
                                                                        
                                                                          
                                                                     
                                                                      
                                                            
                                   
   
                                                                            
                                                                          
                                                                          
                                                                 
   
                                                                         
                                                                          
                                                                             
                                                                         
                                                   
   
                                                                      
                                                                     
                                                                     
                    
   
void
PrepareToInvalidateCacheTuple(Relation relation, HeapTuple tuple, HeapTuple newtuple, void (*function)(int, uint32, Oid))
{
  slist_iter iter;
  Oid reloid;

  CACHE_elog(DEBUG2, "PrepareToInvalidateCacheTuple: called");

     
                   
     
  Assert(RelationIsValid(relation));
  Assert(HeapTupleIsValid(tuple));
  Assert(PointerIsValid(function));
  Assert(CacheHdr != NULL);

  reloid = RelationGetRelid(relation);

                      
                    
                                                                 
                                                          
                                                                   
                      
     

  slist_foreach(iter, &CacheHdr->ch_caches)
  {
    CatCache *ccp = slist_container(CatCache, cc_next, iter.cur);
    uint32 hashvalue;
    Oid dbid;

    if (ccp->cc_reloid != reloid)
    {
      continue;
    }

                                                                  
    if (ccp->cc_tupdesc == NULL)
    {
      CatalogCacheInitializeCache(ccp);
    }

    hashvalue = CatalogCacheComputeTupleHashValue(ccp, ccp->cc_nkeys, tuple);
    dbid = ccp->cc_relisshared ? (Oid)0 : MyDatabaseId;

    (*function)(ccp->id, hashvalue, dbid);

    if (newtuple)
    {
      uint32 newhashvalue;

      newhashvalue = CatalogCacheComputeTupleHashValue(ccp, ccp->cc_nkeys, newtuple);

      if (newhashvalue != hashvalue)
      {
        (*function)(ccp->id, newhashvalue, dbid);
      }
    }
  }
}

   
                                                                         
                                  
   
void
PrintCatCacheLeakWarning(HeapTuple tuple)
{
  CatCTup *ct = (CatCTup *)(((char *)tuple) - offsetof(CatCTup, tuple));

                                                           
  Assert(ct->ct_magic == CT_MAGIC);

  elog(WARNING, "cache reference leak: cache %s (%d), tuple %u/%u has count %d", ct->my_cache->cc_relname, ct->my_cache->id, ItemPointerGetBlockNumber(&(tuple->t_self)), ItemPointerGetOffsetNumber(&(tuple->t_self)), ct->refcount);
}

void
PrintCatCacheListLeakWarning(CatCList *list)
{
  elog(WARNING, "cache reference leak: cache %s (%d), list %p has count %d", list->my_cache->cc_relname, list->my_cache->id, list, list->refcount);
}
