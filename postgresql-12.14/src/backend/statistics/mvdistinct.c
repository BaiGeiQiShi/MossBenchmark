                                                                            
   
                
                                                  
   
                                                                               
                                                             
 
                                                                             
                                                                             
                                                                             
                                                                              
                                                    
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"

static double
ndistinct_for_combination(double totalrows, int numrows, HeapTuple *rows, VacAttrStats **stats, int k, int *combination);
static double
estimate_ndistinct(double totalrows, int numrows, int d, int f1);
static int
n_choose_k(int n, int k);
static int
num_combinations(int n);

                                                            
#define SizeOfHeader (3 * sizeof(uint32))

                                                                    
#define SizeOfItem(natts) (sizeof(double) + sizeof(int) + (natts) * sizeof(AttrNumber))

                                                            
#define MinSizeOfItem SizeOfItem(2)

                                                             
#define MinSizeOfItems(nitems) (SizeOfHeader + (nitems) * MinSizeOfItem)

                               

                                                                  
typedef struct CombinationGenerator
{
  int k;                                          
  int n;                                           
  int current;                                                    
  int ncombinations;                                             
  int *combinations;                                      
} CombinationGenerator;

static CombinationGenerator *
generator_init(int n, int k);
static void
generator_free(CombinationGenerator *state);
static int *
generator_next(CombinationGenerator *state);
static void
generate_combinations(CombinationGenerator *state);

   
                           
                                                                     
   
                                                                      
                                                   
   
MVNDistinct *
statext_ndistinct_build(double totalrows, int numrows, HeapTuple *rows, Bitmapset *attrs, VacAttrStats **stats)
{
  MVNDistinct *result;
  int k;
  int itemcnt;
  int numattrs = bms_num_members(attrs);
  int numcombs = num_combinations(numattrs);

  result = palloc(offsetof(MVNDistinct, items) + numcombs * sizeof(MVNDistinctItem));
  result->magic = STATS_NDISTINCT_MAGIC;
  result->type = STATS_NDISTINCT_TYPE_BASIC;
  result->nitems = numcombs;

  itemcnt = 0;
  for (k = 2; k <= numattrs; k++)
  {
    int *combination;
    CombinationGenerator *generator;

                                                      
    generator = generator_init(numattrs, k);

    while ((combination = generator_next(generator)))
    {
      MVNDistinctItem *item = &result->items[itemcnt];
      int j;

      item->attrs = NULL;
      for (j = 0; j < k; j++)
      {
        item->attrs = bms_add_member(item->attrs, stats[combination[j]]->attr->attnum);
      }
      item->ndistinct = ndistinct_for_combination(totalrows, numrows, rows, stats, k, combination);

      itemcnt++;
      Assert(itemcnt <= result->nitems);
    }

    generator_free(generator);
  }

                                                   
  Assert(itemcnt == result->nitems);

  return result;
}

   
                          
                                                                      
   
MVNDistinct *
statext_ndistinct_load(Oid mvoid)
{
  MVNDistinct *result;
  bool isnull;
  Datum ndist;
  HeapTuple htup;

  htup = SearchSysCache1(STATEXTDATASTXOID, ObjectIdGetDatum(mvoid));
  if (!HeapTupleIsValid(htup))
  {
    elog(ERROR, "cache lookup failed for statistics object %u", mvoid);
  }

  ndist = SysCacheGetAttr(STATEXTDATASTXOID, htup, Anum_pg_statistic_ext_data_stxdndistinct, &isnull);
  if (isnull)
  {
    elog(ERROR, "requested statistics kind \"%c\" is not yet built for statistics object %u", STATS_EXT_NDISTINCT, mvoid);
  }

  result = statext_ndistinct_deserialize(DatumGetByteaPP(ndist));

  ReleaseSysCache(htup);

  return result;
}

   
                               
                                                    
   
bytea *
statext_ndistinct_serialize(MVNDistinct *ndistinct)
{
  int i;
  bytea *output;
  char *tmp;
  Size len;

  Assert(ndistinct->magic == STATS_NDISTINCT_MAGIC);
  Assert(ndistinct->type == STATS_NDISTINCT_TYPE_BASIC);

     
                                                                            
                                                        
     
  len = VARHDRSZ + SizeOfHeader;

                                                               
  for (i = 0; i < ndistinct->nitems; i++)
  {
    int nmembers;

    nmembers = bms_num_members(ndistinct->items[i].attrs);
    Assert(nmembers >= 2);

    len += SizeOfItem(nmembers);
  }

  output = (bytea *)palloc(len);
  SET_VARSIZE(output, len);

  tmp = VARDATA(output);

                                                          
  memcpy(tmp, &ndistinct->magic, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(tmp, &ndistinct->type, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(tmp, &ndistinct->nitems, sizeof(uint32));
  tmp += sizeof(uint32);

     
                                                                     
     
  for (i = 0; i < ndistinct->nitems; i++)
  {
    MVNDistinctItem item = ndistinct->items[i];
    int nmembers = bms_num_members(item.attrs);
    int x;

    memcpy(tmp, &item.ndistinct, sizeof(double));
    tmp += sizeof(double);
    memcpy(tmp, &nmembers, sizeof(int));
    tmp += sizeof(int);

    x = -1;
    while ((x = bms_next_member(item.attrs, x)) >= 0)
    {
      AttrNumber value = (AttrNumber)x;

      memcpy(tmp, &value, sizeof(AttrNumber));
      tmp += sizeof(AttrNumber);
    }

                                   
    Assert(tmp <= ((char *)output + len));
  }

                                                
  Assert(tmp == ((char *)output + len));

  return output;
}

   
                                 
                                                                 
   
MVNDistinct *
statext_ndistinct_deserialize(bytea *data)
{
  int i;
  Size minimum_size;
  MVNDistinct ndist;
  MVNDistinct *ndistinct;
  char *tmp;

  if (data == NULL)
  {
    return NULL;
  }

                                                                 
  if (VARSIZE_ANY_EXHDR(data) < SizeOfHeader)
  {
    elog(ERROR, "invalid MVNDistinct size %zd (expected at least %zd)", VARSIZE_ANY_EXHDR(data), SizeOfHeader);
  }

                                                                     
  tmp = VARDATA_ANY(data);

                                                              
  memcpy(&ndist.magic, tmp, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(&ndist.type, tmp, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(&ndist.nitems, tmp, sizeof(uint32));
  tmp += sizeof(uint32);

  if (ndist.magic != STATS_NDISTINCT_MAGIC)
  {
    elog(ERROR, "invalid ndistinct magic %08x (expected %08x)", ndist.magic, STATS_NDISTINCT_MAGIC);
  }
  if (ndist.type != STATS_NDISTINCT_TYPE_BASIC)
  {
    elog(ERROR, "invalid ndistinct type %d (expected %d)", ndist.type, STATS_NDISTINCT_TYPE_BASIC);
  }
  if (ndist.nitems == 0)
  {
    elog(ERROR, "invalid zero-length item array in MVNDistinct");
  }

                                                                 
  minimum_size = MinSizeOfItems(ndist.nitems);
  if (VARSIZE_ANY_EXHDR(data) < minimum_size)
  {
    elog(ERROR, "invalid MVNDistinct size %zd (expected at least %zd)", VARSIZE_ANY_EXHDR(data), minimum_size);
  }

     
                                                                      
                                                            
     
  ndistinct = palloc0(MAXALIGN(offsetof(MVNDistinct, items)) + (ndist.nitems * sizeof(MVNDistinctItem)));
  ndistinct->magic = ndist.magic;
  ndistinct->type = ndist.type;
  ndistinct->nitems = ndist.nitems;

  for (i = 0; i < ndistinct->nitems; i++)
  {
    MVNDistinctItem *item = &ndistinct->items[i];
    int nelems;

    item->attrs = NULL;

                         
    memcpy(&item->ndistinct, tmp, sizeof(double));
    tmp += sizeof(double);

                              
    memcpy(&nelems, tmp, sizeof(int));
    tmp += sizeof(int);
    Assert((nelems >= 2) && (nelems <= STATS_MAX_DIMENSIONS));

    while (nelems-- > 0)
    {
      AttrNumber attno;

      memcpy(&attno, tmp, sizeof(AttrNumber));
      tmp += sizeof(AttrNumber);
      item->attrs = bms_add_member(item->attrs, attno);
    }

                                
    Assert(tmp <= ((char *)data + VARSIZE_ANY(data)));
  }

                                                       
  Assert(tmp == ((char *)data + VARSIZE_ANY(data)));

  return ndistinct;
}

   
                   
                                        
   
                                                                   
                                                                       
   
Datum
pg_ndistinct_in(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_ndistinct")));

  PG_RETURN_VOID();                          
}

   
                
                                         
   
                                                          
   
Datum
pg_ndistinct_out(PG_FUNCTION_ARGS)
{
  bytea *data = PG_GETARG_BYTEA_PP(0);
  MVNDistinct *ndist = statext_ndistinct_deserialize(data);
  int i;
  StringInfoData str;

  initStringInfo(&str);
  appendStringInfoChar(&str, '{');

  for (i = 0; i < ndist->nitems; i++)
  {
    MVNDistinctItem item = ndist->items[i];
    int x = -1;
    bool first = true;

    if (i > 0)
    {
      appendStringInfoString(&str, ", ");
    }

    while ((x = bms_next_member(item.attrs, x)) >= 0)
    {
      appendStringInfo(&str, "%s%d", first ? "\"" : ", ", x);
      first = false;
    }
    appendStringInfo(&str, "\": %d", (int)item.ndistinct);
  }

  appendStringInfoChar(&str, '}');

  PG_RETURN_CSTRING(str.data);
}

   
                     
                                               
   
Datum
pg_ndistinct_recv(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_ndistinct")));

  PG_RETURN_VOID();                          
}

   
                     
                                                
   
                                                                    
   
Datum
pg_ndistinct_send(PG_FUNCTION_ARGS)
{
  return byteasend(fcinfo);
}

   
                             
                                                                     
   
                                                                       
                  
                            
   
                                                                        
                                    
   
static double
ndistinct_for_combination(double totalrows, int numrows, HeapTuple *rows, VacAttrStats **stats, int k, int *combination)
{
  int i, j;
  int f1, cnt, d;
  bool *isnull;
  Datum *values;
  SortItem *items;
  MultiSortSupport mss;

  mss = multi_sort_init(k);

     
                                                                            
                                                                        
                                                                            
                                                                     
     
  items = (SortItem *)palloc(numrows * sizeof(SortItem));
  values = (Datum *)palloc0(sizeof(Datum) * numrows * k);
  isnull = (bool *)palloc0(sizeof(bool) * numrows * k);

  for (i = 0; i < numrows; i++)
  {
    items[i].values = &values[i * k];
    items[i].isnull = &isnull[i * k];
  }

     
                                                                             
                  
     
                                                                          
                                                                           
     
  for (i = 0; i < k; i++)
  {
    VacAttrStats *colstat = stats[combination[i]];
    TypeCacheEntry *type;

    type = lookup_type_cache(colstat->attrtypid, TYPECACHE_LT_OPR);
    if (type->lt_opr == InvalidOid)                       
    {
      elog(ERROR, "cache lookup failed for ordering operator for type %u", colstat->attrtypid);
    }

                                                      
    multi_sort_add_dimension(mss, i, type->lt_opr, colstat->attrcollid);

                                                                    
    for (j = 0; j < numrows; j++)
    {
      items[j].values[i] = heap_getattr(rows[j], colstat->attr->attnum, colstat->tupDesc, &items[j].isnull[i]);
    }
  }

                                     
  qsort_arg((void *)items, numrows, sizeof(SortItem), multi_sort_compare, mss);

                                                         

  f1 = 0;
  cnt = 1;
  d = 1;
  for (i = 1; i < numrows; i++)
  {
    if (multi_sort_compare(&items[i], &items[i - 1], mss) != 0)
    {
      if (cnt == 1)
      {
        f1 += 1;
      }

      d++;
      cnt = 0;
    }

    cnt += 1;
  }

  if (cnt == 1)
  {
    f1 += 1;
  }

  return estimate_ndistinct(totalrows, numrows, d, f1);
}

                                                     
static double
estimate_ndistinct(double totalrows, int numrows, int d, int f1)
{
  double numer, denom, ndistinct;

  numer = (double)numrows * (double)d;

  denom = (double)(numrows - f1) + (double)f1 * (double)numrows / totalrows;

  ndistinct = numer / denom;

                                                     
  if (ndistinct < (double)d)
  {
    ndistinct = (double)d;
  }

  if (ndistinct > totalrows)
  {
    ndistinct = totalrows;
  }

  return floor(ndistinct + 0.5);
}

   
              
                                                                   
                                     
   
static int
n_choose_k(int n, int k)
{
  int d, r;

  Assert((k > 0) && (n >= k));

                                                 
  k = Min(k, n - k);

  r = 1;
  for (d = 1; d <= k; ++d)
  {
    r *= n--;
    r /= d;
  }

  return r;
}

   
                    
                                                                
   
static int
num_combinations(int n)
{
  int k;
  int ncombs = 1;

  for (k = 1; k <= n; k++)
  {
    ncombs *= 2;
  }

  ncombs -= (n + 1);

  return ncombs;
}

   
                  
                                             
   
                                                                             
                                                                          
                               
   
static CombinationGenerator *
generator_init(int n, int k)
{
  CombinationGenerator *state;

  Assert((n >= k) && (k > 0));

                                                                
  state = (CombinationGenerator *)palloc(sizeof(CombinationGenerator));

  state->ncombinations = n_choose_k(n, k);

                                               
  state->combinations = (int *)palloc(sizeof(int) * k * state->ncombinations);

  state->current = 0;
  state->k = k;
  state->n = n;

                                                                    
  generate_combinations(state);

                                                            
  Assert(state->current == state->ncombinations);

                                                        
  state->current = 0;

  return state;
}

   
                  
                                                        
   
                                                                      
                                                                
   
static int *
generator_next(CombinationGenerator *state)
{
  if (state->current == state->ncombinations)
  {
    return NULL;
  }

  return &state->combinations[state->k * state->current++];
}

   
                  
                                             
   
                                                                   
   
static void
generator_free(CombinationGenerator *state)
{
  pfree(state->combinations);
  pfree(state);
}

   
                                 
                                                       
   
                                                                              
                                                                              
                                                          
   
static void
generate_combinations_recurse(CombinationGenerator *state, int index, int start, int *current)
{
                                                              
  if (index < state->k)
  {
    int i;

       
                                                                       
                                           
       

    for (i = start; i < state->n; i++)
    {
      current[index] = i;
      generate_combinations_recurse(state, (index + 1), (i + 1), current);
    }

    return;
  }
  else
  {
                                                         
    memcpy(&state->combinations[(state->k * state->current)], current, state->k * sizeof(int));
    state->current++;
  }
}

   
                         
                                              
   
static void
generate_combinations(CombinationGenerator *state)
{
  int *current = (int *)palloc0(sizeof(int) * state->k);

  generate_combinations_recurse(state, 0, 0, current);

  pfree(current);
}
