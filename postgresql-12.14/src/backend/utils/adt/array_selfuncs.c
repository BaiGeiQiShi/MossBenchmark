                                                                            
   
                    
                                                             
   
                                                                         
                                                                        
   
   
                  
                                            
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "utils/typcache.h"

                                                              
#define DEFAULT_CONTAIN_SEL 0.005

                                                    
#define DEFAULT_OVERLAP_SEL 0.01

                                            
#define DEFAULT_SEL(operator) ((operator) == OID_ARRAY_OVERLAP_OP ? DEFAULT_OVERLAP_SEL : DEFAULT_CONTAIN_SEL)

static Selectivity
calc_arraycontsel(VariableStatData *vardata, Datum constval, Oid elemtype, Oid operator);
static Selectivity
mcelem_array_selec(ArrayType *array, TypeCacheEntry *typentry, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, float4 *hist, int nhist, Oid operator);
static Selectivity
mcelem_array_contain_overlap_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, Oid operator, TypeCacheEntry * typentry);
static Selectivity
mcelem_array_contained_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, float4 *hist, int nhist, Oid operator, TypeCacheEntry * typentry);
static float *
calc_hist(const float4 *hist, int nhist, int n);
static float *
calc_distr(const float *p, int n, int m, float rest);
static int
floor_log2(uint32 n);
static bool
find_next_mcelem(Datum *mcelem, int nmcelem, Datum value, int *index, TypeCacheEntry *typentry);
static int
element_compare(const void *key1, const void *key2, void *arg);
static int
float_compare_desc(const void *key1, const void *key2);

   
                              
                                                                     
   
                                                                      
                                                                  
                              
   
                                                                               
                                                                            
                                                         
   
                                                                         
   
Selectivity
scalararraysel_containment(PlannerInfo *root, Node *leftop, Node *rightop, Oid elemtype, bool isEquality, bool useOr, int varRelid)
{
  Selectivity selec;
  VariableStatData vardata;
  Datum constval;
  TypeCacheEntry *typentry;
  FmgrInfo *cmpfunc;

     
                                            
     
  examine_variable(root, rightop, varRelid, &vardata);
  if (!vardata.rel)
  {
    ReleaseVariableStats(vardata);
    return -1.0;
  }

     
                                           
     
  if (!IsA(leftop, Const))
  {
    ReleaseVariableStats(vardata);
    return -1.0;
  }
  if (((Const *)leftop)->constisnull)
  {
                                            
    ReleaseVariableStats(vardata);
    return (Selectivity)0.0;
  }
  constval = ((Const *)leftop)->constvalue;

                                                      
  typentry = lookup_type_cache(elemtype, TYPECACHE_CMP_PROC_FINFO);
  if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
  {
    ReleaseVariableStats(vardata);
    return -1.0;
  }
  cmpfunc = &typentry->cmp_proc_finfo;

     
                                                                        
     
  if (!isEquality)
  {
    useOr = !useOr;
  }

                                                     
  if (HeapTupleIsValid(vardata.statsTuple) && statistic_proc_security_check(&vardata, cmpfunc->fn_oid))
  {
    Form_pg_statistic stats;
    AttStatsSlot sslot;
    AttStatsSlot hslot;

    stats = (Form_pg_statistic)GETSTRUCT(vardata.statsTuple);

                                                         
    if (get_attstatsslot(&sslot, vardata.statsTuple, STATISTIC_KIND_MCELEM, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS))
    {
                                                                       
      if (useOr || !get_attstatsslot(&hslot, vardata.statsTuple, STATISTIC_KIND_DECHIST, InvalidOid, ATTSTATSSLOT_NUMBERS))
      {
        memset(&hslot, 0, sizeof(hslot));
      }

         
                                                     
         
                                                     
         
      if (useOr)
      {
        selec = mcelem_array_contain_overlap_selec(sslot.values, sslot.nvalues, sslot.numbers, sslot.nnumbers, &constval, 1, OID_ARRAY_CONTAINS_OP, typentry);
      }
      else
      {
        selec = mcelem_array_contained_selec(sslot.values, sslot.nvalues, sslot.numbers, sslot.nnumbers, &constval, 1, hslot.numbers, hslot.nnumbers, OID_ARRAY_CONTAINED_OP, typentry);
      }

      free_attstatsslot(&hslot);
      free_attstatsslot(&sslot);
    }
    else
    {
                                                       
      if (useOr)
      {
        selec = mcelem_array_contain_overlap_selec(NULL, 0, NULL, 0, &constval, 1, OID_ARRAY_CONTAINS_OP, typentry);
      }
      else
      {
        selec = mcelem_array_contained_selec(NULL, 0, NULL, 0, &constval, 1, NULL, 0, OID_ARRAY_CONTAINED_OP, typentry);
      }
    }

       
                                                                    
       
    selec *= (1.0 - stats->stanullfrac);
  }
  else
  {
                                        
    if (useOr)
    {
      selec = mcelem_array_contain_overlap_selec(NULL, 0, NULL, 0, &constval, 1, OID_ARRAY_CONTAINS_OP, typentry);
    }
    else
    {
      selec = mcelem_array_contained_selec(NULL, 0, NULL, 0, &constval, 1, NULL, 0, OID_ARRAY_CONTAINED_OP, typentry);
    }
                                                               
  }

  ReleaseVariableStats(vardata);

     
                                                
     
  if (!isEquality)
  {
    selec = 1.0 - selec;
  }

  CLAMP_PROBABILITY(selec);

  return selec;
}

   
                                                                          
   
Datum
arraycontsel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Selectivity selec;
  Oid element_typeid;

     
                                                                   
                                                         
     
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {
    PG_RETURN_FLOAT8(DEFAULT_SEL(operator));
  }

     
                                                                          
     
  if (!IsA(other, Const))
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(DEFAULT_SEL(operator));
  }

     
                                                                         
                               
     
  if (((Const *)other)->constisnull)
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(0.0);
  }

     
                                                                             
                                         
     
  if (!varonleft)
  {
    if (operator== OID_ARRAY_CONTAINS_OP)
    {
      operator= OID_ARRAY_CONTAINED_OP;
    }
    else if (operator== OID_ARRAY_CONTAINED_OP)
    {
      operator= OID_ARRAY_CONTAINS_OP;
    }
  }

     
                                                                         
                                                                             
                                                                         
                                                  
     
  element_typeid = get_base_element_type(((Const *)other)->consttype);
  if (element_typeid != InvalidOid && element_typeid == get_base_element_type(vardata.vartype))
  {
    selec = calc_arraycontsel(&vardata, ((Const *)other)->constvalue, element_typeid, operator);
  }
  else
  {
    selec = DEFAULT_SEL(operator);
  }

  ReleaseVariableStats(vardata);

  CLAMP_PROBABILITY(selec);

  PG_RETURN_FLOAT8((float8)selec);
}

   
                                                                       
   
Datum
arraycontjoinsel(PG_FUNCTION_ARGS)
{
                                          
  Oid operator= PG_GETARG_OID(1);

  PG_RETURN_FLOAT8(DEFAULT_SEL(operator));
}

   
                                                                            
                                                     
   
                                                                            
                                                                    
   
static Selectivity
calc_arraycontsel(VariableStatData *vardata, Datum constval, Oid elemtype, Oid operator)
{
  Selectivity selec;
  TypeCacheEntry *typentry;
  FmgrInfo *cmpfunc;
  ArrayType *array;

                                                      
  typentry = lookup_type_cache(elemtype, TYPECACHE_CMP_PROC_FINFO);
  if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
  {
    return DEFAULT_SEL(operator);
  }
  cmpfunc = &typentry->cmp_proc_finfo;

     
                                                                           
                
     
  array = DatumGetArrayTypeP(constval);

  if (HeapTupleIsValid(vardata->statsTuple) && statistic_proc_security_check(vardata, cmpfunc->fn_oid))
  {
    Form_pg_statistic stats;
    AttStatsSlot sslot;
    AttStatsSlot hslot;

    stats = (Form_pg_statistic)GETSTRUCT(vardata->statsTuple);

                                                        
    if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_MCELEM, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS))
    {
         
                                                                      
                         
         
      if (operator!= OID_ARRAY_CONTAINED_OP || !get_attstatsslot(&hslot, vardata->statsTuple, STATISTIC_KIND_DECHIST, InvalidOid, ATTSTATSSLOT_NUMBERS))
      {
        memset(&hslot, 0, sizeof(hslot));
      }

                                                                
      selec = mcelem_array_selec(array, typentry, sslot.values, sslot.nvalues, sslot.numbers, sslot.nnumbers, hslot.numbers, hslot.nnumbers, operator);

      free_attstatsslot(&hslot);
      free_attstatsslot(&sslot);
    }
    else
    {
                                                       
      selec = mcelem_array_selec(array, typentry, NULL, 0, NULL, 0, NULL, 0, operator);
    }

       
                                                                    
       
    selec *= (1.0 - stats->stanullfrac);
  }
  else
  {
                                        
    selec = mcelem_array_selec(array, typentry, NULL, 0, NULL, 0, NULL, 0, operator);
                                                               
  }

                                                         
  if (PointerGetDatum(array) != constval)
  {
    pfree(array);
  }

  return selec;
}

   
                                                                         
   
                                                                            
                                                                           
                                                           
   
static Selectivity
mcelem_array_selec(ArrayType *array, TypeCacheEntry *typentry, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, float4 *hist, int nhist, Oid operator)
{
  Selectivity selec;
  int num_elems;
  Datum *elem_values;
  bool *elem_nulls;
  bool null_present;
  int nonnull_nitems;
  int i;

     
                                                                           
                                                           
     
  deconstruct_array(array, typentry->type_id, typentry->typlen, typentry->typbyval, typentry->typalign, &elem_values, &elem_nulls, &num_elems);

                                      
  nonnull_nitems = 0;
  null_present = false;
  for (i = 0; i < num_elems; i++)
  {
    if (elem_nulls[i])
    {
      null_present = true;
    }
    else
    {
      elem_values[nonnull_nitems++] = elem_values[i];
    }
  }

     
                                                                          
                                                                       
     
  if (null_present && operator== OID_ARRAY_CONTAINS_OP)
  {
    pfree(elem_values);
    pfree(elem_nulls);
    return (Selectivity)0.0;
  }

                                                                        
  qsort_arg(elem_values, nonnull_nitems, sizeof(Datum), element_compare, typentry);

                                            
  if (operator== OID_ARRAY_CONTAINS_OP || operator== OID_ARRAY_OVERLAP_OP)
  {
    selec = mcelem_array_contain_overlap_selec(mcelem, nmcelem, numbers, nnumbers, elem_values, nonnull_nitems, operator, typentry);
  }
  else if (operator== OID_ARRAY_CONTAINED_OP)
  {
    selec = mcelem_array_contained_selec(mcelem, nmcelem, numbers, nnumbers, elem_values, nonnull_nitems, hist, nhist, operator, typentry);
  }
  else
  {
    elog(ERROR, "arraycontsel called for unrecognized operator %u", operator);
    selec = 0.0;                          
  }

  pfree(elem_values);
  pfree(elem_nulls);
  return selec;
}

   
                                                                            
                                                                    
                                
   
                                                                        
                                                                         
                                                                             
   
                                                                         
                                                                  
   
                                                                        
                                                                         
                                                                       
                                              
   
static Selectivity
mcelem_array_contain_overlap_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, Oid operator, TypeCacheEntry * typentry)
{
  Selectivity selec, elem_selec;
  int mcelem_index, i;
  bool use_bsearch;
  float4 minfreq;

     
                                                                            
                                                                        
                                                                            
                   
     
  if (nnumbers != nmcelem + 3)
  {
    numbers = NULL;
    nnumbers = 0;
  }

  if (numbers)
  {
                                            
    minfreq = numbers[nmcelem];
  }
  else
  {
                                                          
    minfreq = 2 * (float4)DEFAULT_CONTAIN_SEL;
  }

                                                                
  if (nitems * floor_log2((uint32)nmcelem) < nmcelem + nitems)
  {
    use_bsearch = true;
  }
  else
  {
    use_bsearch = false;
  }

  if (operator== OID_ARRAY_CONTAINS_OP)
  {
       
                                                                           
                                                         
       
    selec = 1.0;
  }
  else
  {
       
                                                                           
                                                         
       
    selec = 0.0;
  }

                                          
  mcelem_index = 0;
  for (i = 0; i < nitems; i++)
  {
    bool match = false;

                                                  
    if (i > 0 && element_compare(&array_data[i - 1], &array_data[i], typentry) == 0)
    {
      continue;
    }

                                                      
    if (use_bsearch)
    {
      match = find_next_mcelem(mcelem, nmcelem, array_data[i], &mcelem_index, typentry);
    }
    else
    {
      while (mcelem_index < nmcelem)
      {
        int cmp = element_compare(&mcelem[mcelem_index], &array_data[i], typentry);

        if (cmp < 0)
        {
          mcelem_index++;
        }
        else
        {
          if (cmp == 0)
          {
            match = true;                      
          }
          break;
        }
      }
    }

    if (match && numbers)
    {
                                                             
      elem_selec = numbers[mcelem_index];
      mcelem_index++;
    }
    else
    {
         
                                                                  
                                                      
         
      elem_selec = Min(DEFAULT_CONTAIN_SEL, minfreq / 2);
    }

       
                                                                          
                                                             
       
    if (operator== OID_ARRAY_CONTAINS_OP)
    {
      selec *= elem_selec;
    }
    else
    {
      selec = selec + elem_selec - selec * elem_selec;
    }

                                                                        
    CLAMP_PROBABILITY(selec);
  }

  return selec;
}

   
                                                                          
               
   
                                                                        
                                                                         
                                                                             
                                                                              
                                                  
   
                                                                         
                                                                  
   
                                                                           
                                                                             
                                                                               
                                                                        
                                                                          
                                                                        
                                         
   
                                                                           
                                                                            
                                                                         
                                                                       
                                                                            
                                                                            
                                                       
   
                                                                       
                                                                    
                                                 
   
                                                            
                                                                            
   
          
                                                          
                                               
                                                          
                                                                
                                                                     
                                                          
                                                                      
                                                                 
   
                                                                        
                                                                           
   
static Selectivity
mcelem_array_contained_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, float4 *hist, int nhist, Oid operator, TypeCacheEntry * typentry)
{
  int mcelem_index, i, unique_nitems = 0;
  float selec, minfreq, nullelem_freq;
  float *dist, *mcelem_dist, *hist_part;
  float avg_count, mult, rest;
  float *elem_selec;

     
                                                                        
                                                                            
                                                                           
                                                                            
     
  if (numbers == NULL || nnumbers != nmcelem + 3)
  {
    return DEFAULT_CONTAIN_SEL;
  }

                                                       
  if (hist == NULL || nhist < 3)
  {
    return DEFAULT_CONTAIN_SEL;
  }

     
                                                                            
                                                                        
                    
     
  minfreq = numbers[nmcelem];
  nullelem_freq = numbers[nmcelem + 2];
  avg_count = hist[nhist - 1];

     
                                                                   
                                                                           
                                                                             
                                         
     
  rest = avg_count;

     
                                                                         
                                                           
     
  mult = 1.0f;

     
                                                                      
               
     
  elem_selec = (float *)palloc(sizeof(float) * nitems);

                                          
  mcelem_index = 0;
  for (i = 0; i < nitems; i++)
  {
    bool match = false;

                                                  
    if (i > 0 && element_compare(&array_data[i - 1], &array_data[i], typentry) == 0)
    {
      continue;
    }

       
                                                                           
                                                                          
                             
       
    while (mcelem_index < nmcelem)
    {
      int cmp = element_compare(&mcelem[mcelem_index], &array_data[i], typentry);

      if (cmp < 0)
      {
        mult *= (1.0f - numbers[mcelem_index]);
        rest -= numbers[mcelem_index];
        mcelem_index++;
      }
      else
      {
        if (cmp == 0)
        {
          match = true;                      
        }
        break;
      }
    }

    if (match)
    {
                                          
      elem_selec[unique_nitems] = numbers[mcelem_index];
                                                                 
      rest -= numbers[mcelem_index];
      mcelem_index++;
    }
    else
    {
         
                                                                  
                                                      
         
      elem_selec[unique_nitems] = Min(DEFAULT_CONTAIN_SEL, minfreq / 2);
    }

    unique_nitems++;
  }

     
                                                                       
                                                                            
     
  while (mcelem_index < nmcelem)
  {
    mult *= (1.0f - numbers[mcelem_index]);
    rest -= numbers[mcelem_index];
    mcelem_index++;
  }

     
                                                                      
                                                                            
                                                                            
                                   
     
  mult *= exp(-rest);

               
                                                         
                                                   
                                                                      
                                                                             
                                                                            
                                                                            
     
                                                                      
                                                                           
                                                                       
                                                                     
                                                                         
                                                                      
                                                                            
                                                                             
                                                               
               
     
#define EFFORT 100

  if ((nmcelem + unique_nitems) > 0 && unique_nitems > EFFORT * nmcelem / (nmcelem + unique_nitems))
  {
       
                                                                       
                                                        
       
    double b = (double)nmcelem;
    int n;

    n = (int)((sqrt(b * b + 4 * EFFORT * b) - b) / 2);

                                                   
    qsort(elem_selec, unique_nitems, sizeof(float), float_compare_desc);
    unique_nitems = n;
  }

     
                                                                             
                                                                       
                 
     
  dist = calc_distr(elem_selec, unique_nitems, unique_nitems, 0.0f);
  mcelem_dist = calc_distr(numbers, nmcelem, unique_nitems, rest);

                                                                         
  hist_part = calc_hist(hist, nhist - 1, unique_nitems);

  selec = 0.0f;
  for (i = 0; i <= unique_nitems; i++)
  {
       
                                                                    
                                                                           
                                                  
       
    if (mcelem_dist[i] > 0)
    {
      selec += hist_part[i] * mult * dist[i] / mcelem_dist[i];
    }
  }

  pfree(dist);
  pfree(mcelem_dist);
  pfree(hist_part);
  pfree(elem_selec);

                                                     
  selec *= (1.0f - nullelem_freq);

  CLAMP_PROBABILITY(selec);

  return selec;
}

   
                                                                     
                                         
   
                                                                    
                                                
   
                                                                               
                                                                             
                               
   
static float *
calc_hist(const float4 *hist, int nhist, int n)
{
  float *hist_part;
  int k, i = 0;
  float prev_interval = 0, next_interval;
  float frac;

  hist_part = (float *)palloc((n + 1) * sizeof(float));

     
                                                                            
                                                                            
                         
     
  frac = 1.0f / ((float)(nhist - 1));

  for (k = 0; k <= n; k++)
  {
    int count = 0;

       
                                                                           
                                                                     
                                                                           
                                                        
       
    while (i < nhist && hist[i] <= k)
    {
      count++;
      i++;
    }

    if (count > 0)
    {
                                                               
      float val;

                                                                        
      if (i < nhist)
      {
        next_interval = hist[i] - hist[i - 1];
      }
      else
      {
        next_interval = 0;
      }

         
                                                                
                                                                     
                                                               
         
      val = (float)(count - 1);
      if (next_interval > 0)
      {
        val += 0.5f / next_interval;
      }
      if (prev_interval > 0)
      {
        val += 0.5f / prev_interval;
      }
      hist_part[k] = frac * val;

      prev_interval = next_interval;
    }
    else
    {
                                                          
      if (prev_interval > 0)
      {
        hist_part[k] = frac / prev_interval;
      }
      else
      {
        hist_part[k] = 0.0f;
      }
    }
  }

  return hist_part;
}

   
                                                                        
                                                                             
                                         
   
                                                                            
                  
   
                                                                           
                                                                              
                                                                             
                                                       
                                                              
                      
                                                
   
static float *
calc_distr(const float *p, int n, int m, float rest)
{
  float *row, *prev_row, *tmp;
  int i, j;

     
                                                                       
                                                                   
     
  row = (float *)palloc((m + 1) * sizeof(float));
  prev_row = (float *)palloc((m + 1) * sizeof(float));

                  
  row[0] = 1.0f;
  for (i = 1; i <= n; i++)
  {
    float t = p[i - 1];

                   
    tmp = row;
    row = prev_row;
    prev_row = tmp;

                            
    for (j = 0; j <= i && j <= m; j++)
    {
      float val = 0.0f;

      if (j < i)
      {
        val += prev_row[j] * (1.0f - t);
      }
      if (j > 0)
      {
        val += prev_row[j - 1] * t;
      }
      row[j] = val;
    }
  }

     
                                                                         
                                                                        
                           
     
  if (rest > DEFAULT_CONTAIN_SEL)
  {
    float t;

                   
    tmp = row;
    row = prev_row;
    prev_row = tmp;

    for (i = 0; i <= m; i++)
    {
      row[i] = 0.0f;
    }

                                                         
    t = exp(-rest);

       
                                                                         
                             
       
    for (i = 0; i <= m; i++)
    {
      for (j = 0; j <= m - i; j++)
      {
        row[j + i] += prev_row[j] * t;
      }

                                                                  
      t *= rest / (float)(i + 1);
    }
  }

  pfree(prev_row);
  return row;
}

                                                                     
static int
floor_log2(uint32 n)
{
  int logval = 0;

  if (n == 0)
  {
    return -1;
  }
  if (n >= (1 << 16))
  {
    n >>= 16;
    logval += 16;
  }
  if (n >= (1 << 8))
  {
    n >>= 8;
    logval += 8;
  }
  if (n >= (1 << 4))
  {
    n >>= 4;
    logval += 4;
  }
  if (n >= (1 << 2))
  {
    n >>= 2;
    logval += 2;
  }
  if (n >= (1 << 1))
  {
    logval += 1;
  }
  return logval;
}

   
                                                                           
                                                                             
                                                                         
                                                                           
                 
   
static bool
find_next_mcelem(Datum *mcelem, int nmcelem, Datum value, int *index, TypeCacheEntry *typentry)
{
  int l = *index, r = nmcelem - 1, i, res;

  while (l <= r)
  {
    i = (l + r) / 2;
    res = element_compare(&mcelem[i], &value, typentry);
    if (res == 0)
    {
      *index = i;
      return true;
    }
    else if (res < 0)
    {
      l = i + 1;
    }
    else
    {
      r = i - 1;
    }
  }
  *index = l;
  return false;
}

   
                                     
   
                                                                              
                                       
   
                                                 
   
static int
element_compare(const void *key1, const void *key2, void *arg)
{
  Datum d1 = *((const Datum *)key1);
  Datum d2 = *((const Datum *)key2);
  TypeCacheEntry *typentry = (TypeCacheEntry *)arg;
  FmgrInfo *cmpfunc = &typentry->cmp_proc_finfo;
  Datum c;

  c = FunctionCall2Coll(cmpfunc, typentry->typcollation, d1, d2);
  return DatumGetInt32(c);
}

   
                                                                 
   
static int
float_compare_desc(const void *key1, const void *key2)
{
  float d1 = *((const float *)key1);
  float d2 = *((const float *)key2);

  if (d1 > d2)
  {
    return -1;
  }
  else if (d1 < d2)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}
