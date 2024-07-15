                                                                            
   
                      
                                                           
   
                                                                         
                                                                        
   
   
                  
                                              
   
                                                                            
   
#include "postgres.h"

#include "access/tuptoaster.h"
#include "commands/vacuum.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

   
                                                                               
                                                                            
                                                                             
                                                                           
                                            
   
#define ARRAY_WIDTH_THRESHOLD 0x10000

                                                 
typedef struct
{
                                            
  Oid type_id;                           
  Oid eq_opr;                                         
  Oid coll_id;                         
  bool typbyval;                                          
  int16 typlen;
  char typalign;

     
                                                                             
                                                                            
                                  
     
  FmgrInfo *cmp;
  FmgrInfo *hash;

                                         
  AnalyzeAttrComputeStatsFunc std_compute_stats;
  void *std_extra_data;
} ArrayAnalyzeExtraData;

   
                                                                             
                                                                      
                                                                            
                                          
   
static ArrayAnalyzeExtraData *array_extra_data;

                                                         
typedef struct
{
  Datum key;                                                  
  int frequency;                        
  int delta;                                    
  int last_container;                                            
} TrackItem;

                                                     
typedef struct
{
  int count;                                                 
  int frequency;                                            
} DECountItem;

static void
compute_array_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static void
prune_element_hashtable(HTAB *elements_tab, int b_current);
static uint32
element_hash(const void *key, Size keysize);
static int
element_match(const void *key1, const void *key2, Size keysize);
static int
element_compare(const void *key1, const void *key2);
static int
trackitem_compare_frequencies_desc(const void *e1, const void *e2);
static int
trackitem_compare_element(const void *e1, const void *e2);
static int
countitem_compare_count(const void *e1, const void *e2);

   
                                                             
   
Datum
array_typanalyze(PG_FUNCTION_ARGS)
{
  VacAttrStats *stats = (VacAttrStats *)PG_GETARG_POINTER(0);
  Oid element_typeid;
  TypeCacheEntry *typentry;
  ArrayAnalyzeExtraData *extra_data;

     
                                                                        
                                                                       
     
  if (!std_typanalyze(stats))
  {
    PG_RETURN_BOOL(false);
  }

     
                                                                          
     
  element_typeid = get_base_element_type(stats->attrtypid);
  if (!OidIsValid(element_typeid))
  {
    elog(ERROR, "array_typanalyze was invoked for non-array type %u", stats->attrtypid);
  }

     
                                                                    
                                                                         
     
  typentry = lookup_type_cache(element_typeid, TYPECACHE_EQ_OPR | TYPECACHE_CMP_PROC_FINFO | TYPECACHE_HASH_PROC_FINFO);

  if (!OidIsValid(typentry->eq_opr) || !OidIsValid(typentry->cmp_proc_finfo.fn_oid) || !OidIsValid(typentry->hash_proc_finfo.fn_oid))
  {
    PG_RETURN_BOOL(true);
  }

                                                           
  extra_data = (ArrayAnalyzeExtraData *)palloc(sizeof(ArrayAnalyzeExtraData));
  extra_data->type_id = typentry->type_id;
  extra_data->eq_opr = typentry->eq_opr;
  extra_data->coll_id = stats->attrcollid;                              
  extra_data->typbyval = typentry->typbyval;
  extra_data->typlen = typentry->typlen;
  extra_data->typalign = typentry->typalign;
  extra_data->cmp = &typentry->cmp_proc_finfo;
  extra_data->hash = &typentry->hash_proc_finfo;

                                                                       
  extra_data->std_compute_stats = stats->compute_stats;
  extra_data->std_extra_data = stats->extra_data;

                                     
  stats->compute_stats = compute_array_stats;
  stats->extra_data = extra_data;

     
                                                                           
                                               
     

  PG_RETURN_BOOL(true);
}

   
                                                                   
   
                                                                           
                                                                         
                                                             
   
                                                                          
                                                                               
                                                                         
                                                                              
                                                                      
                                                                              
                                                                              
                   
   
                                                                               
                                                                         
                                                                              
                                                                         
                                            
   
                                                         
                                                                          
                                                                            
                                                                             
                                                                             
                                                                             
                                                                            
                                                                            
                                                                       
                                                                            
                                                                         
                                                                          
                                                                            
                                                                      
                                                                         
                                                                          
                                                                            
                                                                       
                                                                          
   
                                                                        
                                                                         
                                                                        
                                                                       
                                              
   
                                                                            
                                                                         
                                                                       
                                                                             
                                                                 
   
static void
compute_array_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{
  ArrayAnalyzeExtraData *extra_data;
  int num_mcelem;
  int null_elem_cnt = 0;
  int analyzed_rows = 0;

                                        
  HTAB *elements_tab;
  HASHCTL elem_hash_ctl;
  HASH_SEQ_STATUS scan_status;

                                                               
  int b_current;

                                         
  int bucket_width;
  int array_no;
  int64 element_no;
  TrackItem *item;
  int slot_idx;
  HTAB *count_tab;
  HASHCTL count_hash_ctl;
  DECountItem *count_item;

  extra_data = (ArrayAnalyzeExtraData *)stats->extra_data;

     
                                                                          
                                                                          
                               
     
  stats->extra_data = extra_data->std_extra_data;
  extra_data->std_compute_stats(stats, fetchfunc, samplerows, totalrows);
  stats->extra_data = extra_data;

     
                                                                         
                                                                         
                        
     
  array_extra_data = extra_data;

     
                                                                       
                                                                           
                                                                           
                                                                
     
  num_mcelem = stats->attr->attstattarget * 10;

     
                                                                        
            
     
  bucket_width = num_mcelem * 1000 / 7;

     
                                                                           
                                                                             
                                                 
     
  MemSet(&elem_hash_ctl, 0, sizeof(elem_hash_ctl));
  elem_hash_ctl.keysize = sizeof(Datum);
  elem_hash_ctl.entrysize = sizeof(TrackItem);
  elem_hash_ctl.hash = element_hash;
  elem_hash_ctl.match = element_match;
  elem_hash_ctl.hcxt = CurrentMemoryContext;
  elements_tab = hash_create("Analyzed elements table", num_mcelem, &elem_hash_ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

                                                    
  MemSet(&count_hash_ctl, 0, sizeof(count_hash_ctl));
  count_hash_ctl.keysize = sizeof(int);
  count_hash_ctl.entrysize = sizeof(DECountItem);
  count_hash_ctl.hcxt = CurrentMemoryContext;
  count_tab = hash_create("Array distinct element count table", 64, &count_hash_ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);

                            
  b_current = 1;
  element_no = 0;

                             
  for (array_no = 0; array_no < samplerows; array_no++)
  {
    Datum value;
    bool isnull;
    ArrayType *array;
    int num_elems;
    Datum *elem_values;
    bool *elem_nulls;
    bool null_present;
    int j;
    int64 prev_element_no = element_no;
    int distinct_count;
    bool count_item_found;

    vacuum_delay_point();

    value = fetchfunc(stats, array_no, &isnull);
    if (isnull)
    {
                                               
      continue;
    }

                                
    if (toast_raw_datum_size(value) > ARRAY_WIDTH_THRESHOLD)
    {
      continue;
    }
    else
    {
      analyzed_rows++;
    }

       
                                                                     
       
    array = DatumGetArrayTypeP(value);

    Assert(ARR_ELEMTYPE(array) == extra_data->type_id);
    deconstruct_array(array, extra_data->type_id, extra_data->typlen, extra_data->typbyval, extra_data->typalign, &elem_values, &elem_nulls, &num_elems);

       
                                                                     
                           
       
    null_present = false;
    for (j = 0; j < num_elems; j++)
    {
      Datum elem_value;
      bool found;

                                                                   
      if (elem_nulls[j])
      {
        null_present = true;
        continue;
      }

                                                                 
      elem_value = elem_values[j];
      item = (TrackItem *)hash_search(elements_tab, (const void *)&elem_value, HASH_ENTER, &found);

      if (found)
      {
                                                               

           
                                                                       
                                                               
           
        if (item->last_container == array_no)
        {
          continue;
        }

        item->frequency++;
        item->last_container = array_no;
      }
      else
      {
                                                  

           
                                                                      
                                                                       
                                                                  
                                                                    
                                                        
           
        item->key = datumCopy(elem_value, extra_data->typbyval, extra_data->typlen);

        item->frequency = 1;
        item->delta = b_current - 1;
        item->last_container = array_no;
      }

                                                                 
      element_no++;

                                                                 
      if (element_no % bucket_width == 0)
      {
        prune_element_hashtable(elements_tab, b_current);
        b_current++;
      }
    }

                                                     
    if (null_present)
    {
      null_elem_cnt++;
    }

                                                                          
    distinct_count = (int)(element_no - prev_element_no);
    count_item = (DECountItem *)hash_search(count_tab, &distinct_count, HASH_ENTER, &count_item_found);

    if (count_item_found)
    {
      count_item->frequency++;
    }
    else
    {
      count_item->frequency = 1;
    }

                                                 
    if (PointerGetDatum(array) != value)
    {
      pfree(array);
    }
    pfree(elem_values);
    pfree(elem_nulls);
  }

                                                               
  slot_idx = 0;
  while (slot_idx < STATISTIC_NUM_SLOTS && stats->stakind[slot_idx] != 0)
  {
    slot_idx++;
  }
  if (slot_idx > STATISTIC_NUM_SLOTS - 2)
  {
    elog(ERROR, "insufficient pg_statistic slots for array stats");
  }

                                                                        
  if (analyzed_rows > 0)
  {
    int nonnull_cnt = analyzed_rows;
    int count_items_count;
    int i;
    TrackItem **sort_table;
    int track_len;
    int64 cutoff_freq;
    int64 minfreq, maxfreq;

       
                                                                      
                                                                      
                                                                      
              
       

       
                                                                       
                                                                          
                                                              
       
                                                                     
                                        
       
    cutoff_freq = 9 * element_no / bucket_width;

    i = hash_get_num_entries(elements_tab);                          
    sort_table = (TrackItem **)palloc(sizeof(TrackItem *) * i);

    hash_seq_init(&scan_status, elements_tab);
    track_len = 0;
    minfreq = element_no;
    maxfreq = 0;
    while ((item = (TrackItem *)hash_seq_search(&scan_status)) != NULL)
    {
      if (item->frequency > cutoff_freq)
      {
        sort_table[track_len++] = item;
        minfreq = Min(minfreq, item->frequency);
        maxfreq = Max(maxfreq, item->frequency);
      }
    }
    Assert(track_len <= i);

                                                 
    elog(DEBUG3,
        "compute_array_stats: target # mces = %d, "
        "bucket width = %d, "
        "# elements = " INT64_FORMAT ", hashtable size = %d, "
        "usable entries = %d",
        num_mcelem, bucket_width, element_no, i, track_len);

       
                                                                          
                                                                           
                                                          
       
    if (num_mcelem < track_len)
    {
      qsort(sort_table, track_len, sizeof(TrackItem *), trackitem_compare_frequencies_desc);
                                                                 
      minfreq = sort_table[num_mcelem - 1]->frequency;
    }
    else
    {
      num_mcelem = track_len;
    }

                                    
    if (num_mcelem > 0)
    {
      MemoryContext old_context;
      Datum *mcelem_values;
      float4 *mcelem_freqs;

         
                                                                       
                                                                       
                                                                   
         
      qsort(sort_table, num_mcelem, sizeof(TrackItem *), trackitem_compare_element);

                                                        
      old_context = MemoryContextSwitchTo(stats->anl_context);

         
                                                                      
                                                                        
                                                                     
                                                                         
         
      mcelem_values = (Datum *)palloc(num_mcelem * sizeof(Datum));
      mcelem_freqs = (float4 *)palloc((num_mcelem + 3) * sizeof(float4));

         
                                                                        
                                        
         
      for (i = 0; i < num_mcelem; i++)
      {
        TrackItem *item = sort_table[i];

        mcelem_values[i] = datumCopy(item->key, extra_data->typbyval, extra_data->typlen);
        mcelem_freqs[i] = (double)item->frequency / (double)nonnull_cnt;
      }
      mcelem_freqs[i++] = (double)minfreq / (double)nonnull_cnt;
      mcelem_freqs[i++] = (double)maxfreq / (double)nonnull_cnt;
      mcelem_freqs[i++] = (double)null_elem_cnt / (double)nonnull_cnt;

      MemoryContextSwitchTo(old_context);

      stats->stakind[slot_idx] = STATISTIC_KIND_MCELEM;
      stats->staop[slot_idx] = extra_data->eq_opr;
      stats->stacoll[slot_idx] = extra_data->coll_id;
      stats->stanumbers[slot_idx] = mcelem_freqs;
                                                           
      stats->numnumbers[slot_idx] = num_mcelem + 3;
      stats->stavalues[slot_idx] = mcelem_values;
      stats->numvalues[slot_idx] = num_mcelem;
                                                 
      stats->statypid[slot_idx] = extra_data->type_id;
      stats->statyplen[slot_idx] = extra_data->typlen;
      stats->statypbyval[slot_idx] = extra_data->typbyval;
      stats->statypalign[slot_idx] = extra_data->typalign;
      slot_idx++;
    }

                                     
    count_items_count = hash_get_num_entries(count_tab);
    if (count_items_count > 0)
    {
      int num_hist = stats->attr->attstattarget;
      DECountItem **sorted_count_items;
      int j;
      int delta;
      int64 frac;
      float4 *hist;

                                                                  
      num_hist = Max(num_hist, 2);

         
                                                                     
                                 
         
      sorted_count_items = (DECountItem **)palloc(sizeof(DECountItem *) * count_items_count);
      hash_seq_init(&scan_status, count_tab);
      j = 0;
      while ((count_item = (DECountItem *)hash_seq_search(&scan_status)) != NULL)
      {
        sorted_count_items[j++] = count_item;
      }
      qsort(sorted_count_items, count_items_count, sizeof(DECountItem *), countitem_compare_count);

         
                                                                        
                                                                   
         
      hist = (float4 *)MemoryContextAlloc(stats->anl_context, sizeof(float4) * (num_hist + 1));
      hist[num_hist] = (double)element_no / (double)nonnull_cnt;

                   
                                                                    
         
                                                                    
                                                                       
                                                                    
                                                                         
                                                                        
                                                                  
                                                                        
                                                                     
                                                                        
                                                                    
                                                                 
                                                                        
                                                                       
                                                                      
                                                                       
                                                                       
                                                                        
                                                                       
                                                                     
                                                                          
                                                                        
                                                                
         
                                                                       
                                                                        
                        
                   
         
      delta = analyzed_rows - 1;
      j = 0;                                          
                                                                       
      frac = (int64)sorted_count_items[0]->frequency * (num_hist - 1);
      for (i = 0; i < num_hist; i++)
      {
        while (frac <= 0)
        {
                                                       
          j++;
          frac += (int64)sorted_count_items[j]->frequency * (num_hist - 1);
        }
        hist[i] = sorted_count_items[j]->count;
        frac -= delta;                                        
      }
      Assert(j == count_items_count - 1);

      stats->stakind[slot_idx] = STATISTIC_KIND_DECHIST;
      stats->staop[slot_idx] = extra_data->eq_opr;
      stats->stacoll[slot_idx] = extra_data->coll_id;
      stats->stanumbers[slot_idx] = hist;
      stats->numnumbers[slot_idx] = num_hist + 1;
      slot_idx++;
    }
  }

     
                                                                            
                                                                       
     
}

   
                                                                          
                                                           
   
static void
prune_element_hashtable(HTAB *elements_tab, int b_current)
{
  HASH_SEQ_STATUS scan_status;
  TrackItem *item;

  hash_seq_init(&scan_status, elements_tab);
  while ((item = (TrackItem *)hash_seq_search(&scan_status)) != NULL)
  {
    if (item->frequency + item->delta <= b_current)
    {
      Datum value = item->key;

      if (hash_search(elements_tab, (const void *)&item->key, HASH_REMOVE, NULL) == NULL)
      {
        elog(ERROR, "hash table corrupted");
      }
                                                                   
      if (!array_extra_data->typbyval)
      {
        pfree(DatumGetPointer(value));
      }
    }
  }
}

   
                               
   
                                                                            
                                       
   
static uint32
element_hash(const void *key, Size keysize)
{
  Datum d = *((const Datum *)key);
  Datum h;

  h = FunctionCall1Coll(array_extra_data->hash, array_extra_data->coll_id, d);
  return DatumGetUInt32(h);
}

   
                                                                    
   
static int
element_match(const void *key1, const void *key2, Size keysize)
{
                                                 
  return element_compare(key1, key2);
}

   
                                     
   
                                                                             
                                       
   
                                                 
   
static int
element_compare(const void *key1, const void *key2)
{
  Datum d1 = *((const Datum *)key1);
  Datum d2 = *((const Datum *)key2);
  Datum c;

  c = FunctionCall2Coll(array_extra_data->cmp, array_extra_data->coll_id, d1, d2);
  return DatumGetInt32(c);
}

   
                                                                              
   
static int
trackitem_compare_frequencies_desc(const void *e1, const void *e2)
{
  const TrackItem *const *t1 = (const TrackItem *const *)e1;
  const TrackItem *const *t2 = (const TrackItem *const *)e2;

  return (*t2)->frequency - (*t1)->frequency;
}

   
                                                               
   
static int
trackitem_compare_element(const void *e1, const void *e2)
{
  const TrackItem *const *t1 = (const TrackItem *const *)e1;
  const TrackItem *const *t2 = (const TrackItem *const *)e2;

  return element_compare(&(*t1)->key, &(*t2)->key);
}

   
                                                        
   
static int
countitem_compare_count(const void *e1, const void *e2)
{
  const DECountItem *const *t1 = (const DECountItem *const *)e1;
  const DECountItem *const *t2 = (const DECountItem *const *)e2;

  if ((*t1)->count < (*t2)->count)
  {
    return -1;
  }
  else if ((*t1)->count == (*t2)->count)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}
