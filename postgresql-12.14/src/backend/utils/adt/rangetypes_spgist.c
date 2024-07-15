                                                                            
   
                       
                                                                              
   
                                                                             
                                                                             
                                                                             
               
   
                                                                            
                                                                               
                                                                 
   
                                                                               
                                                                              
                                                                          
                                                                           
                                                                                
                                                                               
                                                                             
                                                 
   
                                                                                
                                                                              
                                                    
   
                                                                         
                                                                        
   
                  
                                               
   
                                                                            
   

#include "postgres.h"

#include "access/spgist.h"
#include "access/stratnum.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/rangetypes.h"

static int16
getQuadrant(TypeCacheEntry *typcache, RangeType *centroid, RangeType *tst);
static int
bound_cmp(const void *a, const void *b, void *arg);

static int
adjacent_inner_consistent(TypeCacheEntry *typcache, RangeBound *arg, RangeBound *centroid, RangeBound *prev);
static int
adjacent_cmp_bounds(TypeCacheEntry *typcache, RangeBound *arg, RangeBound *centroid);

   
                                        
   
Datum
spg_range_quad_config(PG_FUNCTION_ARGS)
{
                                                                  
  spgConfigOut *cfg = (spgConfigOut *)PG_GETARG_POINTER(1);

  cfg->prefixType = ANYRANGEOID;
  cfg->labelType = VOIDOID;                                
  cfg->canReturnData = true;
  cfg->longValuesOK = false;
  PG_RETURN_VOID();
}

             
                                                                          
             
   
                                     
   
           
             
           
   
                                                                             
                  
   
                                                                                
                                                                              
                                                                              
                                                                           
               
   
                                                            
             
   
static int16
getQuadrant(TypeCacheEntry *typcache, RangeType *centroid, RangeType *tst)
{
  RangeBound centroidLower, centroidUpper;
  bool centroidEmpty;
  RangeBound lower, upper;
  bool empty;

  range_deserialize(typcache, centroid, &centroidLower, &centroidUpper, &centroidEmpty);
  range_deserialize(typcache, tst, &lower, &upper, &empty);

  if (empty)
  {
    return 5;
  }

  if (range_cmp_bounds(typcache, &lower, &centroidLower) >= 0)
  {
    if (range_cmp_bounds(typcache, &upper, &centroidUpper) >= 0)
    {
      return 1;
    }
    else
    {
      return 2;
    }
  }
  else
  {
    if (range_cmp_bounds(typcache, &upper, &centroidUpper) >= 0)
    {
      return 4;
    }
    else
    {
      return 3;
    }
  }
}

   
                                                                   
   
Datum
spg_range_quad_choose(PG_FUNCTION_ARGS)
{
  spgChooseIn *in = (spgChooseIn *)PG_GETARG_POINTER(0);
  spgChooseOut *out = (spgChooseOut *)PG_GETARG_POINTER(1);
  RangeType *inRange = DatumGetRangeTypeP(in->datum), *centroid;
  int16 quadrant;
  TypeCacheEntry *typcache;

  if (in->allTheSame)
  {
    out->resultType = spgMatchNode;
                                   
    out->result.matchNode.levelAdd = 0;
    out->result.matchNode.restDatum = RangeTypePGetDatum(inRange);
    PG_RETURN_VOID();
  }

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(inRange));

     
                                                                            
                                                                             
             
     
  if (!in->hasPrefix)
  {
    out->resultType = spgMatchNode;
    if (RangeIsEmpty(inRange))
    {
      out->result.matchNode.nodeN = 0;
    }
    else
    {
      out->result.matchNode.nodeN = 1;
    }
    out->result.matchNode.levelAdd = 1;
    out->result.matchNode.restDatum = RangeTypePGetDatum(inRange);
    PG_RETURN_VOID();
  }

  centroid = DatumGetRangeTypeP(in->prefixDatum);
  quadrant = getQuadrant(typcache, centroid, inRange);

  Assert(quadrant <= in->nNodes);

                                               
  out->resultType = spgMatchNode;
  out->result.matchNode.nodeN = quadrant - 1;
  out->result.matchNode.levelAdd = 1;
  out->result.matchNode.restDatum = RangeTypePGetDatum(inRange);

  PG_RETURN_VOID();
}

   
                                 
   
static int
bound_cmp(const void *a, const void *b, void *arg)
{
  RangeBound *ba = (RangeBound *)a;
  RangeBound *bb = (RangeBound *)b;
  TypeCacheEntry *typcache = (TypeCacheEntry *)arg;

  return range_cmp_bounds(typcache, ba, bb);
}

   
                                                                          
                                                       
   
Datum
spg_range_quad_picksplit(PG_FUNCTION_ARGS)
{
  spgPickSplitIn *in = (spgPickSplitIn *)PG_GETARG_POINTER(0);
  spgPickSplitOut *out = (spgPickSplitOut *)PG_GETARG_POINTER(1);
  int i;
  int j;
  int nonEmptyCount;
  RangeType *centroid;
  bool empty;
  TypeCacheEntry *typcache;

                                                                             
  RangeBound *lowerBounds, *upperBounds;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(DatumGetRangeTypeP(in->datums[0])));

                                  
  lowerBounds = palloc(sizeof(RangeBound) * in->nTuples);
  upperBounds = palloc(sizeof(RangeBound) * in->nTuples);
  j = 0;

                                                            
  for (i = 0; i < in->nTuples; i++)
  {
    range_deserialize(typcache, DatumGetRangeTypeP(in->datums[i]), &lowerBounds[j], &upperBounds[j], &empty);
    if (!empty)
    {
      j++;
    }
  }
  nonEmptyCount = j;

     
                                                                           
                                                                         
                                                            
     
  if (nonEmptyCount == 0)
  {
    out->nNodes = 2;
    out->hasPrefix = false;
                         
    out->prefixDatum = PointerGetDatum(NULL);
    out->nodeLabels = NULL;

    out->mapTuplesToNodes = palloc(sizeof(int) * in->nTuples);
    out->leafTupleDatums = palloc(sizeof(Datum) * in->nTuples);

                                      
    for (i = 0; i < in->nTuples; i++)
    {
      RangeType *range = DatumGetRangeTypeP(in->datums[i]);

      out->leafTupleDatums[i] = RangeTypePGetDatum(range);
      out->mapTuplesToNodes[i] = 0;
    }
    PG_RETURN_VOID();
  }

                                                  
  qsort_arg(lowerBounds, nonEmptyCount, sizeof(RangeBound), bound_cmp, typcache);
  qsort_arg(upperBounds, nonEmptyCount, sizeof(RangeBound), bound_cmp, typcache);

                                                                         
  centroid = range_serialize(typcache, &lowerBounds[nonEmptyCount / 2], &upperBounds[nonEmptyCount / 2], false);
  out->hasPrefix = true;
  out->prefixDatum = RangeTypePGetDatum(centroid);

                                                              
  out->nNodes = (in->level == 0) ? 5 : 4;
  out->nodeLabels = NULL;                                

  out->mapTuplesToNodes = palloc(sizeof(int) * in->nTuples);
  out->leafTupleDatums = palloc(sizeof(Datum) * in->nTuples);

     
                                                                             
                       
     
  for (i = 0; i < in->nTuples; i++)
  {
    RangeType *range = DatumGetRangeTypeP(in->datums[i]);
    int16 quadrant = getQuadrant(typcache, centroid, range);

    out->leafTupleDatums[i] = RangeTypePGetDatum(range);
    out->mapTuplesToNodes[i] = quadrant - 1;
  }

  PG_RETURN_VOID();
}

   
                                                                      
                                         
   
Datum
spg_range_quad_inner_consistent(PG_FUNCTION_ARGS)
{
  spgInnerConsistentIn *in = (spgInnerConsistentIn *)PG_GETARG_POINTER(0);
  spgInnerConsistentOut *out = (spgInnerConsistentOut *)PG_GETARG_POINTER(1);
  int which;
  int i;
  MemoryContext oldCtx;

     
                                                                            
                                                                           
                                                        
     
  bool needPrevious = false;

  if (in->allTheSame)
  {
                                                 
    out->nNodes = in->nNodes;
    out->nodeNumbers = (int *)palloc(sizeof(int) * in->nNodes);
    for (i = 0; i < in->nNodes; i++)
    {
      out->nodeNumbers[i] = i;
    }
    PG_RETURN_VOID();
  }

  if (!in->hasPrefix)
  {
       
                                                                        
                                                                      
       
    Assert(in->nNodes == 2);

       
                                                                     
                                                                          
                              
       
    which = (1 << 1) | (1 << 2);
    for (i = 0; i < in->nkeys; i++)
    {
      StrategyNumber strategy = in->scankeys[i].sk_strategy;
      bool empty;

         
                                                                         
                                      
         
      if (strategy != RANGESTRAT_CONTAINS_ELEM)
      {
        empty = RangeIsEmpty(DatumGetRangeTypeP(in->scankeys[i].sk_argument));
      }
      else
      {
        empty = false;
      }

      switch (strategy)
      {
      case RANGESTRAT_BEFORE:
      case RANGESTRAT_OVERLEFT:
      case RANGESTRAT_OVERLAPS:
      case RANGESTRAT_OVERRIGHT:
      case RANGESTRAT_AFTER:
      case RANGESTRAT_ADJACENT:
                                                                    
        if (empty)
        {
          which = 0;
        }
        else
        {
          which &= (1 << 2);
        }
        break;

      case RANGESTRAT_CONTAINS:

           
                                                             
                                                 
           
        if (!empty)
        {
          which &= (1 << 2);
        }
        break;

      case RANGESTRAT_CONTAINED_BY:

           
                                                               
                                                                 
                            
           
        if (empty)
        {
          which &= (1 << 1);
        }
        break;

      case RANGESTRAT_CONTAINS_ELEM:
        which &= (1 << 2);
        break;

      case RANGESTRAT_EQ:
        if (empty)
        {
          which &= (1 << 1);
        }
        else
        {
          which &= (1 << 2);
        }
        break;

      default:
        elog(ERROR, "unrecognized range strategy: %d", strategy);
        break;
      }
      if (which == 0)
      {
        break;                                               
      }
    }
  }
  else
  {
    RangeBound centroidLower, centroidUpper;
    bool centroidEmpty;
    TypeCacheEntry *typcache;
    RangeType *centroid;

                                             
    centroid = DatumGetRangeTypeP(in->prefixDatum);
    typcache = range_get_typcache(fcinfo, RangeTypeGetOid(DatumGetRangeTypeP(centroid)));
    range_deserialize(typcache, centroid, &centroidLower, &centroidUpper, &centroidEmpty);

    Assert(in->nNodes == 4 || in->nNodes == 5);

       
                                                                          
                                                                          
                                     
       
    which = (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5);

    for (i = 0; i < in->nkeys; i++)
    {
      StrategyNumber strategy;
      RangeBound lower, upper;
      bool empty;
      RangeType *range = NULL;

      RangeType *prevCentroid = NULL;
      RangeBound prevLower, prevUpper;
      bool prevEmpty;

                                                                   
      RangeBound *minLower = NULL, *maxLower = NULL, *minUpper = NULL, *maxUpper = NULL;

                                                           
      bool inclusive = true;
      bool strictEmpty = true;
      int cmp, which1, which2;

      strategy = in->scankeys[i].sk_strategy;

         
                                                                        
                                                                        
                                                                
                              
         
      if (strategy == RANGESTRAT_CONTAINS_ELEM)
      {
        lower.inclusive = true;
        lower.infinite = false;
        lower.lower = true;
        lower.val = in->scankeys[i].sk_argument;

        upper.inclusive = true;
        upper.infinite = false;
        upper.lower = false;
        upper.val = in->scankeys[i].sk_argument;

        empty = false;

        strategy = RANGESTRAT_CONTAINS;
      }
      else
      {
        range = DatumGetRangeTypeP(in->scankeys[i].sk_argument);
        range_deserialize(typcache, range, &lower, &upper, &empty);
      }

         
                                                                        
                                                                
                                                                    
                                             
         
                                                                       
                                                                     
                                                                         
                
         
      switch (strategy)
      {
      case RANGESTRAT_BEFORE:

           
                                                                  
                                  
           
        maxUpper = &lower;
        inclusive = false;
        break;

      case RANGESTRAT_OVERLEFT:

           
                                                                 
                                                   
           
        maxUpper = &upper;
        break;

      case RANGESTRAT_OVERLAPS:

           
                                                                  
                                                                
           
        maxLower = &upper;
        minUpper = &lower;
        break;

      case RANGESTRAT_OVERRIGHT:

           
                                                                  
                                                      
           
        minLower = &lower;
        break;

      case RANGESTRAT_AFTER:

           
                                                                   
                                  
           
        minLower = &upper;
        inclusive = false;
        break;

      case RANGESTRAT_ADJACENT:
        if (empty)
        {
          break;                                 
        }

           
                                                                  
                                                                 
                                                                 
           
        if (in->traversalValue)
        {
          prevCentroid = DatumGetRangeTypeP(in->traversalValue);
          range_deserialize(typcache, prevCentroid, &prevLower, &prevUpper, &prevEmpty);
        }

           
                                                           
                                                                   
                                                                   
                                                              
                                                                
                                                              
                                                               
           
        cmp = adjacent_inner_consistent(typcache, &lower, &centroidUpper, prevCentroid ? &prevUpper : NULL);
        if (cmp > 0)
        {
          which1 = (1 << 1) | (1 << 4);
        }
        else if (cmp < 0)
        {
          which1 = (1 << 2) | (1 << 3);
        }
        else
        {
          which1 = 0;
        }

           
                                                                 
                                                                
                                                                   
                              
           
        cmp = adjacent_inner_consistent(typcache, &upper, &centroidLower, prevCentroid ? &prevLower : NULL);
        if (cmp > 0)
        {
          which2 = (1 << 1) | (1 << 2);
        }
        else if (cmp < 0)
        {
          which2 = (1 << 3) | (1 << 4);
        }
        else
        {
          which2 = 0;
        }

                                                                 
        which &= which1 | which2;

        needPrevious = true;
        break;

      case RANGESTRAT_CONTAINS:

           
                                                                 
                                                                  
                                                                        
                             
           
                                                        
           
        strictEmpty = false;
        if (!empty)
        {
          which &= (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
          maxLower = &lower;
          minUpper = &upper;
        }
        break;

      case RANGESTRAT_CONTAINED_BY:
                                       
        strictEmpty = false;
        if (empty)
        {
                                                                  
          which &= (1 << 5);
        }
        else
        {
          minLower = &lower;
          maxUpper = &upper;
        }
        break;

      case RANGESTRAT_EQ:

           
                                                              
                                        
           
        strictEmpty = false;
        which &= (1 << getQuadrant(typcache, centroid, range));
        break;

      default:
        elog(ERROR, "unrecognized range strategy: %d", strategy);
        break;
      }

      if (strictEmpty)
      {
        if (empty)
        {
                                                             
          which = 0;
          break;
        }
        else
        {
                                                             
          which &= (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
        }
      }

         
                                                                        
               
         
      if (minLower)
      {
           
                                                                      
                                                                      
                                                                 
                  
           
        if (range_cmp_bounds(typcache, &centroidLower, minLower) <= 0)
        {
          which &= (1 << 1) | (1 << 2) | (1 << 5);
        }
      }
      if (maxLower)
      {
           
                                                                     
                                                                   
                                                                   
                                                                      
                                                                     
                                                                   
                             
           
        int cmp;

        cmp = range_cmp_bounds(typcache, &centroidLower, maxLower);
        if (cmp > 0 || (!inclusive && cmp == 0))
        {
          which &= (1 << 3) | (1 << 4) | (1 << 5);
        }
      }
      if (minUpper)
      {
           
                                                                      
                                                                      
                                                                 
                  
           
        if (range_cmp_bounds(typcache, &centroidUpper, minUpper) <= 0)
        {
          which &= (1 << 1) | (1 << 4) | (1 << 5);
        }
      }
      if (maxUpper)
      {
           
                                                                     
                                                                   
                                                                   
                                                                      
                                                                     
                                                                   
                             
           
        int cmp;

        cmp = range_cmp_bounds(typcache, &centroidUpper, maxUpper);
        if (cmp > 0 || (!inclusive && cmp == 0))
        {
          which &= (1 << 2) | (1 << 3) | (1 << 5);
        }
      }

      if (which == 0)
      {
        break;                                               
      }
    }
  }

                                                                  
  out->nodeNumbers = (int *)palloc(sizeof(int) * in->nNodes);
  if (needPrevious)
  {
    out->traversalValues = (void **)palloc(sizeof(void *) * in->nNodes);
  }
  out->nNodes = 0;

     
                                                        
                            
     
  oldCtx = MemoryContextSwitchTo(in->traversalMemoryContext);

  for (i = 1; i <= in->nNodes; i++)
  {
    if (which & (1 << i))
    {
                                          
      if (needPrevious)
      {
        Datum previousCentroid;

           
                                                                   
                              
           
        previousCentroid = datumCopy(in->prefixDatum, false, -1);
        out->traversalValues[out->nNodes] = (void *)previousCentroid;
      }
      out->nodeNumbers[out->nNodes] = i - 1;
      out->nNodes++;
    }
  }

  MemoryContextSwitchTo(oldCtx);

  PG_RETURN_VOID();
}

   
                       
   
                                                                         
                                                                              
                                                                             
                                                                           
                                                                            
                          
   
                                                                         
   
static int
adjacent_cmp_bounds(TypeCacheEntry *typcache, RangeBound *arg, RangeBound *centroid)
{
  int cmp;

  Assert(arg->lower != centroid->lower);

  cmp = range_cmp_bounds(typcache, arg, centroid);

  if (centroid->lower)
  {
             
                                                                           
                                                                         
                                
       
                                                                       
                                                                     
                                                                       
                                                                      
                                                                       
                                                                    
                                    
       
                                      
                                                                 
                                                                 
                                                                
                                                               
       
                                                                          
                                                          
             
       
    if (cmp < 0 && !bounds_adjacent(typcache, *arg, *centroid))
    {
      return -1;
    }
    else
    {
      return 1;
    }
  }
  else
  {
             
                                                                          
                                                                          
                                
       
                                      
                                                                 
                                                                 
                                                                
                                                                
       
                                                                         
                                                                    
                                                                      
                       
             
       
    if (cmp <= 0)
    {
      return -1;
    }
    else
    {
      return 1;
    }
  }
}

             
                             
   
                                                                      
                                                                           
                                                                          
                                                                           
                                                                         
                                                                           
                                                                             
                   
   
                                                                         
                                                                         
                                                            
   
                                                                          
                                                                             
                                                                          
                                                
   
               
               
               
   
                                                                           
                                                                        
                                                                            
                                                                              
                                                                               
                                                                         
                                                                           
                                                                             
                                
             
   
static int
adjacent_inner_consistent(TypeCacheEntry *typcache, RangeBound *arg, RangeBound *centroid, RangeBound *prev)
{
  if (prev)
  {
    int prevcmp;
    int cmp;

       
                                                                       
                      
       
    prevcmp = adjacent_cmp_bounds(typcache, arg, prev);

                                                 
    cmp = range_cmp_bounds(typcache, centroid, prev);

                                                             
    if ((prevcmp < 0 && cmp >= 0) || (prevcmp > 0 && cmp < 0))
    {
      return 0;
    }
  }

  return adjacent_cmp_bounds(typcache, arg, centroid);
}

   
                                                                              
                                 
   
Datum
spg_range_quad_leaf_consistent(PG_FUNCTION_ARGS)
{
  spgLeafConsistentIn *in = (spgLeafConsistentIn *)PG_GETARG_POINTER(0);
  spgLeafConsistentOut *out = (spgLeafConsistentOut *)PG_GETARG_POINTER(1);
  RangeType *leafRange = DatumGetRangeTypeP(in->leafDatum);
  TypeCacheEntry *typcache;
  bool res;
  int i;

                           
  out->recheck = false;

                                  
  out->leafValue = in->leafDatum;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(leafRange));

                                          
  res = true;
  for (i = 0; i < in->nkeys; i++)
  {
    Datum keyDatum = in->scankeys[i].sk_argument;

                                                              
    switch (in->scankeys[i].sk_strategy)
    {
    case RANGESTRAT_BEFORE:
      res = range_before_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_OVERLEFT:
      res = range_overleft_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_OVERLAPS:
      res = range_overlaps_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_OVERRIGHT:
      res = range_overright_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_AFTER:
      res = range_after_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_ADJACENT:
      res = range_adjacent_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_CONTAINS:
      res = range_contains_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_CONTAINED_BY:
      res = range_contained_by_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    case RANGESTRAT_CONTAINS_ELEM:
      res = range_contains_elem_internal(typcache, leafRange, keyDatum);
      break;
    case RANGESTRAT_EQ:
      res = range_eq_internal(typcache, leafRange, DatumGetRangeTypeP(keyDatum));
      break;
    default:
      elog(ERROR, "unrecognized range strategy: %d", in->scankeys[i].sk_strategy);
      break;
    }

       
                                                                    
                        
       
    if (!res)
    {
      break;
    }
  }

  PG_RETURN_BOOL(res);
}
