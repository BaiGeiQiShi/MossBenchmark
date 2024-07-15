                                                                            
   
                     
                                   
   
                                                                         
                                                                        
   
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/gist.h"
#include "access/stratnum.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/datum.h"
#include "utils/rangetypes.h"

   
                                                                           
                                                                              
                                   
   
#define CLS_NORMAL 0                                                 
#define CLS_LOWER_INF 1                                  
#define CLS_UPPER_INF 2                                  
#define CLS_CONTAIN_EMPTY 4                                       
#define CLS_EMPTY 8                                             

#define CLS_COUNT                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  9                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                                              

   
                                                                              
                                                                              
              
   
#define LIMIT_RATIO 0.3

                                        
#define INFINITE_BOUND_PENALTY 2.0
#define CONTAIN_EMPTY_PENALTY 1.0
#define DEFAULT_SUBTYPE_DIFF_PENALTY 1.0

   
                                                      
   
typedef struct
{
  int index;
  RangeBound bound;
} SingleBoundSortItem;

                                           
typedef enum
{
  SPLIT_LEFT = 0,                                                
  SPLIT_RIGHT
} SplitLR;

   
                                          
   
typedef struct
{
  TypeCacheEntry *typcache;                              
  bool has_subtype_diff;                                    
  int entries_count;                                                 

                                                          

  bool first;                                        

  RangeBound *left_upper;                                    
  RangeBound *right_lower;                                    

  float4 ratio;                     
  float4 overlap;                                                
  int common_left;                                              
  int common_right;
} ConsiderSplitContext;

   
                                                       
                                    
   
typedef struct
{
  RangeBound lower;
  RangeBound upper;
} NonEmptyRange;

   
                                                                            
                                                                  
   
typedef struct
{
                                           
  int index;
                                                                  
  double delta;
} CommonEntry;

                                                                             
                                                                          
#define PLACE_LEFT(range, off)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (v->spl_nleft > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      left_range = range_super_union(typcache, left_range, range);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      left_range = (range);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    v->spl_left[v->spl_nleft++] = (off);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  } while (0)

#define PLACE_RIGHT(range, off)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (v->spl_nright > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      right_range = range_super_union(typcache, right_range, range);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      right_range = (range);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    v->spl_right[v->spl_nright++] = (off);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  } while (0)

                                                                          
#define rangeCopy(r) ((RangeType *)DatumGetPointer(datumCopy(PointerGetDatum(r), false, -1)))

static RangeType *
range_super_union(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2);
static bool
range_gist_consistent_int(TypeCacheEntry *typcache, StrategyNumber strategy, RangeType *key, Datum query);
static bool
range_gist_consistent_leaf(TypeCacheEntry *typcache, StrategyNumber strategy, RangeType *key, Datum query);
static void
range_gist_fallback_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v);
static void
range_gist_class_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v, SplitLR *classes_groups);
static void
range_gist_single_sorting_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v, bool use_upper_bound);
static void
range_gist_double_sorting_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v);
static void
range_gist_consider_split(ConsiderSplitContext *context, RangeBound *right_lower, int min_left_count, RangeBound *left_upper, int max_left_count);
static int
get_gist_range_class(RangeType *range);
static int
single_bound_cmp(const void *a, const void *b, void *arg);
static int
interval_cmp_lower(const void *a, const void *b, void *arg);
static int
interval_cmp_upper(const void *a, const void *b, void *arg);
static int
common_entry_cmp(const void *i1, const void *i2);
static float8
call_subtype_diff(TypeCacheEntry *typcache, Datum val1, Datum val2);

                                  
Datum
range_gist_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  Datum query = PG_GETARG_DATUM(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

                                       
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  RangeType *key = DatumGetRangeTypeP(entry->key);
  TypeCacheEntry *typcache;

                                                       
  *recheck = false;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(key));

  if (GIST_LEAF(entry))
  {
    PG_RETURN_BOOL(range_gist_consistent_leaf(typcache, strategy, key, query));
  }
  else
  {
    PG_RETURN_BOOL(range_gist_consistent_int(typcache, strategy, key, query));
  }
}

                      
Datum
range_gist_union(PG_FUNCTION_ARGS)
{
  GistEntryVector *entryvec = (GistEntryVector *)PG_GETARG_POINTER(0);
  GISTENTRY *ent = entryvec->vector;
  RangeType *result_range;
  TypeCacheEntry *typcache;
  int i;

  result_range = DatumGetRangeTypeP(ent[0].key);

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(result_range));

  for (i = 1; i < entryvec->n; i++)
  {
    result_range = range_super_union(typcache, result_range, DatumGetRangeTypeP(ent[i].key));
  }

  PG_RETURN_RANGE_P(result_range);
}

   
                                                                
                                                                        
                                                    
   

   
                                     
   
                                                                             
               
                                 
                                                          
                                                                             
                                                         
   
Datum
range_gist_penalty(PG_FUNCTION_ARGS)
{
  GISTENTRY *origentry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *newentry = (GISTENTRY *)PG_GETARG_POINTER(1);
  float *penalty = (float *)PG_GETARG_POINTER(2);
  RangeType *orig = DatumGetRangeTypeP(origentry->key);
  RangeType *new = DatumGetRangeTypeP(newentry->key);
  TypeCacheEntry *typcache;
  bool has_subtype_diff;
  RangeBound orig_lower, new_lower, orig_upper, new_upper;
  bool orig_empty, new_empty;

  if (RangeTypeGetOid(orig) != RangeTypeGetOid(new))
  {
    elog(ERROR, "range types do not match");
  }

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(orig));

  has_subtype_diff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);

  range_deserialize(typcache, orig, &orig_lower, &orig_upper, &orig_empty);
  range_deserialize(typcache, new, &new_lower, &new_upper, &new_empty);

     
                                                                           
                                                                          
                
     
  if (new_empty)
  {
                                         
    if (orig_empty)
    {
         
                                                                
                                                                    
                                            
         
      *penalty = 0.0;
    }
    else if (RangeIsOrContainsEmpty(orig))
    {
         
                                                                   
                                                                       
                                                                       
                             
         
      *penalty = CONTAIN_EMPTY_PENALTY;
    }
    else if (orig_lower.infinite && orig_upper.infinite)
    {
         
                                                                       
                                         
         
      *penalty = 2 * CONTAIN_EMPTY_PENALTY;
    }
    else if (orig_lower.infinite || orig_upper.infinite)
    {
         
                                                                     
                                                            
         
      *penalty = 3 * CONTAIN_EMPTY_PENALTY;
    }
    else
    {
         
                                                                 
         
      *penalty = 4 * CONTAIN_EMPTY_PENALTY;
    }
  }
  else if (new_lower.infinite && new_upper.infinite)
  {
                                                
    if (orig_lower.infinite && orig_upper.infinite)
    {
         
                                                                
         
      *penalty = 0.0;
    }
    else if (orig_lower.infinite || orig_upper.infinite)
    {
         
                                                                   
                                                                 
                    
         
      *penalty = INFINITE_BOUND_PENALTY;
    }
    else
    {
         
                                                                
         
      *penalty = 2 * INFINITE_BOUND_PENALTY;
    }

    if (RangeIsOrContainsEmpty(orig))
    {
         
                                                                  
                                                   
         
      *penalty += CONTAIN_EMPTY_PENALTY;
    }
  }
  else if (new_lower.infinite)
  {
                                             
    if (!orig_empty && orig_lower.infinite)
    {
      if (orig_upper.infinite)
      {
           
                                                                       
                                                                  
                                                                       
                                                                      
                                  
           
        *penalty = 0.0;
      }
      else
      {
        if (range_cmp_bounds(typcache, &new_upper, &orig_upper) > 0)
        {
             
                                                                     
                                                   
             
          if (has_subtype_diff)
          {
            *penalty = call_subtype_diff(typcache, new_upper.val, orig_upper.val);
          }
          else
          {
            *penalty = DEFAULT_SUBTYPE_DIFF_PENALTY;
          }
        }
        else
        {
                                              
          *penalty = 0.0;
        }
      }
    }
    else
    {
         
                                                                         
                         
         
      *penalty = get_float4_infinity();
    }
  }
  else if (new_upper.infinite)
  {
                                             
    if (!orig_empty && orig_upper.infinite)
    {
      if (orig_lower.infinite)
      {
           
                                                                    
                                                                     
                                                                       
                                                                      
                                  
           
        *penalty = 0.0;
      }
      else
      {
        if (range_cmp_bounds(typcache, &new_lower, &orig_lower) < 0)
        {
             
                                                                     
                                                   
             
          if (has_subtype_diff)
          {
            *penalty = call_subtype_diff(typcache, orig_lower.val, new_lower.val);
          }
          else
          {
            *penalty = DEFAULT_SUBTYPE_DIFF_PENALTY;
          }
        }
        else
        {
                                              
          *penalty = 0.0;
        }
      }
    }
    else
    {
         
                                                                         
                         
         
      *penalty = get_float4_infinity();
    }
  }
  else
  {
                                                                    
    if (orig_empty || orig_lower.infinite || orig_upper.infinite)
    {
         
                                                                    
         
      *penalty = get_float4_infinity();
    }
    else
    {
         
                                                                        
                                                   
         
      float8 diff = 0.0;

      if (range_cmp_bounds(typcache, &new_lower, &orig_lower) < 0)
      {
        if (has_subtype_diff)
        {
          diff += call_subtype_diff(typcache, orig_lower.val, new_lower.val);
        }
        else
        {
          diff += DEFAULT_SUBTYPE_DIFF_PENALTY;
        }
      }
      if (range_cmp_bounds(typcache, &new_upper, &orig_upper) > 0)
      {
        if (has_subtype_diff)
        {
          diff += call_subtype_diff(typcache, new_upper.val, orig_upper.val);
        }
        else
        {
          diff += DEFAULT_SUBTYPE_DIFF_PENALTY;
        }
      }
      *penalty = diff;
    }
  }

  PG_RETURN_POINTER(penalty);
}

   
                                        
   
                                                                             
                                                                              
   
Datum
range_gist_picksplit(PG_FUNCTION_ARGS)
{
  GistEntryVector *entryvec = (GistEntryVector *)PG_GETARG_POINTER(0);
  GIST_SPLITVEC *v = (GIST_SPLITVEC *)PG_GETARG_POINTER(1);
  TypeCacheEntry *typcache;
  OffsetNumber i;
  RangeType *pred_left;
  int nbytes;
  OffsetNumber maxoff;
  int count_in_classes[CLS_COUNT];
  int j;
  int non_empty_classes_count = 0;
  int biggest_class = -1;
  int biggest_class_count = 0;
  int total_count;

                                                   
  pred_left = DatumGetRangeTypeP(entryvec->vector[FirstOffsetNumber].key);
  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(pred_left));

  maxoff = entryvec->n - 1;
  nbytes = (maxoff + 1) * sizeof(OffsetNumber);
  v->spl_left = (OffsetNumber *)palloc(nbytes);
  v->spl_right = (OffsetNumber *)palloc(nbytes);

     
                                              
     
  memset(count_in_classes, 0, sizeof(count_in_classes));
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    RangeType *range = DatumGetRangeTypeP(entryvec->vector[i].key);

    count_in_classes[get_gist_range_class(range)]++;
  }

     
                                                     
     
  total_count = maxoff;
  for (j = 0; j < CLS_COUNT; j++)
  {
    if (count_in_classes[j] > 0)
    {
      if (count_in_classes[j] > biggest_class_count)
      {
        biggest_class_count = count_in_classes[j];
        biggest_class = j;
      }
      non_empty_classes_count++;
    }
  }

  Assert(non_empty_classes_count > 0);

  if (non_empty_classes_count == 1)
  {
                                                    
    if ((biggest_class & ~CLS_CONTAIN_EMPTY) == CLS_NORMAL)
    {
                                                  
      range_gist_double_sorting_split(typcache, entryvec, v);
    }
    else if ((biggest_class & ~CLS_CONTAIN_EMPTY) == CLS_LOWER_INF)
    {
                                                          
      range_gist_single_sorting_split(typcache, entryvec, v, true);
    }
    else if ((biggest_class & ~CLS_CONTAIN_EMPTY) == CLS_UPPER_INF)
    {
                                                          
      range_gist_single_sorting_split(typcache, entryvec, v, false);
    }
    else
    {
                                                                  
      range_gist_fallback_split(typcache, entryvec, v);
    }
  }
  else
  {
       
                          
       
                                                                         
                                   
       
    SplitLR classes_groups[CLS_COUNT];

    memset(classes_groups, 0, sizeof(classes_groups));

    if (count_in_classes[CLS_NORMAL] > 0)
    {
                                         
      classes_groups[CLS_NORMAL] = SPLIT_RIGHT;
    }
    else
    {
                   
                                                  
                                                              
                                                    
         
                                                                         
                                                                        
                                                         
                   
         
      int infCount, nonInfCount;
      int emptyCount, nonEmptyCount;

      nonInfCount = count_in_classes[CLS_NORMAL] + count_in_classes[CLS_CONTAIN_EMPTY] + count_in_classes[CLS_EMPTY];
      infCount = total_count - nonInfCount;

      nonEmptyCount = count_in_classes[CLS_NORMAL] + count_in_classes[CLS_LOWER_INF] + count_in_classes[CLS_UPPER_INF] + count_in_classes[CLS_LOWER_INF | CLS_UPPER_INF];
      emptyCount = total_count - nonEmptyCount;

      if (infCount > 0 && nonInfCount > 0 && (Abs(infCount - nonInfCount) <= Abs(emptyCount - nonEmptyCount)))
      {
        classes_groups[CLS_NORMAL] = SPLIT_RIGHT;
        classes_groups[CLS_CONTAIN_EMPTY] = SPLIT_RIGHT;
        classes_groups[CLS_EMPTY] = SPLIT_RIGHT;
      }
      else if (emptyCount > 0 && nonEmptyCount > 0)
      {
        classes_groups[CLS_NORMAL] = SPLIT_RIGHT;
        classes_groups[CLS_LOWER_INF] = SPLIT_RIGHT;
        classes_groups[CLS_UPPER_INF] = SPLIT_RIGHT;
        classes_groups[CLS_LOWER_INF | CLS_UPPER_INF] = SPLIT_RIGHT;
      }
      else
      {
           
                                                              
                     
           
        classes_groups[biggest_class] = SPLIT_RIGHT;
      }
    }

    range_gist_class_split(typcache, entryvec, v, classes_groups);
  }

  PG_RETURN_POINTER(v);
}

                                  
Datum
range_gist_same(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  bool *result = (bool *)PG_GETARG_POINTER(2);

     
                                                                            
                                                                            
                                                                          
                                                
     
  if (range_get_flags(r1) != range_get_flags(r2))
  {
    *result = false;
  }
  else
  {
    TypeCacheEntry *typcache;

    typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

    *result = range_eq_internal(typcache, r1, r2);
  }

  PG_RETURN_POINTER(result);
}

   
                                                             
                    
                                                             
   

   
                                                     
   
                                                               
                                                                          
                                                 
                                                                         
                                                                       
                                                                        
   
static RangeType *
range_super_union(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeType *result;
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;
  char flags1, flags2;
  RangeBound *result_lower;
  RangeBound *result_upper;

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);
  flags1 = range_get_flags(r1);
  flags2 = range_get_flags(r2);

  if (empty1)
  {
                                                                   
    if (flags2 & (RANGE_EMPTY | RANGE_CONTAIN_EMPTY))
    {
      return r2;
    }
                                                               
    r2 = rangeCopy(r2);
    range_set_contain_empty(r2);
    return r2;
  }
  if (empty2)
  {
                                                                   
    if (flags1 & (RANGE_EMPTY | RANGE_CONTAIN_EMPTY))
    {
      return r1;
    }
                                                               
    r1 = rangeCopy(r1);
    range_set_contain_empty(r1);
    return r1;
  }

  if (range_cmp_bounds(typcache, &lower1, &lower2) <= 0)
  {
    result_lower = &lower1;
  }
  else
  {
    result_lower = &lower2;
  }

  if (range_cmp_bounds(typcache, &upper1, &upper2) >= 0)
  {
    result_upper = &upper1;
  }
  else
  {
    result_upper = &upper2;
  }

                                                      
  if (result_lower == &lower1 && result_upper == &upper1 && ((flags1 & RANGE_CONTAIN_EMPTY) || !(flags2 & RANGE_CONTAIN_EMPTY)))
  {
    return r1;
  }
  if (result_lower == &lower2 && result_upper == &upper2 && ((flags2 & RANGE_CONTAIN_EMPTY) || !(flags1 & RANGE_CONTAIN_EMPTY)))
  {
    return r2;
  }

  result = make_range(typcache, result_lower, result_upper, false);

  if ((flags1 & RANGE_CONTAIN_EMPTY) || (flags2 & RANGE_CONTAIN_EMPTY))
  {
    range_set_contain_empty(result);
  }

  return result;
}

   
                                                  
   
static bool
range_gist_consistent_int(TypeCacheEntry *typcache, StrategyNumber strategy, RangeType *key, Datum query)
{
  switch (strategy)
  {
  case RANGESTRAT_BEFORE:
    if (RangeIsEmpty(key) || RangeIsEmpty(DatumGetRangeTypeP(query)))
    {
      return false;
    }
    return (!range_overright_internal(typcache, key, DatumGetRangeTypeP(query)));
  case RANGESTRAT_OVERLEFT:
    if (RangeIsEmpty(key) || RangeIsEmpty(DatumGetRangeTypeP(query)))
    {
      return false;
    }
    return (!range_after_internal(typcache, key, DatumGetRangeTypeP(query)));
  case RANGESTRAT_OVERLAPS:
    return range_overlaps_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_OVERRIGHT:
    if (RangeIsEmpty(key) || RangeIsEmpty(DatumGetRangeTypeP(query)))
    {
      return false;
    }
    return (!range_before_internal(typcache, key, DatumGetRangeTypeP(query)));
  case RANGESTRAT_AFTER:
    if (RangeIsEmpty(key) || RangeIsEmpty(DatumGetRangeTypeP(query)))
    {
      return false;
    }
    return (!range_overleft_internal(typcache, key, DatumGetRangeTypeP(query)));
  case RANGESTRAT_ADJACENT:
    if (RangeIsEmpty(key) || RangeIsEmpty(DatumGetRangeTypeP(query)))
    {
      return false;
    }
    if (range_adjacent_internal(typcache, key, DatumGetRangeTypeP(query)))
    {
      return true;
    }
    return range_overlaps_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_CONTAINS:
    return range_contains_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_CONTAINED_BY:

       
                                                               
                                                                       
                                               
       
    if (RangeIsOrContainsEmpty(key))
    {
      return true;
    }
    return range_overlaps_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_CONTAINS_ELEM:
    return range_contains_elem_internal(typcache, key, query);
  case RANGESTRAT_EQ:

       
                                                                     
                                                                
       
    if (RangeIsEmpty(DatumGetRangeTypeP(query)))
    {
      return RangeIsOrContainsEmpty(key);
    }
    return range_contains_internal(typcache, key, DatumGetRangeTypeP(query));
  default:
    elog(ERROR, "unrecognized range strategy: %d", strategy);
    return false;                          
  }
}

   
                                              
   
static bool
range_gist_consistent_leaf(TypeCacheEntry *typcache, StrategyNumber strategy, RangeType *key, Datum query)
{
  switch (strategy)
  {
  case RANGESTRAT_BEFORE:
    return range_before_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_OVERLEFT:
    return range_overleft_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_OVERLAPS:
    return range_overlaps_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_OVERRIGHT:
    return range_overright_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_AFTER:
    return range_after_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_ADJACENT:
    return range_adjacent_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_CONTAINS:
    return range_contains_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_CONTAINED_BY:
    return range_contained_by_internal(typcache, key, DatumGetRangeTypeP(query));
  case RANGESTRAT_CONTAINS_ELEM:
    return range_contains_elem_internal(typcache, key, query);
  case RANGESTRAT_EQ:
    return range_eq_internal(typcache, key, DatumGetRangeTypeP(query));
  default:
    elog(ERROR, "unrecognized range strategy: %d", strategy);
    return false;                          
  }
}

   
                                                             
                                         
   
static void
range_gist_fallback_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v)
{
  RangeType *left_range = NULL;
  RangeType *right_range = NULL;
  OffsetNumber i, maxoff, split_idx;

  maxoff = entryvec->n - 1;
                                                               
  split_idx = (maxoff - FirstOffsetNumber) / 2 + FirstOffsetNumber;

  v->spl_nleft = 0;
  v->spl_nright = 0;
  for (i = FirstOffsetNumber; i <= maxoff; i++)
  {
    RangeType *range = DatumGetRangeTypeP(entryvec->vector[i].key);

    if (i < split_idx)
    {
      PLACE_LEFT(range, i);
    }
    else
    {
      PLACE_RIGHT(range, i);
    }
  }

  v->spl_ldatum = RangeTypePGetDatum(left_range);
  v->spl_rdatum = RangeTypePGetDatum(right_range);
}

   
                                     
   
                                                   
                                                                             
                                        
   
static void
range_gist_class_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v, SplitLR *classes_groups)
{
  RangeType *left_range = NULL;
  RangeType *right_range = NULL;
  OffsetNumber i, maxoff;

  maxoff = entryvec->n - 1;

  v->spl_nleft = 0;
  v->spl_nright = 0;
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    RangeType *range = DatumGetRangeTypeP(entryvec->vector[i].key);
    int class;

                            
    class = get_gist_range_class(range);

                                         
    if (classes_groups[class] == SPLIT_LEFT)
    {
      PLACE_LEFT(range, i);
    }
    else
    {
      Assert(classes_groups[class] == SPLIT_RIGHT);
      PLACE_RIGHT(range, i);
    }
  }

  v->spl_ldatum = RangeTypePGetDatum(left_range);
  v->spl_rdatum = RangeTypePGetDatum(right_range);
}

   
                                                                            
                                                                          
                                                                           
                      
   
static void
range_gist_single_sorting_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v, bool use_upper_bound)
{
  SingleBoundSortItem *sortItems;
  RangeType *left_range = NULL;
  RangeType *right_range = NULL;
  OffsetNumber i, maxoff, split_idx;

  maxoff = entryvec->n - 1;

  sortItems = (SingleBoundSortItem *)palloc(maxoff * sizeof(SingleBoundSortItem));

     
                                                  
     
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    RangeType *range = DatumGetRangeTypeP(entryvec->vector[i].key);
    RangeBound bound2;
    bool empty;

    sortItems[i - 1].index = i;
                                          
    if (use_upper_bound)
    {
      range_deserialize(typcache, range, &bound2, &sortItems[i - 1].bound, &empty);
    }
    else
    {
      range_deserialize(typcache, range, &sortItems[i - 1].bound, &bound2, &empty);
    }
    Assert(!empty);
  }

  qsort_arg(sortItems, maxoff, sizeof(SingleBoundSortItem), single_bound_cmp, typcache);

  split_idx = maxoff / 2;

  v->spl_nleft = 0;
  v->spl_nright = 0;

  for (i = 0; i < maxoff; i++)
  {
    int idx = sortItems[i].index;
    RangeType *range = DatumGetRangeTypeP(entryvec->vector[idx].key);

    if (i < split_idx)
    {
      PLACE_LEFT(range, idx);
    }
    else
    {
      PLACE_RIGHT(range, idx);
    }
  }

  v->spl_ldatum = RangeTypePGetDatum(left_range);
  v->spl_rdatum = RangeTypePGetDatum(right_range);
}

   
                                   
   
                                                                             
                                                                        
                                                                           
                                                                           
                                                                                
                                                                              
                                                                      
                                                                         
                           
   
                                                              
                                                       
                                                        
                                                                             
                        
   
                                                                          
                                                                               
                                                       
   
                    
                                                                     
               
                                                                           
   
static void
range_gist_double_sorting_split(TypeCacheEntry *typcache, GistEntryVector *entryvec, GIST_SPLITVEC *v)
{
  ConsiderSplitContext context;
  OffsetNumber i, maxoff;
  RangeType *range, *left_range = NULL, *right_range = NULL;
  int common_entries_count;
  NonEmptyRange *by_lower, *by_upper;
  CommonEntry *common_entries;
  int nentries, i1, i2;
  RangeBound *right_lower, *left_upper;

  memset(&context, 0, sizeof(ConsiderSplitContext));
  context.typcache = typcache;
  context.has_subtype_diff = OidIsValid(typcache->rng_subdiff_finfo.fn_oid);

  maxoff = entryvec->n - 1;
  nentries = context.entries_count = maxoff - FirstOffsetNumber + 1;
  context.first = true;

                                               
  by_lower = (NonEmptyRange *)palloc(nentries * sizeof(NonEmptyRange));
  by_upper = (NonEmptyRange *)palloc(nentries * sizeof(NonEmptyRange));

                             
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    RangeType *range = DatumGetRangeTypeP(entryvec->vector[i].key);
    bool empty;

    range_deserialize(typcache, range, &by_lower[i - FirstOffsetNumber].lower, &by_lower[i - FirstOffsetNumber].upper, &empty);
    Assert(!empty);
  }

     
                                                                            
                            
     
  memcpy(by_upper, by_lower, nentries * sizeof(NonEmptyRange));
  qsort_arg(by_lower, nentries, sizeof(NonEmptyRange), interval_cmp_lower, typcache);
  qsort_arg(by_upper, nentries, sizeof(NonEmptyRange), interval_cmp_upper, typcache);

               
                                                                     
                                                                    
     
                                                              
     
               
         
            
            
              
     
                                                                
                                                                      
                                                                   
                                                                 
                                                                     
                 
     
                                                                 
                      
                      
                      
     
                          
                      
                      
                      
               
     

     
                                                                        
                                
     
  i1 = 0;
  i2 = 0;
  right_lower = &by_lower[i1].lower;
  left_upper = &by_upper[i2].lower;
  while (true)
  {
       
                                             
       
    while (i1 < nentries && range_cmp_bounds(typcache, right_lower, &by_lower[i1].lower) == 0)
    {
      if (range_cmp_bounds(typcache, &by_lower[i1].upper, left_upper) > 0)
      {
        left_upper = &by_lower[i1].upper;
      }
      i1++;
    }
    if (i1 >= nentries)
    {
      break;
    }
    right_lower = &by_lower[i1].lower;

       
                                                                      
              
       
    while (i2 < nentries && range_cmp_bounds(typcache, &by_upper[i2].upper, left_upper) <= 0)
    {
      i2++;
    }

       
                                                                    
       
    range_gist_consider_split(&context, right_lower, i1, left_upper, i2);
  }

     
                                                                            
                           
     
  i1 = nentries - 1;
  i2 = nentries - 1;
  right_lower = &by_lower[i1].upper;
  left_upper = &by_upper[i2].upper;
  while (true)
  {
       
                                            
       
    while (i2 >= 0 && range_cmp_bounds(typcache, left_upper, &by_upper[i2].upper) == 0)
    {
      if (range_cmp_bounds(typcache, &by_upper[i2].lower, right_lower) < 0)
      {
        right_lower = &by_upper[i2].lower;
      }
      i2--;
    }
    if (i2 < 0)
    {
      break;
    }
    left_upper = &by_upper[i2].upper;

       
                                                                          
              
       
    while (i1 >= 0 && range_cmp_bounds(typcache, &by_lower[i1].lower, right_lower) >= 0)
    {
      i1--;
    }

       
                                                                    
       
    range_gist_consider_split(&context, right_lower, i1 + 1, left_upper, i2 + 1);
  }

     
                                                                    
     
  if (context.first)
  {
    range_gist_fallback_split(typcache, entryvec, v);
    return;
  }

     
                                                                   
                                                                             
                                                                    
     

                                    
  v->spl_left = (OffsetNumber *)palloc(nentries * sizeof(OffsetNumber));
  v->spl_right = (OffsetNumber *)palloc(nentries * sizeof(OffsetNumber));
  v->spl_nleft = 0;
  v->spl_nright = 0;

     
                                                                             
                                                                 
     
  common_entries_count = 0;
  common_entries = (CommonEntry *)palloc(nentries * sizeof(CommonEntry));

     
                                                                            
                     
     
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    RangeBound lower, upper;
    bool empty;

       
                                                       
       
    range = DatumGetRangeTypeP(entryvec->vector[i].key);

    range_deserialize(typcache, range, &lower, &upper, &empty);

    if (range_cmp_bounds(typcache, &upper, context.left_upper) <= 0)
    {
                                  
      if (range_cmp_bounds(typcache, &lower, context.right_lower) >= 0)
      {
                                                             
        common_entries[common_entries_count].index = i;
        if (context.has_subtype_diff)
        {
             
                                                     
                                          
             
          common_entries[common_entries_count].delta = call_subtype_diff(typcache, lower.val, context.right_lower->val) - call_subtype_diff(typcache, context.left_upper->val, upper.val);
        }
        else
        {
                                                             
          common_entries[common_entries_count].delta = 0;
        }
        common_entries_count++;
      }
      else
      {
                                                                       
        PLACE_LEFT(range, i);
      }
    }
    else
    {
         
                                                                         
                                                                        
                
         
      Assert(range_cmp_bounds(typcache, &lower, context.right_lower) >= 0);
      PLACE_RIGHT(range, i);
    }
  }

     
                                          
     
  if (common_entries_count > 0)
  {
       
                                                                         
                                         
       
    qsort(common_entries, common_entries_count, sizeof(CommonEntry), common_entry_cmp);

       
                                                                        
       
    for (i = 0; i < common_entries_count; i++)
    {
      int idx = common_entries[i].index;

      range = DatumGetRangeTypeP(entryvec->vector[idx].key);

         
                                                                         
                      
         
      if (i < context.common_left)
      {
        PLACE_LEFT(range, idx);
      }
      else
      {
        PLACE_RIGHT(range, idx);
      }
    }
  }

  v->spl_ldatum = PointerGetDatum(left_range);
  v->spl_rdatum = PointerGetDatum(right_range);
}

   
                                                                      
                                           
   
static void
range_gist_consider_split(ConsiderSplitContext *context, RangeBound *right_lower, int min_left_count, RangeBound *left_upper, int max_left_count)
{
  int left_count, right_count;
  float4 ratio, overlap;

     
                                                                             
                        
     
  if (min_left_count >= (context->entries_count + 1) / 2)
  {
    left_count = min_left_count;
  }
  else if (max_left_count <= context->entries_count / 2)
  {
    left_count = max_left_count;
  }
  else
  {
    left_count = context->entries_count / 2;
  }
  right_count = context->entries_count - left_count;

     
                                                                      
                                                                        
                                                          
     
  ratio = ((float4)Min(left_count, right_count)) / ((float4)context->entries_count);

  if (ratio > LIMIT_RATIO)
  {
    bool selectthis = false;

       
                                                                         
                                                                      
                                                                  
                                                                          
                                                             
       
    if (context->has_subtype_diff)
    {
      overlap = call_subtype_diff(context->typcache, left_upper->val, right_lower->val);
    }
    else
    {
      overlap = max_left_count - min_left_count;
    }

                                                              
    if (context->first)
    {
      selectthis = true;
    }
    else
    {
         
                                                                   
                                   
         
      if (overlap < context->overlap || (overlap == context->overlap && ratio > context->ratio))
      {
        selectthis = true;
      }
    }

    if (selectthis)
    {
                                                 
      context->first = false;
      context->ratio = ratio;
      context->overlap = overlap;
      context->right_lower = right_lower;
      context->left_upper = left_upper;
      context->common_left = max_left_count - left_count;
      context->common_right = left_count - min_left_count;
    }
  }
}

   
                                
   
                                                                    
                                                                     
                                         
   
static int
get_gist_range_class(RangeType *range)
{
  int classNumber;
  char flags;

  flags = range_get_flags(range);
  if (flags & RANGE_EMPTY)
  {
    classNumber = CLS_EMPTY;
  }
  else
  {
    classNumber = 0;
    if (flags & RANGE_LB_INF)
    {
      classNumber |= CLS_LOWER_INF;
    }
    if (flags & RANGE_UB_INF)
    {
      classNumber |= CLS_UPPER_INF;
    }
    if (flags & RANGE_CONTAIN_EMPTY)
    {
      classNumber |= CLS_CONTAIN_EMPTY;
    }
  }
  return classNumber;
}

   
                                                            
   
static int
single_bound_cmp(const void *a, const void *b, void *arg)
{
  SingleBoundSortItem *i1 = (SingleBoundSortItem *)a;
  SingleBoundSortItem *i2 = (SingleBoundSortItem *)b;
  TypeCacheEntry *typcache = (TypeCacheEntry *)arg;

  return range_cmp_bounds(typcache, &i1->bound, &i2->bound);
}

   
                                          
   
static int
interval_cmp_lower(const void *a, const void *b, void *arg)
{
  NonEmptyRange *i1 = (NonEmptyRange *)a;
  NonEmptyRange *i2 = (NonEmptyRange *)b;
  TypeCacheEntry *typcache = (TypeCacheEntry *)arg;

  return range_cmp_bounds(typcache, &i1->lower, &i2->lower);
}

   
                                          
   
static int
interval_cmp_upper(const void *a, const void *b, void *arg)
{
  NonEmptyRange *i1 = (NonEmptyRange *)a;
  NonEmptyRange *i2 = (NonEmptyRange *)b;
  TypeCacheEntry *typcache = (TypeCacheEntry *)arg;

  return range_cmp_bounds(typcache, &i1->upper, &i2->upper);
}

   
                                         
   
static int
common_entry_cmp(const void *i1, const void *i2)
{
  double delta1 = ((CommonEntry *)i1)->delta;
  double delta2 = ((CommonEntry *)i2)->delta;

  if (delta1 < delta2)
  {
    return -1;
  }
  else if (delta1 > delta2)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

   
                                                                       
                                                                          
   
static float8
call_subtype_diff(TypeCacheEntry *typcache, Datum val1, Datum val2)
{
  float8 value;

  value = DatumGetFloat8(FunctionCall2Coll(&typcache->rng_subdiff_finfo, typcache->rng_collation, val1, val2));
                                                               
  if (value >= 0.0)
  {
    return value;
  }
  return 0.0;
}
