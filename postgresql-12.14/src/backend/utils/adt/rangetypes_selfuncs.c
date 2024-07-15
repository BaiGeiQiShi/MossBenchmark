                                                                            
   
                         
                                                             
   
                                                                        
                             
   
                                                                         
                                                                        
   
   
                  
                                                 
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_type.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/rangetypes.h"
#include "utils/selfuncs.h"
#include "utils/typcache.h"

static double
calc_rangesel(TypeCacheEntry *typcache, VariableStatData *vardata, RangeType *constval, Oid operator);
static double default_range_selectivity(Oid
    operator);
static double
calc_hist_selectivity(TypeCacheEntry *typcache, VariableStatData *vardata, RangeType *constval, Oid operator);
static double
calc_hist_selectivity_scalar(TypeCacheEntry *typcache, RangeBound *constbound, RangeBound *hist, int hist_nvalues, bool equal);
static int
rbound_bsearch(TypeCacheEntry *typcache, RangeBound *value, RangeBound *hist, int hist_length, bool equal);
static float8
get_position(TypeCacheEntry *typcache, RangeBound *value, RangeBound *hist1, RangeBound *hist2);
static float8
get_len_position(double value, double hist1, double hist2);
static float8
get_distance(TypeCacheEntry *typcache, RangeBound *bound1, RangeBound *bound2);
static int
length_hist_bsearch(Datum *length_hist_values, int length_hist_nvalues, double value, bool equal);
static double
calc_length_hist_frac(Datum *length_hist_values, int length_hist_nvalues, double length1, double length2, bool equal);
static double
calc_hist_selectivity_contained(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, RangeBound *hist_lower, int hist_nvalues, Datum *length_hist_values, int length_hist_nvalues);
static double
calc_hist_selectivity_contains(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, RangeBound *hist_lower, int hist_nvalues, Datum *length_hist_values, int length_hist_nvalues);

   
                                                                            
                                                       
   
static double
default_range_selectivity(Oid operator)
{
  switch (operator)
  {
  case OID_RANGE_OVERLAP_OP:
    return 0.01;

  case OID_RANGE_CONTAINS_OP:
  case OID_RANGE_CONTAINED_OP:
    return 0.005;

  case OID_RANGE_CONTAINS_ELEM_OP:
  case OID_RANGE_ELEM_CONTAINED_OP:

       
                                                             
                                       
       
    return DEFAULT_RANGE_INEQ_SEL;

  case OID_RANGE_LESS_OP:
  case OID_RANGE_LESS_EQUAL_OP:
  case OID_RANGE_GREATER_OP:
  case OID_RANGE_GREATER_EQUAL_OP:
  case OID_RANGE_LEFT_OP:
  case OID_RANGE_RIGHT_OP:
  case OID_RANGE_OVERLAPS_LEFT_OP:
  case OID_RANGE_OVERLAPS_RIGHT_OP:
                                                          
    return DEFAULT_INEQ_SEL;

  default:
                                                                       
    return 0.01;
  }
}

   
                                                           
   
Datum
rangesel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Selectivity selec;
  TypeCacheEntry *typcache = NULL;
  RangeType *constrange = NULL;

     
                                                                   
                                                         
     
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {
    PG_RETURN_FLOAT8(default_range_selectivity(operator));
  }

     
                                                                          
     
  if (!IsA(other, Const))
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(default_range_selectivity(operator));
  }

     
                                                                             
                 
     
  if (((Const *)other)->constisnull)
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(0.0);
  }

     
                                                                             
                                         
     
  if (!varonleft)
  {
                                                            
    operator= get_commutator(operator);
    if (!operator)
    {
                                                                       
      ReleaseVariableStats(vardata);
      PG_RETURN_FLOAT8(default_range_selectivity(operator));
    }
  }

     
                                                                         
                                                                             
                                                                           
                                      
     
                                                                          
                                                                           
                                                                             
                      
     
  if (operator== OID_RANGE_CONTAINS_ELEM_OP)
  {
    typcache = range_get_typcache(fcinfo, vardata.vartype);

    if (((Const *)other)->consttype == typcache->rngelemtype->type_id)
    {
      RangeBound lower, upper;

      lower.inclusive = true;
      lower.val = ((Const *)other)->constvalue;
      lower.infinite = false;
      lower.lower = true;
      upper.inclusive = true;
      upper.val = ((Const *)other)->constvalue;
      upper.infinite = false;
      upper.lower = false;
      constrange = range_serialize(typcache, &lower, &upper, false);
    }
  }
  else if (operator== OID_RANGE_ELEM_CONTAINED_OP)
  {
       
                                                                           
                                                                        
                                                  
       
  }
  else if (((Const *)other)->consttype == vardata.vartype)
  {
                                            
    typcache = range_get_typcache(fcinfo, vardata.vartype);

    constrange = DatumGetRangeTypeP(((Const *)other)->constvalue);
  }

     
                                                                        
                                                                             
                                                        
                                  
     
  if (constrange)
  {
    selec = calc_rangesel(typcache, &vardata, constrange, operator);
  }
  else
  {
    selec = default_range_selectivity(operator);
  }

  ReleaseVariableStats(vardata);

  CLAMP_PROBABILITY(selec);

  PG_RETURN_FLOAT8((float8)selec);
}

static double
calc_rangesel(TypeCacheEntry *typcache, VariableStatData *vardata, RangeType *constval, Oid operator)
{
  double hist_selec;
  double selec;
  float4 empty_frac, null_frac;

     
                                                                             
     
  if (HeapTupleIsValid(vardata->statsTuple))
  {
    Form_pg_statistic stats;
    AttStatsSlot sslot;

    stats = (Form_pg_statistic)GETSTRUCT(vardata->statsTuple);
    null_frac = stats->stanullfrac;

                                             
    if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_RANGE_LENGTH_HISTOGRAM, InvalidOid, ATTSTATSSLOT_NUMBERS))
    {
      if (sslot.nnumbers != 1)
      {
        elog(ERROR, "invalid empty fraction statistic");                       
      }
      empty_frac = sslot.numbers[0];
      free_attstatsslot(&sslot);
    }
    else
    {
                                                                
      empty_frac = 0.0;
    }
  }
  else
  {
       
                                                                     
                                                                           
                                                                   
                                          
       
    null_frac = 0.0;
    empty_frac = 0.0;
  }

  if (RangeIsEmpty(constval))
  {
       
                                                                        
                                 
       
    switch (operator)
    {
                                                          
    case OID_RANGE_OVERLAP_OP:
    case OID_RANGE_OVERLAPS_LEFT_OP:
    case OID_RANGE_OVERLAPS_RIGHT_OP:
    case OID_RANGE_LEFT_OP:
    case OID_RANGE_RIGHT_OP:
                                               
    case OID_RANGE_LESS_OP:
      selec = 0.0;
      break;

                                                                
    case OID_RANGE_CONTAINED_OP:
                                                   
    case OID_RANGE_LESS_EQUAL_OP:
      selec = empty_frac;
      break;

                                              
    case OID_RANGE_CONTAINS_OP:
                                           
    case OID_RANGE_GREATER_EQUAL_OP:
      selec = 1.0;
      break;

                                                     
    case OID_RANGE_GREATER_OP:
      selec = 1.0 - empty_frac;
      break;

                                      
    case OID_RANGE_CONTAINS_ELEM_OP:
    default:
      elog(ERROR, "unexpected operator %u", operator);
      selec = 0.0;                          
      break;
    }
  }
  else
  {
       
                                                                       
                                                                      
                                                                       
                                                                       
                                                                       
                                                   
       
    hist_selec = calc_hist_selectivity(typcache, vardata, constval, operator);
    if (hist_selec < 0.0)
    {
      hist_selec = default_range_selectivity(operator);
    }

       
                                                                
                                                                  
                                   
       
    if (operator== OID_RANGE_CONTAINED_OP)
    {
                                                    
      selec = (1.0 - empty_frac) * hist_selec + empty_frac;
    }
    else
    {
                                                                       
      selec = (1.0 - empty_frac) * hist_selec;
    }
  }

                                      
  selec *= (1.0 - null_frac);

                                                   
  CLAMP_PROBABILITY(selec);

  return selec;
}

   
                                                                          
   
                                                                         
         
   
static double
calc_hist_selectivity(TypeCacheEntry *typcache, VariableStatData *vardata, RangeType *constval, Oid operator)
{
  AttStatsSlot hslot;
  AttStatsSlot lslot;
  int nhist;
  RangeBound *hist_lower;
  RangeBound *hist_upper;
  int i;
  RangeBound const_lower;
  RangeBound const_upper;
  bool empty;
  double hist_selec;

                                                                     
  if (!statistic_proc_security_check(vardata, typcache->rng_cmp_proc_finfo.fn_oid))
  {
    return -1;
  }
  if (OidIsValid(typcache->rng_subdiff_finfo.fn_oid) && !statistic_proc_security_check(vardata, typcache->rng_subdiff_finfo.fn_oid))
  {
    return -1;
  }

                                      
  if (!(HeapTupleIsValid(vardata->statsTuple) && get_attstatsslot(&hslot, vardata->statsTuple, STATISTIC_KIND_BOUNDS_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES)))
  {
    return -1.0;
  }

                                                           
  if (hslot.nvalues < 2)
  {
    free_attstatsslot(&hslot);
    return -1.0;
  }

     
                                                                        
             
     
  nhist = hslot.nvalues;
  hist_lower = (RangeBound *)palloc(sizeof(RangeBound) * nhist);
  hist_upper = (RangeBound *)palloc(sizeof(RangeBound) * nhist);
  for (i = 0; i < nhist; i++)
  {
    range_deserialize(typcache, DatumGetRangeTypeP(hslot.values[i]), &hist_lower[i], &hist_upper[i], &empty);
                                                           
    if (empty)
    {
      elog(ERROR, "bounds histogram contains an empty range");
    }
  }

                                                        
  if (operator== OID_RANGE_CONTAINS_OP || operator== OID_RANGE_CONTAINED_OP)
  {
    if (!(HeapTupleIsValid(vardata->statsTuple) && get_attstatsslot(&lslot, vardata->statsTuple, STATISTIC_KIND_RANGE_LENGTH_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES)))
    {
      free_attstatsslot(&hslot);
      return -1.0;
    }

                                                             
    if (lslot.nvalues < 2)
    {
      free_attstatsslot(&lslot);
      free_attstatsslot(&hslot);
      return -1.0;
    }
  }
  else
  {
    memset(&lslot, 0, sizeof(lslot));
  }

                                                 
  range_deserialize(typcache, constval, &const_lower, &const_upper, &empty);
  Assert(!empty);

     
                                                                     
                                                           
     
  switch (operator)
  {
  case OID_RANGE_LESS_OP:

       
                                                                      
                                                                    
                                                                       
                                                                  
                                                                   
                    
       
    hist_selec = calc_hist_selectivity_scalar(typcache, &const_lower, hist_lower, nhist, false);
    break;

  case OID_RANGE_LESS_EQUAL_OP:
    hist_selec = calc_hist_selectivity_scalar(typcache, &const_lower, hist_lower, nhist, true);
    break;

  case OID_RANGE_GREATER_OP:
    hist_selec = 1 - calc_hist_selectivity_scalar(typcache, &const_lower, hist_lower, nhist, false);
    break;

  case OID_RANGE_GREATER_EQUAL_OP:
    hist_selec = 1 - calc_hist_selectivity_scalar(typcache, &const_lower, hist_lower, nhist, true);
    break;

  case OID_RANGE_LEFT_OP:
                                                     
    hist_selec = calc_hist_selectivity_scalar(typcache, &const_lower, hist_upper, nhist, false);
    break;

  case OID_RANGE_RIGHT_OP:
                                                     
    hist_selec = 1 - calc_hist_selectivity_scalar(typcache, &const_upper, hist_lower, nhist, true);
    break;

  case OID_RANGE_OVERLAPS_RIGHT_OP:
                              
    hist_selec = 1 - calc_hist_selectivity_scalar(typcache, &const_lower, hist_lower, nhist, false);
    break;

  case OID_RANGE_OVERLAPS_LEFT_OP:
                              
    hist_selec = calc_hist_selectivity_scalar(typcache, &const_upper, hist_upper, nhist, true);
    break;

  case OID_RANGE_OVERLAP_OP:
  case OID_RANGE_CONTAINS_ELEM_OP:

       
                                          
       
                                                                    
                                                                      
           
       
                                                                    
                                                                      
                                                  
       
    hist_selec = calc_hist_selectivity_scalar(typcache, &const_lower, hist_upper, nhist, false);
    hist_selec += (1.0 - calc_hist_selectivity_scalar(typcache, &const_upper, hist_lower, nhist, true));
    hist_selec = 1.0 - hist_selec;
    break;

  case OID_RANGE_CONTAINS_OP:
    hist_selec = calc_hist_selectivity_contains(typcache, &const_lower, &const_upper, hist_lower, nhist, lslot.values, lslot.nvalues);
    break;

  case OID_RANGE_CONTAINED_OP:
    if (const_lower.infinite)
    {
         
                                                                   
                                                  
         
      hist_selec = calc_hist_selectivity_scalar(typcache, &const_upper, hist_upper, nhist, true);
    }
    else if (const_upper.infinite)
    {
      hist_selec = 1.0 - calc_hist_selectivity_scalar(typcache, &const_lower, hist_lower, nhist, false);
    }
    else
    {
      hist_selec = calc_hist_selectivity_contained(typcache, &const_lower, &const_upper, hist_lower, nhist, lslot.values, lslot.nvalues);
    }
    break;

  default:
    elog(ERROR, "unknown range operator %u", operator);
    hist_selec = -1.0;                          
    break;
  }

  free_attstatsslot(&lslot);
  free_attstatsslot(&hslot);

  return hist_selec;
}

   
                                                                           
                                                          
   
static double
calc_hist_selectivity_scalar(TypeCacheEntry *typcache, RangeBound *constbound, RangeBound *hist, int hist_nvalues, bool equal)
{
  Selectivity selec;
  int index;

     
                                                                    
                                                        
     
  index = rbound_bsearch(typcache, constbound, hist, hist_nvalues, equal);
  selec = (Selectivity)(Max(index, 0)) / (Selectivity)(hist_nvalues - 1);

                                                        
  if (index >= 0 && index < hist_nvalues - 1)
  {
    selec += get_position(typcache, constbound, &hist[index], &hist[index + 1]) / (Selectivity)(hist_nvalues - 1);
  }

  return selec;
}

   
                                                                              
                                                                              
                                                                               
                                                                        
   
                                                                            
                                                                  
                                                                                   
   
static int
rbound_bsearch(TypeCacheEntry *typcache, RangeBound *value, RangeBound *hist, int hist_length, bool equal)
{
  int lower = -1, upper = hist_length - 1, cmp, middle;

  while (lower < upper)
  {
    middle = (lower + upper + 1) / 2;
    cmp = range_cmp_bounds(typcache, &hist[middle], value);

    if (cmp < 0 || (equal && cmp == 0))
    {
      lower = middle;
    }
    else
    {
      upper = middle - 1;
    }
  }
  return lower;
}

   
                                                                                
                                                                                
                                                                             
                             
   
static int
length_hist_bsearch(Datum *length_hist_values, int length_hist_nvalues, double value, bool equal)
{
  int lower = -1, upper = length_hist_nvalues - 1, middle;

  while (lower < upper)
  {
    double middleval;

    middle = (lower + upper + 1) / 2;

    middleval = DatumGetFloat8(length_hist_values[middle]);
    if (middleval < value || (equal && middleval <= value))
    {
      lower = middle;
    }
    else
    {
      upper = middle - 1;
    }
  }
  return lower;
}

   
                                                                   
   
static float8
get_position(TypeCacheEntry *typcache, RangeBound *value, RangeBound *hist1, RangeBound *hist2)
{
  bool has_subdiff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);
  float8 position;

  if (!hist1->infinite && !hist2->infinite)
  {
    float8 bin_width;

       
                                                                          
                                                                    
                                                                        
            
       
    if (value->infinite)
    {
      return 0.5;
    }

                                                    
    if (!has_subdiff)
    {
      return 0.5;
    }

                                                             
    bin_width = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo, typcache->rng_collation, hist2->val, hist1->val));
    if (isnan(bin_width) || bin_width <= 0.0)
    {
      return 0.5;                                     
    }

    position = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo, typcache->rng_collation, value->val, hist1->val)) / bin_width;

    if (isnan(position))
    {
      return 0.5;                                              
    }

                                                  
    position = Max(position, 0.0);
    position = Min(position, 1.0);
    return position;
  }
  else if (hist1->infinite && !hist2->infinite)
  {
       
                                                                         
                                                                        
                                                                           
              
       
    return ((value->infinite && value->lower) ? 0.0 : 1.0);
  }
  else if (!hist1->infinite && hist2->infinite)
  {
                                       
    return ((value->infinite && !value->lower) ? 1.0 : 0.0);
  }
  else
  {
       
                                                                         
                                                                      
                                                                           
                                                      
       
                                                                     
       
    return 0.5;
  }
}

   
                                                                            
   
static double
get_len_position(double value, double hist1, double hist2)
{
  if (!isinf(hist1) && !isinf(hist2))
  {
       
                                                                          
                                                                     
                  
       
    if (isinf(value))
    {
      return 0.5;
    }

    return 1.0 - (hist2 - value) / (hist2 - hist1);
  }
  else if (isinf(hist1) && !isinf(hist2))
  {
       
                                                                       
                                                                  
       
    return 1.0;
  }
  else if (isinf(hist1) && isinf(hist2))
  {
                                       
    return 0.0;
  }
  else
  {
       
                                                                         
                                                                      
                                                                          
                             
       
                                                                     
       
    return 0.5;
  }
}

   
                                              
   
static float8
get_distance(TypeCacheEntry *typcache, RangeBound *bound1, RangeBound *bound2)
{
  bool has_subdiff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);

  if (!bound1->infinite && !bound2->infinite)
  {
       
                                                                         
                                                
       
    if (has_subdiff)
    {
      float8 res;

      res = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo, typcache->rng_collation, bound2->val, bound1->val));
                                                            
      if (isnan(res) || res < 0.0)
      {
        return 1.0;
      }
      else
      {
        return res;
      }
    }
    else
    {
      return 1.0;
    }
  }
  else if (bound1->infinite && bound2->infinite)
  {
                                  
    if (bound1->lower == bound2->lower)
    {
      return 0.0;
    }
    else
    {
      return get_float8_infinity();
    }
  }
  else
  {
                                                 
    return get_float8_infinity();
  }
}

   
                                                                               
                                                                           
                     
   
static double
calc_length_hist_frac(Datum *length_hist_values, int length_hist_nvalues, double length1, double length2, bool equal)
{
  double frac;
  double A, B, PA, PB;
  double pos;
  int i;
  double area;

  Assert(length2 >= length1);

  if (length2 < 0.0)
  {
    return 0.0;                                                  
  }

                                                 
  if (isinf(length2) && equal)
  {
    return 1.0;
  }

               
                                                                        
              
     
         
            
                      
             
         
     
                                                                          
                                                                          
                                                                             
                                    
     
             
           
           
               
              
              
                
               
     
                                                                            
                                                                        
     
                                                                     
     
                                                                        
                                                                          
                                                                           
                                                                          
                                                                     
                                              
                                               
     

                                                    
  i = length_hist_bsearch(length_hist_values, length_hist_nvalues, length1, equal);
  if (i >= length_hist_nvalues - 1)
  {
    return 1.0;
  }

  if (i < 0)
  {
    i = 0;
    pos = 0.0;
  }
  else
  {
                                                   
    pos = get_len_position(length1, DatumGetFloat8(length_hist_values[i]), DatumGetFloat8(length_hist_values[i + 1]));
  }
  PB = (((double)i) + pos) / (double)(length_hist_nvalues - 1);
  B = length1;

     
                                                                   
                                                                            
                                   
     
  if (length2 == length1)
  {
    return PB;
  }

     
                                                                        
                                                                          
                                      
     
  area = 0.0;
  for (; i < length_hist_nvalues - 1; i++)
  {
    double bin_upper = DatumGetFloat8(length_hist_values[i + 1]);

                                             
    if (!(bin_upper < length2 || (equal && bin_upper <= length2)))
    {
      break;
    }

                                                                        
    A = B;
    PA = PB;

    B = bin_upper;
    PB = (double)i / (double)(length_hist_nvalues - 1);

       
                                                                     
                                                                        
                                                                          
                                                    
       
    if (PA > 0 || PB > 0)
    {
      area += 0.5 * (PB + PA) * (B - A);
    }
  }

                
  A = B;
  PA = PB;

  B = length2;                                             
  if (i >= length_hist_nvalues - 1)
  {
    pos = 0.0;
  }
  else
  {
    if (DatumGetFloat8(length_hist_values[i]) == DatumGetFloat8(length_hist_values[i + 1]))
    {
      pos = 0.0;
    }
    else
    {
      pos = get_len_position(length2, DatumGetFloat8(length_hist_values[i]), DatumGetFloat8(length_hist_values[i + 1]));
    }
  }
  PB = (((double)i) + pos) / (double)(length_hist_nvalues - 1);

  if (PA > 0 || PB > 0)
  {
    area += 0.5 * (PB + PA) * (B - A);
  }

     
                                                                           
                                
     
                                                                          
                                                                            
                                                   
     
  if (isinf(area) && isinf(length2))
  {
    frac = 0.5;
  }
  else
  {
    frac = area / (length2 - length1);
  }

  return frac;
}

   
                                                                               
                                                                             
                                                                             
                                                               
   
                                                                           
           
   
static double
calc_hist_selectivity_contained(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, RangeBound *hist_lower, int hist_nvalues, Datum *length_hist_values, int length_hist_nvalues)
{
  int i, upper_index;
  float8 prev_dist;
  double bin_width;
  double upper_bin_width;
  double sum_frac;

     
                                                                             
                                                                          
                                                                       
     
  upper->inclusive = !upper->inclusive;
  upper->lower = true;
  upper_index = rbound_bsearch(typcache, upper, hist_lower, hist_nvalues, false);

     
                                                                          
                     
     
  if (upper_index < 0)
  {
    return 0.0;
  }

     
                                                                           
                                                                           
                                                                        
                                                                      
                                                                             
                       
     
  upper_index = Min(upper_index, hist_nvalues - 2);

     
                                                                      
                                                                           
                                                     
     
  upper_bin_width = get_position(typcache, upper, &hist_lower[upper_index], &hist_lower[upper_index + 1]);

     
                                                                             
                                                           
     
                                                                            
                                                                          
                                                                             
                                                     
     
  prev_dist = 0.0;
  bin_width = upper_bin_width;

  sum_frac = 0.0;
  for (i = upper_index; i >= 0; i--)
  {
    double dist;
    double length_hist_frac;
    bool final_bin = false;

       
                                                                          
                                                                           
                                                                       
                             
       
    if (range_cmp_bounds(typcache, &hist_lower[i], lower) < 0)
    {
      dist = get_distance(typcache, lower, upper);

         
                                                                         
                 
         
      bin_width -= get_position(typcache, lower, &hist_lower[i], &hist_lower[i + 1]);
      if (bin_width < 0.0)
      {
        bin_width = 0.0;
      }
      final_bin = true;
    }
    else
    {
      dist = get_distance(typcache, &hist_lower[i], upper);
    }

       
                                                                          
                                                                         
       
    length_hist_frac = calc_length_hist_frac(length_hist_values, length_hist_nvalues, prev_dist, dist, true);

       
                                                                          
                  
       
    sum_frac += length_hist_frac * bin_width / (double)(hist_nvalues - 1);

    if (final_bin)
    {
      break;
    }

    bin_width = 1.0;
    prev_dist = dist;
  }

  return sum_frac;
}

   
                                                                               
                                                                         
                                                                             
                                                               
   
static double
calc_hist_selectivity_contains(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, RangeBound *hist_lower, int hist_nvalues, Datum *length_hist_values, int length_hist_nvalues)
{
  int i, lower_index;
  double bin_width, lower_bin_width;
  double sum_frac;
  float8 prev_dist;

                                                               
  lower_index = rbound_bsearch(typcache, lower, hist_lower, hist_nvalues, true);

     
                                                                          
                     
     
  if (lower_index < 0)
  {
    return 0.0;
  }

     
                                                                           
                                                                           
                                                                        
                                                                      
                                                                             
                       
     
  lower_index = Min(lower_index, hist_nvalues - 2);

     
                                                                         
                                                                           
                                                     
     
  lower_bin_width = get_position(typcache, lower, &hist_lower[lower_index], &hist_lower[lower_index + 1]);

     
                                                                         
                                                                    
                                                                           
                                                                             
                                                                         
                                                   
     
                                                                            
                                                                       
                                             
     
  prev_dist = get_distance(typcache, lower, upper);
  sum_frac = 0.0;
  bin_width = lower_bin_width;
  for (i = lower_index; i >= 0; i--)
  {
    float8 dist;
    double length_hist_frac;

       
                                                                         
                                                                        
                  
       
    dist = get_distance(typcache, &hist_lower[i], upper);

       
                                                                       
                                                                         
       
    length_hist_frac = 1.0 - calc_length_hist_frac(length_hist_values, length_hist_nvalues, prev_dist, dist, false);

    sum_frac += length_hist_frac * bin_width / (double)(hist_nvalues - 1);

    bin_width = 1.0;
    prev_dist = dist;
  }

  return sum_frac;
}
