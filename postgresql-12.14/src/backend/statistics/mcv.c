                                                                            
   
         
                                     
   
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "fmgr.h"
#include "funcapi.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/fmgroids.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

   
                                                                      
                                                                          
                                                                        
                                                                      
   
                                                
   
                                                 
                                          
                                    
                                        
   
                                                     
                                                       
   
                                                                
   
#define ITEM_SIZE(ndims) ((ndims) * (sizeof(uint16) + sizeof(bool)) + 2 * sizeof(double))

   
                                                               
   
#define MinSizeOfMCVList (VARHDRSZ + sizeof(uint32) * 3 + sizeof(AttrNumber))

   
                                                                   
                                                                    
                                                                     
                            
   
#define SizeOfMCVList(ndims, nitems) ((MinSizeOfMCVList + sizeof(Oid) * (ndims)) + ((ndims) * sizeof(DimensionInfo)) + ((nitems) * ITEM_SIZE(ndims)))

static MultiSortSupport
build_mss(VacAttrStats **stats, int numattrs);

static SortItem *
build_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss, int *ndistinct);

static SortItem **
build_column_frequencies(SortItem *groups, int ngroups, MultiSortSupport mss, int *ncounts);

static int
count_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss);

   
                                                                        
                                
   
#define RESULT_MERGE(value, is_or, match) ((is_or) ? ((value) || (match)) : ((value) && (match)))

   
                                                                             
                                                                              
                                                                               
                                                                              
   
                                                                            
                                    
   
#define RESULT_IS_FINAL(value, is_or) ((is_or) ? (value) : (!(value)))

   
                             
                                                                      
                                                       
   
                                                                     
                                                                           
                                                                           
                                                                     
                                                                   
                        
   
                                                                        
                                                                      
                                                                       
                                                                           
                                                                      
                                                                           
                                                                        
                                                  
                                             
                                                                      
                                                                          
                                                                          
                                                                           
                                                                        
                                
                                       
                                                                          
                                                                         
                                                                       
                                                                        
                                                                        
                                                                    
                                                                          
   
                                                                           
                                                                     
                                                                           
                                                                        
                                                                        
                                                                        
                                                                         
                                                           
   
static double
get_mincount_for_mcv_list(int samplerows, double totalrows)
{
  double n = samplerows;
  double N = totalrows;
  double numer, denom;

  numer = n * (N - n);
  denom = N - n + 0.04 * n * (N - 1);

                                                              
  if (denom == 0.0)
  {
    return 0.0;
  }

  return numer / denom;
}

   
                                                 
   
                                  
   
                                                                   
   
                                                         
   
                                                                   
   
                                                             
   
   
MCVList *
statext_mcv_build(int numrows, HeapTuple *rows, Bitmapset *attrs, VacAttrStats **stats, double totalrows)
{
  int i, numattrs, ngroups, nitems;
  AttrNumber *attnums;
  double mincount;
  SortItem *items;
  SortItem *groups;
  MCVList *mcvlist = NULL;
  MultiSortSupport mss;

  attnums = build_attnums_array(attrs, &numattrs);

                                      
  mss = build_mss(stats, numattrs);

                     
  items = build_sorted_items(numrows, &nitems, rows, stats[0]->tupDesc, mss, numattrs, attnums);

  if (!items)
  {
    return NULL;
  }

                                                                   
  groups = build_distinct_groups(nitems, items, mss, &ngroups);

     
                                                                           
                                                                        
     
  nitems = stats[0]->attr->attstattarget;
  for (i = 1; i < numattrs; i++)
  {
    if (stats[i]->attr->attstattarget > nitems)
    {
      nitems = stats[i]->attr->attstattarget;
    }
  }
  if (nitems > ngroups)
  {
    nitems = ngroups;
  }

     
                                                                          
                                                                        
                                                                        
                                                                         
                                                                  
     
                                                                        
                                                                          
                                                                            
                                                                          
                                                                   
     
                                                                        
                                                                            
                               
     
  mincount = get_mincount_for_mcv_list(numrows, totalrows);

     
                                                                          
                                                                            
                    
     
  for (i = 0; i < nitems; i++)
  {
    if (groups[i].count < mincount)
    {
      nitems = i;
      break;
    }
  }

     
                                                                             
                                                                           
                                                                  
     
  if (nitems > 0)
  {
    int j;
    SortItem key;
    MultiSortSupport tmp;

                                                  
    SortItem **freqs;
    int *nfreqs;

                               
    tmp = (MultiSortSupport)palloc(offsetof(MultiSortSupportData, ssup) + sizeof(SortSupportData));

                                                       
    nfreqs = (int *)palloc0(sizeof(int) * numattrs);
    freqs = build_column_frequencies(groups, ngroups, mss, nfreqs);

       
                                                                   
       
    mcvlist = (MCVList *)palloc0(offsetof(MCVList, items) + sizeof(MCVItem) * nitems);

    mcvlist->magic = STATS_MCV_MAGIC;
    mcvlist->type = STATS_MCV_TYPE_BASIC;
    mcvlist->ndimensions = numattrs;
    mcvlist->nitems = nitems;

                                         
    for (i = 0; i < numattrs; i++)
    {
      mcvlist->types[i] = stats[i]->attrtypid;
    }

                                                         
    for (i = 0; i < nitems; i++)
    {
                                                        
      MCVItem *item = &mcvlist->items[i];

      item->values = (Datum *)palloc(sizeof(Datum) * numattrs);
      item->isnull = (bool *)palloc(sizeof(bool) * numattrs);

                                     
      memcpy(item->values, groups[i].values, sizeof(Datum) * numattrs);
      memcpy(item->isnull, groups[i].isnull, sizeof(bool) * numattrs);

                                                                    
      Assert((i == 0) || (groups[i - 1].count >= groups[i].count));

                           
      item->frequency = (double)groups[i].count / numrows;

                                                              
      item->base_frequency = 1.0;
      for (j = 0; j < numattrs; j++)
      {
        SortItem *freq;

                              
        tmp->ndims = 1;
        tmp->ssup[0] = mss->ssup[j];

                             
        key.values = &groups[i].values[j];
        key.isnull = &groups[i].isnull[j];

        freq = (SortItem *)bsearch_arg(&key, freqs[j], nfreqs[j], sizeof(SortItem), multi_sort_compare, tmp);

        item->base_frequency *= ((double)freq->count) / numrows;
      }
    }

    pfree(nfreqs);
    pfree(freqs);
  }

  pfree(items);
  pfree(groups);

  return mcvlist;
}

   
             
                                                             
   
static MultiSortSupport
build_mss(VacAttrStats **stats, int numattrs)
{
  int i;

                                                             
  MultiSortSupport mss = multi_sort_init(numattrs);

                                                         
  for (i = 0; i < numattrs; i++)
  {
    VacAttrStats *colstat = stats[i];
    TypeCacheEntry *type;

    type = lookup_type_cache(colstat->attrtypid, TYPECACHE_LT_OPR);
    if (type->lt_opr == InvalidOid)                       
    {
      elog(ERROR, "cache lookup failed for ordering operator for type %u", colstat->attrtypid);
    }

    multi_sort_add_dimension(mss, i, type->lt_opr, colstat->attrcollid);
  }

  return mss;
}

   
                         
                                                         
   
                                                                        
   
static int
count_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss)
{
  int i;
  int ndistinct;

  ndistinct = 1;
  for (i = 1; i < numrows; i++)
  {
                                              
    Assert(multi_sort_compare(&items[i], &items[i - 1], mss) >= 0);

    if (multi_sort_compare(&items[i], &items[i - 1], mss) != 0)
    {
      ndistinct += 1;
    }
  }

  return ndistinct;
}

   
                           
                                                                           
   
static int
compare_sort_item_count(const void *a, const void *b)
{
  SortItem *ia = (SortItem *)a;
  SortItem *ib = (SortItem *)b;

  if (ia->count == ib->count)
  {
    return 0;
  }
  else if (ia->count > ib->count)
  {
    return -1;
  }

  return 1;
}

   
                         
                                                                             
   
                                           
   
static SortItem *
build_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss, int *ndistinct)
{
  int i, j;
  int ngroups = count_distinct_groups(numrows, items, mss);

  SortItem *groups = (SortItem *)palloc(ngroups * sizeof(SortItem));

  j = 0;
  groups[0] = items[0];
  groups[0].count = 1;

  for (i = 1; i < numrows; i++)
  {
                                           
    Assert(multi_sort_compare(&items[i], &items[i - 1], mss) >= 0);

                                      
    if (multi_sort_compare(&items[i], &items[i - 1], mss) != 0)
    {
      groups[++j] = items[i];
      groups[j].count = 0;
    }

    groups[j].count++;
  }

                                                               
  Assert(j + 1 == ngroups);

                                                                    
  pg_qsort((void *)groups, ngroups, sizeof(SortItem), compare_sort_item_count);

  *ndistinct = ngroups;
  return groups;
}

                                           
static int
sort_item_compare(const void *a, const void *b, void *arg)
{
  SortSupport ssup = (SortSupport)arg;
  SortItem *ia = (SortItem *)a;
  SortItem *ib = (SortItem *)b;

  return ApplySortComparator(ia->values[0], ia->isnull[0], ib->values[0], ib->isnull[0], ssup);
}

   
                            
                                                
   
                                                                         
                                                                        
                                                       
   
                                                                         
                                                                       
                                                                       
                    
   
static SortItem **
build_column_frequencies(SortItem *groups, int ngroups, MultiSortSupport mss, int *ncounts)
{
  int i, dim;
  SortItem **result;
  char *ptr;

  Assert(groups);
  Assert(ncounts);

                                                         
  ptr = palloc(MAXALIGN(sizeof(SortItem *) * mss->ndims) + mss->ndims * MAXALIGN(sizeof(SortItem) * ngroups));

                                 
  result = (SortItem **)ptr;
  ptr += MAXALIGN(sizeof(SortItem *) * mss->ndims);

  for (dim = 0; dim < mss->ndims; dim++)
  {
    SortSupport ssup = &mss->ssup[dim];

                                             
    result[dim] = (SortItem *)ptr;
    ptr += MAXALIGN(sizeof(SortItem) * ngroups);

                                        
    for (i = 0; i < ngroups; i++)
    {
                                       
      result[dim][i].values = &groups[i].values[dim];
      result[dim][i].isnull = &groups[i].isnull[dim];
      result[dim][i].count = groups[i].count;
    }

                                      
    qsort_arg((void *)result[dim], ngroups, sizeof(SortItem), sort_item_compare, ssup);

       
                                                                   
                                                                   
                                
       
    ncounts[dim] = 1;
    for (i = 1; i < ngroups; i++)
    {
      if (sort_item_compare(&result[dim][i - 1], &result[dim][i], ssup) == 0)
      {
        result[dim][ncounts[dim] - 1].count += result[dim][i].count;
        continue;
      }

      result[dim][ncounts[dim]] = result[dim][i];

      ncounts[dim]++;
    }
  }

  return result;
}

   
                    
                                                               
   
MCVList *
statext_mcv_load(Oid mvoid)
{
  MCVList *result;
  bool isnull;
  Datum mcvlist;
  HeapTuple htup = SearchSysCache1(STATEXTDATASTXOID, ObjectIdGetDatum(mvoid));

  if (!HeapTupleIsValid(htup))
  {
    elog(ERROR, "cache lookup failed for statistics object %u", mvoid);
  }

  mcvlist = SysCacheGetAttr(STATEXTDATASTXOID, htup, Anum_pg_statistic_ext_data_stxdmcv, &isnull);

  if (isnull)
  {
    elog(ERROR, "requested statistics kind \"%c\" is not yet built for statistics object %u", STATS_EXT_DEPENDENCIES, mvoid);
  }

  result = statext_mcv_deserialize(DatumGetByteaP(mcvlist));

  ReleaseSysCache(htup);

  return result;
}

   
                         
                                                 
   
                                                                               
                                                                             
                                                                               
                                            
   
                                                                           
   
                                                                    
                                                                    
                                                                    
   
                                                                              
                                                                             
                                                                             
                                                                    
   
                                                                              
                                                                               
                                                                                
                                                                                  
                                                                              
                                                  
   
                                                                         
                                                                          
                                      
   
                                                                                
                                                               
   
bytea *
statext_mcv_serialize(MCVList *mcvlist, VacAttrStats **stats)
{
  int i;
  int dim;
  int ndims = mcvlist->ndimensions;

  SortSupport ssup;
  DimensionInfo *info;

  Size total_length;

                                                    
  bytea *raw;
  char *ptr;
  char *endptr PG_USED_FOR_ASSERTS_ONLY;

                                                            
  Datum **values = (Datum **)palloc0(sizeof(Datum *) * ndims);
  int *counts = (int *)palloc0(sizeof(int) * ndims);

     
                                                                          
                                                                        
                                                                       
                                                                             
                                                                             
                                               
     
  info = (DimensionInfo *)palloc0(sizeof(DimensionInfo) * ndims);

                                                                     
  ssup = (SortSupport)palloc0(sizeof(SortSupportData) * ndims);

                                                                     
  for (dim = 0; dim < ndims; dim++)
  {
    int ndistinct;
    TypeCacheEntry *typentry;

       
                                                                         
                                                                  
       
    typentry = lookup_type_cache(stats[dim]->attrtypid, TYPECACHE_LT_OPR);

                                                                    
    info[dim].typlen = stats[dim]->attrtype->typlen;
    info[dim].typbyval = stats[dim]->attrtype->typbyval;

                                                                     
    values[dim] = (Datum *)palloc0(sizeof(Datum) * mcvlist->nitems);

    for (i = 0; i < mcvlist->nitems; i++)
    {
                                                                 
      if (mcvlist->items[i].isnull[dim])
      {
        continue;
      }

                                       
      values[dim][counts[dim]] = mcvlist->items[i].values[dim];
      counts[dim] += 1;
    }

                                                                     
    if (counts[dim] == 0)
    {
      continue;
    }

                                       
    ssup[dim].ssup_cxt = CurrentMemoryContext;
    ssup[dim].ssup_collation = stats[dim]->attrcollid;
    ssup[dim].ssup_nulls_first = false;

    PrepareSortSupportFromOrderingOp(typentry->lt_opr, &ssup[dim]);

    qsort_arg(values[dim], counts[dim], sizeof(Datum), compare_scalars_simple, &ssup[dim]);

       
                                                                           
                                                                      
                                                                      
                
       
    ndistinct = 1;                                
    for (i = 1; i < counts[dim]; i++)
    {
                               
      Assert(compare_datums_simple(values[dim][i - 1], values[dim][i], &ssup[dim]) <= 0);

                                                                        
      if (!compare_datums_simple(values[dim][i - 1], values[dim][i], &ssup[dim]))
      {
        continue;
      }

      values[dim][ndistinct] = values[dim][i];
      ndistinct += 1;
    }

                                                                    
    Assert(ndistinct <= PG_UINT16_MAX);

       
                                                                          
                                                                           
                                                                       
                                                  
       
    info[dim].nvalues = ndistinct;

    if (info[dim].typbyval)                          
    {
      info[dim].nbytes = info[dim].nvalues * info[dim].typlen;

         
                                                                       
                                                    
         
      info[dim].nbytes_aligned = 0;
    }
    else if (info[dim].typlen > 0)                          
    {
         
                                                                     
                                                                       
                                                                     
                                                        
         
                                                                      
                                                                
         
      info[dim].nbytes = info[dim].nvalues * info[dim].typlen;
      info[dim].nbytes_aligned = info[dim].nvalues * MAXALIGN(info[dim].typlen);
    }
    else if (info[dim].typlen == -1)              
    {
      info[dim].nbytes = 0;
      info[dim].nbytes_aligned = 0;
      for (i = 0; i < info[dim].nvalues; i++)
      {
        Size len;

           
                                                                   
                                                                      
                                                                    
                                                       
           
        values[dim][i] = PointerGetDatum(PG_DETOAST_DATUM(values[dim][i]));

                                                      
        len = VARSIZE_ANY_EXHDR(values[dim][i]);
        info[dim].nbytes += sizeof(uint32);             
        info[dim].nbytes += len;                                   

           
                                                                     
                                                                  
           
        info[dim].nbytes_aligned += MAXALIGN(VARHDRSZ + len);
      }
    }
    else if (info[dim].typlen == -2)              
    {
      info[dim].nbytes = 0;
      info[dim].nbytes_aligned = 0;
      for (i = 0; i < info[dim].nvalues; i++)
      {
        Size len;

           
                                                                      
                                                                       
                                                                       
                                                               
           

                                                      
        len = strlen(DatumGetCString(values[dim][i])) + 1;
        info[dim].nbytes += sizeof(uint32);             
        info[dim].nbytes += len;                       

                                                                   
        info[dim].nbytes_aligned += MAXALIGN(len);
      }
    }

                                                      
    Assert(info[dim].nbytes > 0);
  }

     
                                                                           
                                                                           
                                                         
     
  total_length = (3 * sizeof(uint32))                                
                 + sizeof(AttrNumber)                      
                 + (ndims * sizeof(Oid));                      

                      
  total_length += ndims * sizeof(DimensionInfo);

                                                       
  for (i = 0; i < ndims; i++)
  {
    total_length += info[i].nbytes;
  }

     
                                                                          
                                                                         
     
  total_length += mcvlist->nitems * ITEM_SIZE(dim);

     
                                                                            
                                                                
     
  raw = (bytea *)palloc0(VARHDRSZ + total_length);
  SET_VARSIZE(raw, VARHDRSZ + total_length);

  ptr = VARDATA(raw);
  endptr = ptr + total_length;

                                                   
  memcpy(ptr, &mcvlist->magic, sizeof(uint32));
  ptr += sizeof(uint32);

  memcpy(ptr, &mcvlist->type, sizeof(uint32));
  ptr += sizeof(uint32);

  memcpy(ptr, &mcvlist->nitems, sizeof(uint32));
  ptr += sizeof(uint32);

  memcpy(ptr, &mcvlist->ndimensions, sizeof(AttrNumber));
  ptr += sizeof(AttrNumber);

  memcpy(ptr, mcvlist->types, sizeof(Oid) * ndims);
  ptr += (sizeof(Oid) * ndims);

                                                                  
  memcpy(ptr, info, sizeof(DimensionInfo) * ndims);
  ptr += sizeof(DimensionInfo) * ndims;

                                                                      
  for (dim = 0; dim < ndims; dim++)
  {
                                                       
    char *start PG_USED_FOR_ASSERTS_ONLY = ptr;

    for (i = 0; i < info[dim].nvalues; i++)
    {
      Datum value = values[dim][i];

      if (info[dim].typbyval)                      
      {
        Datum tmp;

           
                                                                
                                                                     
                                                                 
                                                                 
                                                                     
                                                                      
                                                        
           
        store_att_byval(&tmp, value, info[dim].typlen);

        memcpy(ptr, &tmp, info[dim].typlen);
        ptr += info[dim].typlen;
      }
      else if (info[dim].typlen > 0)                          
      {
                                                                
        memcpy(ptr, DatumGetPointer(value), info[dim].typlen);
        ptr += info[dim].typlen;
      }
      else if (info[dim].typlen == -1)              
      {
        uint32 len = VARSIZE_ANY_EXHDR(DatumGetPointer(value));

                             
        memcpy(ptr, &len, sizeof(uint32));
        ptr += sizeof(uint32);

                                                              
        memcpy(ptr, VARDATA_ANY(DatumGetPointer(value)), len);
        ptr += len;
      }
      else if (info[dim].typlen == -2)              
      {
        uint32 len = (uint32)strlen(DatumGetCString(value)) + 1;

                             
        memcpy(ptr, &len, sizeof(uint32));
        ptr += sizeof(uint32);

                   
        memcpy(ptr, DatumGetCString(value), len);
        ptr += len;
      }

                                      
      Assert((ptr > start) && ((ptr - start) <= info[dim].nbytes));
    }

                                                                 
    Assert((ptr - start) == info[dim].nbytes);
  }

                                                                       
  for (i = 0; i < mcvlist->nitems; i++)
  {
    MCVItem *mcvitem = &mcvlist->items[i];

                                                
    Assert(ptr <= (endptr - ITEM_SIZE(dim)));

                                                               
    memcpy(ptr, mcvitem->isnull, sizeof(bool) * ndims);
    ptr += sizeof(bool) * ndims;

    memcpy(ptr, &mcvitem->frequency, sizeof(double));
    ptr += sizeof(double);

    memcpy(ptr, &mcvitem->base_frequency, sizeof(double));
    ptr += sizeof(double);

                                
    for (dim = 0; dim < ndims; dim++)
    {
      uint16 index = 0;
      Datum *value;

                                                  
      if (!mcvitem->isnull[dim])
      {
        value = (Datum *)bsearch_arg(&mcvitem->values[dim], values[dim], info[dim].nvalues, sizeof(Datum), compare_scalars_simple, &ssup[dim]);

        Assert(value != NULL);                                           

                                                         
        index = (uint16)(value - values[dim]);

                                                       
        Assert(index < info[dim].nvalues);
      }

                                                  
      memcpy(ptr, &index, sizeof(uint16));
      ptr += sizeof(uint16);
    }

                                                         
    Assert(ptr <= endptr);
  }

                                                                 
  Assert(ptr == endptr);

  pfree(values);
  pfree(counts);

  return raw;
}

   
                           
                                                      
   
                                                                            
                                               
   
MCVList *
statext_mcv_deserialize(bytea *data)
{
  int dim, i;
  Size expected_size;
  MCVList *mcvlist;
  char *raw;
  char *ptr;
  char *endptr PG_USED_FOR_ASSERTS_ONLY;

  int ndims, nitems;
  DimensionInfo *info = NULL;

                                                               
  Datum **map = NULL;

                
  Size mcvlen;

                                  
  Size datalen;
  char *dataptr;
  char *valuesptr;
  char *isnullptr;

  if (data == NULL)
  {
    return NULL;
  }

     
                                                                             
                                                                        
                                                                      
     
  if (VARSIZE_ANY(data) < MinSizeOfMCVList)
  {
    elog(ERROR, "invalid MCV size %zd (expected at least %zu)", VARSIZE_ANY(data), MinSizeOfMCVList);
  }

                                
  mcvlist = (MCVList *)palloc0(offsetof(MCVList, items));

                                                          
  raw = (char *)data;
  ptr = VARDATA_ANY(raw);
  endptr = (char *)raw + VARSIZE_ANY(data);

                                                        
  memcpy(&mcvlist->magic, ptr, sizeof(uint32));
  ptr += sizeof(uint32);

  memcpy(&mcvlist->type, ptr, sizeof(uint32));
  ptr += sizeof(uint32);

  memcpy(&mcvlist->nitems, ptr, sizeof(uint32));
  ptr += sizeof(uint32);

  memcpy(&mcvlist->ndimensions, ptr, sizeof(AttrNumber));
  ptr += sizeof(AttrNumber);

  if (mcvlist->magic != STATS_MCV_MAGIC)
  {
    elog(ERROR, "invalid MCV magic %u (expected %u)", mcvlist->magic, STATS_MCV_MAGIC);
  }

  if (mcvlist->type != STATS_MCV_TYPE_BASIC)
  {
    elog(ERROR, "invalid MCV type %u (expected %u)", mcvlist->type, STATS_MCV_TYPE_BASIC);
  }

  if (mcvlist->ndimensions == 0)
  {
    elog(ERROR, "invalid zero-length dimension array in MCVList");
  }
  else if ((mcvlist->ndimensions > STATS_MAX_DIMENSIONS) || (mcvlist->ndimensions < 0))
  {
    elog(ERROR, "invalid length (%d) dimension array in MCVList", mcvlist->ndimensions);
  }

  if (mcvlist->nitems == 0)
  {
    elog(ERROR, "invalid zero-length item array in MCVList");
  }
  else if (mcvlist->nitems > STATS_MCVLIST_MAX_ITEMS)
  {
    elog(ERROR, "invalid length (%u) item array in MCVList", mcvlist->nitems);
  }

  nitems = mcvlist->nitems;
  ndims = mcvlist->ndimensions;

     
                                                                         
                                                                      
                                                              
     
  expected_size = SizeOfMCVList(ndims, nitems);

     
                                                                            
                                                                             
                                                                  
     
  if (VARSIZE_ANY(data) < expected_size)
  {
    elog(ERROR, "invalid MCV size %zd (expected %zu)", VARSIZE_ANY(data), expected_size);
  }

                                        
  memcpy(mcvlist->types, ptr, sizeof(Oid) * ndims);
  ptr += (sizeof(Oid) * ndims);

                                                   
  info = palloc(ndims * sizeof(DimensionInfo));

  memcpy(info, ptr, ndims * sizeof(DimensionInfo));
  ptr += (ndims * sizeof(DimensionInfo));

                                    
  for (dim = 0; dim < ndims; dim++)
  {
       
                                                                       
                                         
       
    Assert(info[dim].nvalues >= 0);
    Assert(info[dim].nbytes >= 0);

    expected_size += info[dim].nbytes;
  }

     
                                                                       
                                                                            
                    
     
  if (VARSIZE_ANY(data) != expected_size)
  {
    elog(ERROR, "invalid MCV size %zd (expected %zu)", VARSIZE_ANY(data), expected_size);
  }

     
                                                                         
                                                                         
                                                      
     
                                                                           
                                                                         
                                         
     
  datalen = 0;                            
  map = (Datum **)palloc(ndims * sizeof(Datum *));

  for (dim = 0; dim < ndims; dim++)
  {
    map[dim] = (Datum *)palloc(sizeof(Datum) * info[dim].nvalues);

                                                          
    datalen += info[dim].nbytes_aligned;
  }

     
                                                                           
     
                                                                             
                                                                             
                                                                   
     
                                                                              
                                                        
     
  mcvlen = MAXALIGN(offsetof(MCVList, items) + (sizeof(MCVItem) * nitems));

                                                           
  mcvlen += nitems * MAXALIGN(sizeof(Datum) * ndims);
  mcvlen += nitems * MAXALIGN(sizeof(bool) * ndims);

                                                                           
  mcvlen += MAXALIGN(datalen);

                                                                           
  mcvlist = repalloc(mcvlist, mcvlen);

                                                        
  valuesptr = (char *)mcvlist + MAXALIGN(offsetof(MCVList, items) + (sizeof(MCVItem) * nitems));

  isnullptr = valuesptr + (nitems * MAXALIGN(sizeof(Datum) * ndims));

  dataptr = isnullptr + (nitems * MAXALIGN(sizeof(bool) * ndims));

     
                                                                             
                                   
     
  for (dim = 0; dim < ndims; dim++)
  {
                                                    
    char *start PG_USED_FOR_ASSERTS_ONLY = ptr;

    if (info[dim].typbyval)
    {
                                                                 
      for (i = 0; i < info[dim].nvalues; i++)
      {
        Datum v = 0;

        memcpy(&v, ptr, info[dim].typlen);
        ptr += info[dim].typlen;

        map[dim][i] = fetch_att(&v, true, info[dim].typlen);

                                              
        Assert(ptr <= (start + info[dim].nbytes));
      }
    }
    else
    {
                                                                    

                                                                  
      if (info[dim].typlen > 0)
      {
        for (i = 0; i < info[dim].nvalues; i++)
        {
          memcpy(dataptr, ptr, info[dim].typlen);
          ptr += info[dim].typlen;

                                         
          map[dim][i] = PointerGetDatum(dataptr);
          dataptr += MAXALIGN(info[dim].typlen);
        }
      }
      else if (info[dim].typlen == -1)
      {
                     
        for (i = 0; i < info[dim].nvalues; i++)
        {
          uint32 len;

                                      
          memcpy(&len, ptr, sizeof(uint32));
          ptr += sizeof(uint32);

                                       
          SET_VARSIZE(dataptr, len + VARHDRSZ);
          memcpy(VARDATA(dataptr), ptr, len);
          ptr += len;

                                         
          map[dim][i] = PointerGetDatum(dataptr);

                                                            
          dataptr += MAXALIGN(len + VARHDRSZ);
        }
      }
      else if (info[dim].typlen == -2)
      {
                     
        for (i = 0; i < info[dim].nvalues; i++)
        {
          uint32 len;

          memcpy(&len, ptr, sizeof(uint32));
          ptr += sizeof(uint32);

          memcpy(dataptr, ptr, len);
          ptr += len;

                                         
          map[dim][i] = PointerGetDatum(dataptr);
          dataptr += MAXALIGN(len);
        }
      }

                                            
      Assert(ptr <= (start + info[dim].nbytes));

                                               
      Assert(dataptr <= ((char *)mcvlist + mcvlen));
    }

                                                                 
    Assert(ptr == (start + info[dim].nbytes));
  }

                                                       
  Assert(dataptr == ((char *)mcvlist + mcvlen));

                                                                     
  for (i = 0; i < nitems; i++)
  {
    MCVItem *item = &mcvlist->items[i];

    item->values = (Datum *)valuesptr;
    valuesptr += MAXALIGN(sizeof(Datum) * ndims);

    item->isnull = (bool *)isnullptr;
    isnullptr += MAXALIGN(sizeof(bool) * ndims);

    memcpy(item->isnull, ptr, sizeof(bool) * ndims);
    ptr += sizeof(bool) * ndims;

    memcpy(&item->frequency, ptr, sizeof(double));
    ptr += sizeof(double);

    memcpy(&item->base_frequency, ptr, sizeof(double));
    ptr += sizeof(double);

                                                           
    for (dim = 0; dim < ndims; dim++)
    {
      uint16 index;

      memcpy(&index, ptr, sizeof(uint16));
      ptr += sizeof(uint16);

      if (item->isnull[dim])
      {
        continue;
      }

      item->values[dim] = map[dim][index];
    }

                                               
    Assert(ptr <= endptr);
  }

                                            
  Assert(ptr == endptr);

                                            
  for (dim = 0; dim < ndims; dim++)
  {
    pfree(map[dim]);
  }

  pfree(map);

  return mcvlist;
}

   
                                                  
   
                          
                           
                                
                                  
                                       
   
                                                                             
                                         
   
Datum
pg_stats_ext_mcvlist_items(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcontext;
    MCVList *mcvlist;
    TupleDesc tupdesc;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

                                                                          
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    mcvlist = statext_mcv_deserialize(PG_GETARG_BYTEA_P(0));

    funcctx->user_fctx = mcvlist;

                                               
    funcctx->max_calls = 0;
    if (funcctx->user_fctx != NULL)
    {
      funcctx->max_calls = mcvlist->nitems;
    }

                                                      
    if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function returning record called in context "
                                                                     "that cannot accept type record")));
    }
    tupdesc = BlessTupleDesc(tupdesc);

       
                                                                           
                 
       
    funcctx->attinmeta = TupleDescGetAttInMetadata(tupdesc);

    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();

  if (funcctx->call_cntr < funcctx->max_calls)                                         
  {
    Datum values[5];
    bool nulls[5];
    HeapTuple tuple;
    Datum result;
    ArrayBuildState *astate_values = NULL;
    ArrayBuildState *astate_nulls = NULL;

    int i;
    MCVList *mcvlist;
    MCVItem *item;

    mcvlist = (MCVList *)funcctx->user_fctx;

    Assert(funcctx->call_cntr < mcvlist->nitems);

    item = &mcvlist->items[funcctx->call_cntr];

    for (i = 0; i < mcvlist->ndimensions; i++)
    {

      astate_nulls = accumArrayResult(astate_nulls, BoolGetDatum(item->isnull[i]), false, BOOLOID, CurrentMemoryContext);

      if (!item->isnull[i])
      {
        bool isvarlena;
        Oid outfunc;
        FmgrInfo fmgrinfo;
        Datum val;
        text *txt;

                                             
        getTypeOutputInfo(mcvlist->types[i], &outfunc, &isvarlena);
        fmgr_info(outfunc, &fmgrinfo);

        val = FunctionCall1(&fmgrinfo, item->values[i]);
        txt = cstring_to_text(DatumGetPointer(val));

        astate_values = accumArrayResult(astate_values, PointerGetDatum(txt), false, TEXTOID, CurrentMemoryContext);
      }
      else
      {
        astate_values = accumArrayResult(astate_values, (Datum)0, true, TEXTOID, CurrentMemoryContext);
      }
    }

    values[0] = Int32GetDatum(funcctx->call_cntr);
    values[1] = PointerGetDatum(makeArrayResult(astate_values, CurrentMemoryContext));
    values[2] = PointerGetDatum(makeArrayResult(astate_nulls, CurrentMemoryContext));
    values[3] = Float8GetDatum(item->frequency);
    values[4] = Float8GetDatum(item->base_frequency);

                               
    memset(nulls, 0, sizeof(nulls));

                       
    tuple = heap_form_tuple(funcctx->attinmeta->tupdesc, values, nulls);

                                     
    result = HeapTupleGetDatum(tuple);

    SRF_RETURN_NEXT(funcctx, result);
  }
  else                                    
  {
    SRF_RETURN_DONE(funcctx);
  }
}

   
                                                         
   
                                                                             
                                       
   
Datum
pg_mcv_list_in(PG_FUNCTION_ARGS)
{
     
                                                                          
                                   
     
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_mcv_list")));

  PG_RETURN_VOID();                          
}

   
                                                           
   
                                                                             
                                                                             
                                                                
   
                                                                         
                                                                        
                                                      
   
Datum
pg_mcv_list_out(PG_FUNCTION_ARGS)
{
  return byteaout(fcinfo);
}

   
                                                                  
   
Datum
pg_mcv_list_recv(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_mcv_list")));

  PG_RETURN_VOID();                          
}

   
                                                                   
   
                                                                         
                                          
   
Datum
pg_mcv_list_send(PG_FUNCTION_ARGS)
{
  return byteasend(fcinfo);
}

   
                        
                                                                     
   
                                                                        
                                                                       
                                                                          
                                                
   
                                                                    
                                                                           
                                                                       
   
                                                                          
                                                                          
                                                                            
                                                                        
                                                           
   
static bool *
mcv_get_match_bitmap(PlannerInfo *root, List *clauses, Bitmapset *keys, MCVList *mcvlist, bool is_or)
{
  int i;
  ListCell *l;
  bool *matches;

                                          
  Assert(clauses != NIL);
  Assert(list_length(clauses) >= 1);
  Assert(mcvlist != NULL);
  Assert(mcvlist->nitems > 0);
  Assert(mcvlist->nitems <= STATS_MCVLIST_MAX_ITEMS);

  matches = palloc(sizeof(bool) * mcvlist->nitems);
  memset(matches, (is_or) ? false : true, sizeof(bool) * mcvlist->nitems);

     
                                                                             
                                                            
     
  foreach (l, clauses)
  {
    Node *clause = (Node *)lfirst(l);

                                                         
    if (IsA(clause, RestrictInfo))
    {
      clause = (Node *)((RestrictInfo *)clause)->clause;
    }

       
                                                                    
                  
       
    if (is_opclause(clause))
    {
      OpExpr *expr = (OpExpr *)clause;
      FmgrInfo opproc;

                                                                     
      Var *var;
      Const *cst;
      bool varonleft;

      fmgr_info(get_opcode(expr->opno), &opproc);

                                                         
      if (examine_opclause_expression(expr, &var, &cst, &varonleft))
      {
        int idx;

                                                                 
        idx = bms_member_index(keys, var->varattno);

           
                                                                       
                                                              
                                                                    
                           
           
        for (i = 0; i < mcvlist->nitems; i++)
        {
          bool match = true;
          MCVItem *item = &mcvlist->items[i];

             
                                                                       
                                                                       
                            
             
          if (item->isnull[idx] || cst->constisnull)
          {
            matches[i] = RESULT_MERGE(matches[i], is_or, false);
            continue;
          }

             
                                                                    
                                                                  
                                                              
             
          if (RESULT_IS_FINAL(matches[i], is_or))
          {
            continue;
          }

             
                                                                 
                                                                    
                                  
             
                                                                     
                                                                    
                                                                      
                                                                    
                                                                   
                                        
             
          if (varonleft)
          {
            match = DatumGetBool(FunctionCall2Coll(&opproc, var->varcollid, item->values[idx], cst->constvalue));
          }
          else
          {
            match = DatumGetBool(FunctionCall2Coll(&opproc, var->varcollid, cst->constvalue, item->values[idx]));
          }

                                                       
          matches[i] = RESULT_MERGE(matches[i], is_or, match);
        }
      }
    }
    else if (IsA(clause, NullTest))
    {
      NullTest *expr = (NullTest *)clause;
      Var *var = (Var *)(expr->arg);

                                                               
      int idx = bms_member_index(keys, var->varattno);

         
                                                                        
                                                                      
                                                                     
         
      for (i = 0; i < mcvlist->nitems; i++)
      {
        bool match = false;                      
        MCVItem *item = &mcvlist->items[i];

                                                                      
        switch (expr->nulltesttype)
        {
        case IS_NULL:
          match = (item->isnull[idx]) ? true : match;
          break;

        case IS_NOT_NULL:
          match = (!item->isnull[idx]) ? true : match;
          break;
        }

                                                                    
        matches[i] = RESULT_MERGE(matches[i], is_or, match);
      }
    }
    else if (is_orclause(clause) || is_andclause(clause))
    {
                                                               

      int i;
      BoolExpr *bool_clause = ((BoolExpr *)clause);
      List *bool_clauses = bool_clause->args;

                                                   
      bool *bool_matches = NULL;

      Assert(bool_clauses != NIL);
      Assert(list_length(bool_clauses) >= 2);

                                                     
      bool_matches = mcv_get_match_bitmap(root, bool_clauses, keys, mcvlist, is_orclause(clause));

         
                                                                    
                                                                        
                                             
         
      for (i = 0; i < mcvlist->nitems; i++)
      {
        matches[i] = RESULT_MERGE(matches[i], is_or, bool_matches[i]);
      }

      pfree(bool_matches);
    }
    else if (is_notclause(clause))
    {
                                                      

      int i;
      BoolExpr *not_clause = ((BoolExpr *)clause);
      List *not_args = not_clause->args;

                                                   
      bool *not_matches = NULL;

      Assert(not_args != NIL);
      Assert(list_length(not_args) == 1);

                                                     
      not_matches = mcv_get_match_bitmap(root, not_args, keys, mcvlist, false);

         
                                                                    
                                                                        
                                                   
         
      for (i = 0; i < mcvlist->nitems; i++)
      {
        matches[i] = RESULT_MERGE(matches[i], is_or, !not_matches[i]);
      }

      pfree(not_matches);
    }
    else if (IsA(clause, Var))
    {
                                                                  

      Var *var = (Var *)(clause);

                                                               
      int idx = bms_member_index(keys, var->varattno);

      Assert(var->vartype == BOOLOID);

         
                                                                        
                                                                      
                                                                     
         
      for (i = 0; i < mcvlist->nitems; i++)
      {
        MCVItem *item = &mcvlist->items[i];
        bool match = false;

                                                  
        if (!item->isnull[idx] && DatumGetBool(item->values[idx]))
        {
          match = true;
        }

                                      
        matches[i] = RESULT_MERGE(matches[i], is_or, match);
      }
    }
    else
    {
      elog(ERROR, "unknown clause type: %d", clause->type);
    }
  }

  return matches;
}

   
                              
                                                                
   
                                                                          
                                      
   
                                                                     
                                                                          
                                                              
   
Selectivity
mcv_clauselist_selectivity(PlannerInfo *root, StatisticExtInfo *stat, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Selectivity *basesel, Selectivity *totalsel)
{
  int i;
  MCVList *mcv;
  Selectivity s = 0.0;

                                               
  bool *matches = NULL;

                                                         
  mcv = statext_mcv_load(stat->statOid);

                                            
  matches = mcv_get_match_bitmap(root, clauses, stat->keys, mcv, false);

                                                      
  *basesel = 0.0;
  *totalsel = 0.0;
  for (i = 0; i < mcv->nitems; i++)
  {
    *totalsel += mcv->items[i].frequency;

    if (matches[i] != false)
    {
                                                                  
      *basesel += mcv->items[i].base_frequency;
      s += mcv->items[i].frequency;
    }
  }

  return s;
}
