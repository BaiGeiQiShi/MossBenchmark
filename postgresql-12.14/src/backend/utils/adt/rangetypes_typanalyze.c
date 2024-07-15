                                                                            
   
                          
                                                           
   
                                                                      
                                                        
   
                                                                      
                                                                         
                                                                          
                                                                         
                                                                          
                                                        
   
                                                                         
                                                                        
   
   
                  
                                                   
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_operator.h"
#include "commands/vacuum.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/rangetypes.h"

static int
float8_qsort_cmp(const void *a1, const void *a2);
static int
range_bound_qsort_cmp(const void *a1, const void *a2, void *arg);
static void
compute_range_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);

   
                                                             
   
Datum
range_typanalyze(PG_FUNCTION_ARGS)
{
  VacAttrStats *stats = (VacAttrStats *)PG_GETARG_POINTER(0);
  TypeCacheEntry *typcache;
  Form_pg_attribute attr = stats->attr;

                                                                       
  typcache = range_get_typcache(fcinfo, getBaseType(stats->attrtypid));

  if (attr->attstattarget < 0)
  {
    attr->attstattarget = default_statistics_target;
  }

  stats->compute_stats = compute_range_stats;
  stats->extra_data = typcache;
                                 
  stats->minrows = 300 * attr->attstattarget;

  PG_RETURN_BOOL(true);
}

   
                                                                    
   
static int
float8_qsort_cmp(const void *a1, const void *a2)
{
  const float8 *f1 = (const float8 *)a1;
  const float8 *f2 = (const float8 *)a2;

  if (*f1 < *f2)
  {
    return -1;
  }
  else if (*f1 == *f2)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

   
                                                
   
static int
range_bound_qsort_cmp(const void *a1, const void *a2, void *arg)
{
  RangeBound *b1 = (RangeBound *)a1;
  RangeBound *b2 = (RangeBound *)a2;
  TypeCacheEntry *typcache = (TypeCacheEntry *)arg;

  return range_cmp_bounds(typcache, b1, b2);
}

   
                                                                  
   
static void
compute_range_stats(VacAttrStats *stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{
  TypeCacheEntry *typcache = (TypeCacheEntry *)stats->extra_data;
  bool has_subdiff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);
  int null_cnt = 0;
  int non_null_cnt = 0;
  int non_empty_cnt = 0;
  int empty_cnt = 0;
  int range_no;
  int slot_idx;
  int num_bins = stats->attr->attstattarget;
  int num_hist;
  float8 *lengths;
  RangeBound *lowers, *uppers;
  double total_width = 0;

                                                                              
  lowers = (RangeBound *)palloc(sizeof(RangeBound) * samplerows);
  uppers = (RangeBound *)palloc(sizeof(RangeBound) * samplerows);
  lengths = (float8 *)palloc(sizeof(float8) * samplerows);

                                    
  for (range_no = 0; range_no < samplerows; range_no++)
  {
    Datum value;
    bool isnull, empty;
    RangeType *range;
    RangeBound lower, upper;
    float8 length;

    vacuum_delay_point();

    value = fetchfunc(stats, range_no, &isnull);
    if (isnull)
    {
                                          
      null_cnt++;
      continue;
    }

       
                                                                       
                                            
       
    total_width += VARSIZE_ANY(DatumGetPointer(value));

                                                            
    range = DatumGetRangeTypeP(value);
    range_deserialize(typcache, range, &lower, &upper, &empty);

    if (!empty)
    {
                                                                      
      lowers[non_empty_cnt] = lower;
      uppers[non_empty_cnt] = upper;

      if (lower.infinite || upper.infinite)
      {
                                                                 
        length = get_float8_infinity();
      }
      else if (has_subdiff)
      {
           
                                                                     
                                   
           
        length = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo, typcache->rng_collation, upper.val, lower.val));
      }
      else
      {
                                                                  
        length = 1.0;
      }
      lengths[non_empty_cnt] = length;

      non_empty_cnt++;
    }
    else
    {
      empty_cnt++;
    }

    non_null_cnt++;
  }

  slot_idx = 0;

                                                                        
  if (non_null_cnt > 0)
  {
    Datum *bound_hist_values;
    Datum *length_hist_values;
    int pos, posfrac, delta, deltafrac, i;
    MemoryContext old_cxt;
    float4 *emptyfrac;

    stats->stats_valid = true;
                                                 
    stats->stanullfrac = (double)null_cnt / (double)samplerows;
    stats->stawidth = total_width / (double)non_null_cnt;

                                                  
    stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);

                                                      
    old_cxt = MemoryContextSwitchTo(stats->anl_context);

       
                                                                        
               
       
    if (non_empty_cnt >= 2)
    {
                             
      qsort_arg(lowers, non_empty_cnt, sizeof(RangeBound), range_bound_qsort_cmp, typcache);
      qsort_arg(uppers, non_empty_cnt, sizeof(RangeBound), range_bound_qsort_cmp, typcache);

      num_hist = non_empty_cnt;
      if (num_hist > num_bins)
      {
        num_hist = num_bins + 1;
      }

      bound_hist_values = (Datum *)palloc(num_hist * sizeof(Datum));

         
                                                                       
                                                                        
                                                                        
                                                                       
                                                                      
                                                                      
                                                                       
                                                                         
                         
         
      delta = (non_empty_cnt - 1) / (num_hist - 1);
      deltafrac = (non_empty_cnt - 1) % (num_hist - 1);
      pos = posfrac = 0;

      for (i = 0; i < num_hist; i++)
      {
        bound_hist_values[i] = PointerGetDatum(range_serialize(typcache, &lowers[pos], &uppers[pos], false));
        pos += delta;
        posfrac += deltafrac;
        if (posfrac >= (num_hist - 1))
        {
                                                                
          pos++;
          posfrac -= (num_hist - 1);
        }
      }

      stats->stakind[slot_idx] = STATISTIC_KIND_BOUNDS_HISTOGRAM;
      stats->stavalues[slot_idx] = bound_hist_values;
      stats->numvalues[slot_idx] = num_hist;
      slot_idx++;
    }

       
                                                                        
               
       
    if (non_empty_cnt >= 2)
    {
         
                                                                
                   
         
      qsort(lengths, non_empty_cnt, sizeof(float8), float8_qsort_cmp);

      num_hist = non_empty_cnt;
      if (num_hist > num_bins)
      {
        num_hist = num_bins + 1;
      }

      length_hist_values = (Datum *)palloc(num_hist * sizeof(Datum));

         
                                                                         
                                                                         
                                                                   
                                                                       
                                                                      
                                                                        
                                                                  
         
      delta = (non_empty_cnt - 1) / (num_hist - 1);
      deltafrac = (non_empty_cnt - 1) % (num_hist - 1);
      pos = posfrac = 0;

      for (i = 0; i < num_hist; i++)
      {
        length_hist_values[i] = Float8GetDatum(lengths[pos]);
        pos += delta;
        posfrac += deltafrac;
        if (posfrac >= (num_hist - 1))
        {
                                                                
          pos++;
          posfrac -= (num_hist - 1);
        }
      }
    }
    else
    {
         
                                                                       
                                                                     
                                                                         
                                                                        
         
      length_hist_values = palloc(0);
      num_hist = 0;
    }
    stats->staop[slot_idx] = Float8LessOperator;
    stats->stacoll[slot_idx] = InvalidOid;
    stats->stavalues[slot_idx] = length_hist_values;
    stats->numvalues[slot_idx] = num_hist;
    stats->statypid[slot_idx] = FLOAT8OID;
    stats->statyplen[slot_idx] = sizeof(float8);
#ifdef USE_FLOAT8_BYVAL
    stats->statypbyval[slot_idx] = true;
#else
    stats->statypbyval[slot_idx] = false;
#endif
    stats->statypalign[slot_idx] = 'd';

                                            
    emptyfrac = (float4 *)palloc(sizeof(float4));
    *emptyfrac = ((double)empty_cnt) / ((double)non_null_cnt);
    stats->stanumbers[slot_idx] = emptyfrac;
    stats->numnumbers[slot_idx] = 1;

    stats->stakind[slot_idx] = STATISTIC_KIND_RANGE_LENGTH_HISTOGRAM;
    slot_idx++;

    MemoryContextSwitchTo(old_cxt);
  }
  else if (null_cnt > 0)
  {
                                                                 
    stats->stats_valid = true;
    stats->stanullfrac = 1.0;
    stats->stawidth = 0;                     
    stats->stadistinct = 0.0;                
  }

     
                                                                            
                                                                       
     
}
