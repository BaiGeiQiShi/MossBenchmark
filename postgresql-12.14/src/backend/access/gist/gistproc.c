                                                                            
   
              
                                                                              
              
   
                                                                         
   
   
                                                                         
                                                                        
   
                  
                                      
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/gist.h"
#include "access/stratnum.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/geo_decls.h"

static bool
gist_box_leaf_consistent(BOX *key, BOX *query, StrategyNumber strategy);
static bool
rtree_internal_consistent(BOX *key, BOX *query, StrategyNumber strategy);

                                     
#define LIMIT_RATIO 0.3

                                                    
           
                                                    

   
                                                                       
   
static void
rt_box_union(BOX *n, const BOX *a, const BOX *b)
{
  n->high.x = float8_max(a->high.x, b->high.x);
  n->high.y = float8_max(a->high.y, b->high.y);
  n->low.x = float8_min(a->low.x, b->low.x);
  n->low.y = float8_min(a->low.y, b->low.y);
}

   
                                                   
                                             
   
static float8
size_box(const BOX *box)
{
     
                                                                          
                                                                            
                                                               
     
                                                                        
     
  if (float8_le(box->high.x, box->low.x) || float8_le(box->high.y, box->low.y))
  {
    return 0.0;
  }

     
                                                                            
                                                                        
                                               
     
  if (isnan(box->high.x) || isnan(box->high.y))
  {
    return get_float8_infinity();
  }
  return float8_mul(float8_mi(box->high.x, box->low.x), float8_mi(box->high.y, box->low.y));
}

   
                                                                    
                                                                       
   
static float8
box_penalty(const BOX *original, const BOX *new)
{
  BOX unionbox;

  rt_box_union(&unionbox, original, new);
  return float8_mi(size_box(&unionbox), size_box(original));
}

   
                                        
   
                                                            
                                                                
                                                   
   
Datum
gist_box_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  BOX *query = PG_GETARG_BOX_P(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

                                        
  bool *recheck = (bool *)PG_GETARG_POINTER(4);

                                                   
  *recheck = false;

  if (DatumGetBoxP(entry->key) == NULL || query == NULL)
  {
    PG_RETURN_BOOL(false);
  }

     
                                                                   
                              
     
  if (GIST_LEAF(entry))
  {
    PG_RETURN_BOOL(gist_box_leaf_consistent(DatumGetBoxP(entry->key), query, strategy));
  }
  else
  {
    PG_RETURN_BOOL(rtree_internal_consistent(DatumGetBoxP(entry->key), query, strategy));
  }
}

   
                                    
   
static void
adjustBox(BOX *b, const BOX *addon)
{
  if (float8_lt(b->high.x, addon->high.x))
  {
    b->high.x = addon->high.x;
  }
  if (float8_gt(b->low.x, addon->low.x))
  {
    b->low.x = addon->low.x;
  }
  if (float8_lt(b->high.y, addon->high.y))
  {
    b->high.y = addon->high.y;
  }
  if (float8_gt(b->low.y, addon->low.y))
  {
    b->low.y = addon->low.y;
  }
}

   
                                   
   
                                                                              
   
Datum
gist_box_union(PG_FUNCTION_ARGS)
{
  GistEntryVector *entryvec = (GistEntryVector *)PG_GETARG_POINTER(0);
  int *sizep = (int *)PG_GETARG_POINTER(1);
  int numranges, i;
  BOX *cur, *pageunion;

  numranges = entryvec->n;
  pageunion = (BOX *)palloc(sizeof(BOX));
  cur = DatumGetBoxP(entryvec->vector[0].key);
  memcpy((void *)pageunion, (void *)cur, sizeof(BOX));

  for (i = 1; i < numranges; i++)
  {
    cur = DatumGetBoxP(entryvec->vector[i].key);
    adjustBox(pageunion, cur);
  }
  *sizep = sizeof(BOX);

  PG_RETURN_POINTER(pageunion);
}

   
                                                              
                                             
   

   
                                                            
   
                                                                       
   
Datum
gist_box_penalty(PG_FUNCTION_ARGS)
{
  GISTENTRY *origentry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *newentry = (GISTENTRY *)PG_GETARG_POINTER(1);
  float *result = (float *)PG_GETARG_POINTER(2);
  BOX *origbox = DatumGetBoxP(origentry->key);
  BOX *newbox = DatumGetBoxP(newentry->key);

  *result = (float)box_penalty(origbox, newbox);
  PG_RETURN_POINTER(result);
}

   
                                                             
                                 
   
static void
fallbackSplit(GistEntryVector *entryvec, GIST_SPLITVEC *v)
{
  OffsetNumber i, maxoff;
  BOX *unionL = NULL, *unionR = NULL;
  int nbytes;

  maxoff = entryvec->n - 1;

  nbytes = (maxoff + 2) * sizeof(OffsetNumber);
  v->spl_left = (OffsetNumber *)palloc(nbytes);
  v->spl_right = (OffsetNumber *)palloc(nbytes);
  v->spl_nleft = v->spl_nright = 0;

  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    BOX *cur = DatumGetBoxP(entryvec->vector[i].key);

    if (i <= (maxoff - FirstOffsetNumber + 1) / 2)
    {
      v->spl_left[v->spl_nleft] = i;
      if (unionL == NULL)
      {
        unionL = (BOX *)palloc(sizeof(BOX));
        *unionL = *cur;
      }
      else
      {
        adjustBox(unionL, cur);
      }

      v->spl_nleft++;
    }
    else
    {
      v->spl_right[v->spl_nright] = i;
      if (unionR == NULL)
      {
        unionR = (BOX *)palloc(sizeof(BOX));
        *unionR = *cur;
      }
      else
      {
        adjustBox(unionR, cur);
      }

      v->spl_nright++;
    }
  }

  v->spl_ldatum = BoxPGetDatum(unionL);
  v->spl_rdatum = BoxPGetDatum(unionR);
}

   
                                                                            
                                                                  
   
typedef struct
{
                                           
  int index;
                                                                        
  float8 delta;
} CommonEntry;

   
                                                                          
                                                
   
typedef struct
{
  int entriesCount;                                          
  BOX boundingBox;                                               

                                                          

  bool first;                                        

  float8 leftUpper;                                    
  float8 rightLower;                                    

  float4 ratio;
  float4 overlap;
  int dim;                              
  float8 range;                                           
                                   
} ConsiderSplitContext;

   
                                                  
   
typedef struct
{
  float8 lower, upper;
} SplitInterval;

   
                                                                
   
static int
interval_cmp_lower(const void *i1, const void *i2)
{
  float8 lower1 = ((const SplitInterval *)i1)->lower, lower2 = ((const SplitInterval *)i2)->lower;

  return float8_cmp_internal(lower1, lower2);
}

   
                                                                
   
static int
interval_cmp_upper(const void *i1, const void *i2)
{
  float8 upper1 = ((const SplitInterval *)i1)->upper, upper2 = ((const SplitInterval *)i2)->upper;

  return float8_cmp_internal(upper1, upper2);
}

   
                                              
   
static inline float
non_negative(float val)
{
  if (val >= 0.0f)
  {
    return val;
  }
  else
  {
    return 0.0f;
  }
}

   
                                                                         
   
static inline void
g_box_consider_split(ConsiderSplitContext *context, int dimNum, float8 rightLower, int minLeftCount, float8 leftUpper, int maxLeftCount)
{
  int leftCount, rightCount;
  float4 ratio, overlap;
  float8 range;

     
                                                                             
                        
     
  if (minLeftCount >= (context->entriesCount + 1) / 2)
  {
    leftCount = minLeftCount;
  }
  else
  {
    if (maxLeftCount <= context->entriesCount / 2)
    {
      leftCount = maxLeftCount;
    }
    else
    {
      leftCount = context->entriesCount / 2;
    }
  }
  rightCount = context->entriesCount - leftCount;

     
                                                                      
                    
     
  ratio = float4_div(Min(leftCount, rightCount), context->entriesCount);

  if (ratio > LIMIT_RATIO)
  {
    bool selectthis = false;

       
                                                                         
                                                                           
                                                                           
                                                                         
                                        
       
    if (dimNum == 0)
    {
      range = float8_mi(context->boundingBox.high.x, context->boundingBox.low.x);
    }
    else
    {
      range = float8_mi(context->boundingBox.high.y, context->boundingBox.low.y);
    }

    overlap = float8_div(float8_mi(leftUpper, rightLower), range);

                                                        
    if (context->first)
    {
      selectthis = true;
    }
    else if (context->dim == dimNum)
    {
         
                                                                     
                                                            
         
      if (overlap < context->overlap || (overlap == context->overlap && ratio > context->ratio))
      {
        selectthis = true;
      }
    }
    else
    {
         
                                                                     
                                                                    
                                                                        
                                                                    
                                                                         
                                                                       
                                                                      
                                                                      
                                                                       
                                                                  
                                                                       
                                                                       
                                                                      
                                                                      
                                                                 
         
      if (non_negative(overlap) < non_negative(context->overlap) || (range > context->range && non_negative(overlap) <= non_negative(context->overlap)))
      {
        selectthis = true;
      }
    }

    if (selectthis)
    {
                                                 
      context->first = false;
      context->ratio = ratio;
      context->range = range;
      context->overlap = overlap;
      context->rightLower = rightLower;
      context->leftUpper = leftUpper;
      context->dim = dimNum;
    }
  }
}

   
                                           
   
static int
common_entry_cmp(const void *i1, const void *i2)
{
  float8 delta1 = ((const CommonEntry *)i1)->delta, delta2 = ((const CommonEntry *)i2)->delta;

  return float8_cmp_internal(delta1, delta2);
}

   
                                                                              
                                                                           
   
                                                                             
                                                                             
                                                                         
                                                                         
                                                                           
                                                                      
                          
   
                                                              
   
                                                       
                                                        
                                                                              
                                     
   
                                                             
   
                    
                                                                                 
                                                                           
                                                                              
   
Datum
gist_box_picksplit(PG_FUNCTION_ARGS)
{
  GistEntryVector *entryvec = (GistEntryVector *)PG_GETARG_POINTER(0);
  GIST_SPLITVEC *v = (GIST_SPLITVEC *)PG_GETARG_POINTER(1);
  OffsetNumber i, maxoff;
  ConsiderSplitContext context;
  BOX *box, *leftBox, *rightBox;
  int dim, commonEntriesCount;
  SplitInterval *intervalsLower, *intervalsUpper;
  CommonEntry *commonEntries;
  int nentries;

  memset(&context, 0, sizeof(ConsiderSplitContext));

  maxoff = entryvec->n - 1;
  nentries = context.entriesCount = maxoff - FirstOffsetNumber + 1;

                                                
  intervalsLower = (SplitInterval *)palloc(nentries * sizeof(SplitInterval));
  intervalsUpper = (SplitInterval *)palloc(nentries * sizeof(SplitInterval));

     
                                                                      
     
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    box = DatumGetBoxP(entryvec->vector[i].key);
    if (i == FirstOffsetNumber)
    {
      context.boundingBox = *box;
    }
    else
    {
      adjustBox(&context.boundingBox, box);
    }
  }

     
                                                    
     
  context.first = true;                           
  for (dim = 0; dim < 2; dim++)
  {
    float8 leftUpper, rightLower;
    int i1, i2;

                                                                 
    for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
    {
      box = DatumGetBoxP(entryvec->vector[i].key);
      if (dim == 0)
      {
        intervalsLower[i - FirstOffsetNumber].lower = box->low.x;
        intervalsLower[i - FirstOffsetNumber].upper = box->high.x;
      }
      else
      {
        intervalsLower[i - FirstOffsetNumber].lower = box->low.y;
        intervalsLower[i - FirstOffsetNumber].upper = box->high.y;
      }
    }

       
                                                                           
                              
       
    memcpy(intervalsUpper, intervalsLower, sizeof(SplitInterval) * nentries);
    qsort(intervalsLower, nentries, sizeof(SplitInterval), interval_cmp_lower);
    qsort(intervalsUpper, nentries, sizeof(SplitInterval), interval_cmp_upper);

           
                                                                          
                                                                         
       
                                                                   
       
                 
           
              
              
                
       
                                                                     
                                                                        
                                                                     
                                                                   
                                                                       
                   
       
                                                                   
                        
                        
                        
       
                            
                        
                        
                        
       

       
                                                                          
                                  
       
    i1 = 0;
    i2 = 0;
    rightLower = intervalsLower[i1].lower;
    leftUpper = intervalsUpper[i2].lower;
    while (true)
    {
         
                                               
         
      while (i1 < nentries && float8_eq(rightLower, intervalsLower[i1].lower))
      {
        if (float8_lt(leftUpper, intervalsLower[i1].upper))
        {
          leftUpper = intervalsLower[i1].upper;
        }
        i1++;
      }
      if (i1 >= nentries)
      {
        break;
      }
      rightLower = intervalsLower[i1].lower;

         
                                                                      
                     
         
      while (i2 < nentries && float8_le(intervalsUpper[i2].upper, leftUpper))
      {
        i2++;
      }

         
                               
         
      g_box_consider_split(&context, dim, rightLower, i1, leftUpper, i2);
    }

       
                                                                        
                                   
       
    i1 = nentries - 1;
    i2 = nentries - 1;
    rightLower = intervalsLower[i1].upper;
    leftUpper = intervalsUpper[i2].upper;
    while (true)
    {
         
                                              
         
      while (i2 >= 0 && float8_eq(leftUpper, intervalsUpper[i2].upper))
      {
        if (float8_gt(rightLower, intervalsUpper[i2].lower))
        {
          rightLower = intervalsUpper[i2].lower;
        }
        i2--;
      }
      if (i2 < 0)
      {
        break;
      }
      leftUpper = intervalsUpper[i2].upper;

         
                                                                      
                      
         
      while (i1 >= 0 && float8_ge(intervalsLower[i1].lower, rightLower))
      {
        i1--;
      }

         
                               
         
      g_box_consider_split(&context, dim, rightLower, i1 + 1, leftUpper, i2 + 1);
    }
  }

     
                                                                    
     
  if (context.first)
  {
    fallbackSplit(entryvec, v);
    PG_RETURN_POINTER(v);
  }

     
                                                         
     
                                                                            
                                                                          
                                                                          
     

                                    
  v->spl_left = (OffsetNumber *)palloc(nentries * sizeof(OffsetNumber));
  v->spl_right = (OffsetNumber *)palloc(nentries * sizeof(OffsetNumber));
  v->spl_nleft = 0;
  v->spl_nright = 0;

                                                        
  leftBox = palloc0(sizeof(BOX));
  rightBox = palloc0(sizeof(BOX));

     
                                                                             
                                                                 
     
  commonEntriesCount = 0;
  commonEntries = (CommonEntry *)palloc(nentries * sizeof(CommonEntry));

                                                                  
#define PLACE_LEFT(box, off)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (v->spl_nleft > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
      adjustBox(leftBox, box);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      *leftBox = *(box);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    v->spl_left[v->spl_nleft++] = off;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

#define PLACE_RIGHT(box, off)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (v->spl_nright > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
      adjustBox(rightBox, box);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      *rightBox = *(box);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    v->spl_right[v->spl_nright++] = off;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  } while (0)

     
                                                                            
                     
     
  for (i = FirstOffsetNumber; i <= maxoff; i = OffsetNumberNext(i))
  {
    float8 lower, upper;

       
                                                       
       
    box = DatumGetBoxP(entryvec->vector[i].key);
    if (context.dim == 0)
    {
      lower = box->low.x;
      upper = box->high.x;
    }
    else
    {
      lower = box->low.y;
      upper = box->high.y;
    }

    if (float8_le(upper, context.leftUpper))
    {
                                  
      if (float8_ge(lower, context.rightLower))
      {
                                                             
        commonEntries[commonEntriesCount++].index = i;
      }
      else
      {
                                                                       
        PLACE_LEFT(box, i);
      }
    }
    else
    {
         
                                                                         
                                                                        
                
         
      Assert(float8_ge(lower, context.rightLower));

                                                                     
      PLACE_RIGHT(box, i);
    }
  }

     
                                          
     
  if (commonEntriesCount > 0)
  {
       
                                                                       
                                     
       
    int m = ceil(LIMIT_RATIO * nentries);

       
                                                                     
                         
       
    for (i = 0; i < commonEntriesCount; i++)
    {
      box = DatumGetBoxP(entryvec->vector[commonEntries[i].index].key);
      commonEntries[i].delta = Abs(float8_mi(box_penalty(leftBox, box), box_penalty(rightBox, box)));
    }

       
                                                                         
                                         
       
    qsort(commonEntries, commonEntriesCount, sizeof(CommonEntry), common_entry_cmp);

       
                                                   
       
    for (i = 0; i < commonEntriesCount; i++)
    {
      box = DatumGetBoxP(entryvec->vector[commonEntries[i].index].key);

         
                                                                         
                      
         
      if (v->spl_nleft + (commonEntriesCount - i) <= m)
      {
        PLACE_LEFT(box, commonEntries[i].index);
      }
      else if (v->spl_nright + (commonEntriesCount - i) <= m)
      {
        PLACE_RIGHT(box, commonEntries[i].index);
      }
      else
      {
                                                           
        if (box_penalty(leftBox, box) < box_penalty(rightBox, box))
        {
          PLACE_LEFT(box, commonEntries[i].index);
        }
        else
        {
          PLACE_RIGHT(box, commonEntries[i].index);
        }
      }
    }
  }

  v->spl_ldatum = PointerGetDatum(leftBox);
  v->spl_rdatum = PointerGetDatum(rightBox);
  PG_RETURN_POINTER(v);
}

   
                   
   
                                                                             
                                
   
                                                                          
                                                                              
                             
   
Datum
gist_box_same(PG_FUNCTION_ARGS)
{
  BOX *b1 = PG_GETARG_BOX_P(0);
  BOX *b2 = PG_GETARG_BOX_P(1);
  bool *result = (bool *)PG_GETARG_POINTER(2);

  if (b1 && b2)
  {
    *result = (float8_eq(b1->low.x, b2->low.x) && float8_eq(b1->low.y, b2->low.y) && float8_eq(b1->high.x, b2->high.x) && float8_eq(b1->high.y, b2->high.y));
  }
  else
  {
    *result = (b1 == NULL && b2 == NULL);
  }
  PG_RETURN_POINTER(result);
}

   
                                                                   
   
static bool
gist_box_leaf_consistent(BOX *key, BOX *query, StrategyNumber strategy)
{
  bool retval;

  switch (strategy)
  {
  case RTLeftStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_left, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverLeftStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_overleft, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverlapStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_overlap, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverRightStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_overright, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTRightStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_right, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTSameStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_same, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTContainsStrategyNumber:
  case RTOldContainsStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_contain, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTContainedByStrategyNumber:
  case RTOldContainedByStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_contained, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverBelowStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_overbelow, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTBelowStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_below, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTAboveStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_above, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverAboveStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_overabove, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  default:
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    retval = false;                          
    break;
  }
  return retval;
}

                                           
                                                             
                                           

   
                                                 
   
                                                                          
                                 
   
static bool
rtree_internal_consistent(BOX *key, BOX *query, StrategyNumber strategy)
{
  bool retval;

  switch (strategy)
  {
  case RTLeftStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_overright, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverLeftStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_right, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverlapStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_overlap, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverRightStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_left, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTRightStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_overleft, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTSameStrategyNumber:
  case RTContainsStrategyNumber:
  case RTOldContainsStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_contain, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTContainedByStrategyNumber:
  case RTOldContainedByStrategyNumber:
    retval = DatumGetBool(DirectFunctionCall2(box_overlap, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverBelowStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_above, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTBelowStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_overabove, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTAboveStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_overbelow, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  case RTOverAboveStrategyNumber:
    retval = !DatumGetBool(DirectFunctionCall2(box_below, PointerGetDatum(key), PointerGetDatum(query)));
    break;
  default:
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    retval = false;                          
    break;
  }
  return retval;
}

                                                    
               
                                                    

   
                                                                       
   
Datum
gist_poly_compress(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *retval;

  if (entry->leafkey)
  {
    POLYGON *in = DatumGetPolygonP(entry->key);
    BOX *r;

    r = (BOX *)palloc(sizeof(BOX));
    memcpy((void *)r, (void *)&(in->boundbox), sizeof(BOX));

    retval = (GISTENTRY *)palloc(sizeof(GISTENTRY));
    gistentryinit(*retval, PointerGetDatum(r), entry->rel, entry->page, entry->offset, false);
  }
  else
  {
    retval = entry;
  }
  PG_RETURN_POINTER(retval);
}

   
                                           
   
Datum
gist_poly_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  POLYGON *query = PG_GETARG_POLYGON_P(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

                                        
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  bool result;

                                                     
  *recheck = true;

  if (DatumGetBoxP(entry->key) == NULL || query == NULL)
  {
    PG_RETURN_BOOL(false);
  }

     
                                                                 
                                                                        
                                                                 
     
  result = rtree_internal_consistent(DatumGetBoxP(entry->key), &(query->boundbox), strategy);

                                                     
  PG_FREE_IF_COPY(query, 1);

  PG_RETURN_BOOL(result);
}

                                                    
              
                                                    

   
                                                                     
   
Datum
gist_circle_compress(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *retval;

  if (entry->leafkey)
  {
    CIRCLE *in = DatumGetCircleP(entry->key);
    BOX *r;

    r = (BOX *)palloc(sizeof(BOX));
    r->high.x = float8_pl(in->center.x, in->radius);
    r->low.x = float8_mi(in->center.x, in->radius);
    r->high.y = float8_pl(in->center.y, in->radius);
    r->low.y = float8_mi(in->center.y, in->radius);

    retval = (GISTENTRY *)palloc(sizeof(GISTENTRY));
    gistentryinit(*retval, PointerGetDatum(r), entry->rel, entry->page, entry->offset, false);
  }
  else
  {
    retval = entry;
  }
  PG_RETURN_POINTER(retval);
}

   
                                          
   
Datum
gist_circle_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  CIRCLE *query = PG_GETARG_CIRCLE_P(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

                                        
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  BOX bbox;
  bool result;

                                                     
  *recheck = true;

  if (DatumGetBoxP(entry->key) == NULL || query == NULL)
  {
    PG_RETURN_BOOL(false);
  }

     
                                                                 
                                                                        
                                                                
     
  bbox.high.x = float8_pl(query->center.x, query->radius);
  bbox.low.x = float8_mi(query->center.x, query->radius);
  bbox.high.y = float8_pl(query->center.y, query->radius);
  bbox.low.y = float8_mi(query->center.y, query->radius);

  result = rtree_internal_consistent(DatumGetBoxP(entry->key), &bbox, strategy);

  PG_RETURN_BOOL(result);
}

                                                    
             
                                                    

Datum
gist_point_compress(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);

  if (entry->leafkey)                      
  {
    BOX *box = palloc(sizeof(BOX));
    Point *point = DatumGetPointP(entry->key);
    GISTENTRY *retval = palloc(sizeof(GISTENTRY));

    box->high = box->low = *point;

    gistentryinit(*retval, BoxPGetDatum(box), entry->rel, entry->page, entry->offset, false);

    PG_RETURN_POINTER(retval);
  }

  PG_RETURN_POINTER(entry);
}

   
                               
   
                                                                        
              
   
Datum
gist_point_fetch(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  BOX *in = DatumGetBoxP(entry->key);
  Point *r;
  GISTENTRY *retval;

  retval = palloc(sizeof(GISTENTRY));

  r = (Point *)palloc(sizeof(Point));
  r->x = in->high.x;
  r->y = in->high.y;
  gistentryinit(*retval, PointerGetDatum(r), entry->rel, entry->page, entry->offset, false);

  PG_RETURN_POINTER(retval);
}

#define point_point_distance(p1, p2) DatumGetFloat8(DirectFunctionCall2(point_distance, PointPGetDatum(p1), PointPGetDatum(p2)))

static float8
computeDistance(bool isLeaf, BOX *box, Point *point)
{
  float8 result = 0.0;

  if (isLeaf)
  {
                                        
    result = point_point_distance(point, &box->low);
  }
  else if (point->x <= box->high.x && point->x >= box->low.x && point->y <= box->high.y && point->y >= box->low.y)
  {
                              
    result = 0.0;
  }
  else if (point->x <= box->high.x && point->x >= box->low.x)
  {
                                    
    Assert(box->low.y <= box->high.y);
    if (point->y > box->high.y)
    {
      result = float8_mi(point->y, box->high.y);
    }
    else if (point->y < box->low.y)
    {
      result = float8_mi(box->low.y, point->y);
    }
    else
    {
      elog(ERROR, "inconsistent point values");
    }
  }
  else if (point->y <= box->high.y && point->y >= box->low.y)
  {
                                          
    Assert(box->low.x <= box->high.x);
    if (point->x > box->high.x)
    {
      result = float8_mi(point->x, box->high.x);
    }
    else if (point->x < box->low.x)
    {
      result = float8_mi(box->low.x, point->x);
    }
    else
    {
      elog(ERROR, "inconsistent point values");
    }
  }
  else
  {
                                        
    Point p;
    float8 subresult;

    result = point_point_distance(point, &box->low);

    subresult = point_point_distance(point, &box->high);
    if (result > subresult)
    {
      result = subresult;
    }

    p.x = box->low.x;
    p.y = box->high.y;
    subresult = point_point_distance(point, &p);
    if (result > subresult)
    {
      result = subresult;
    }

    p.x = box->high.x;
    p.y = box->low.y;
    subresult = point_point_distance(point, &p);
    if (result > subresult)
    {
      result = subresult;
    }
  }

  return result;
}

static bool
gist_point_consistent_internal(StrategyNumber strategy, bool isLeaf, BOX *key, Point *query)
{
  bool result = false;

  switch (strategy)
  {
  case RTLeftStrategyNumber:
    result = FPlt(key->low.x, query->x);
    break;
  case RTRightStrategyNumber:
    result = FPgt(key->high.x, query->x);
    break;
  case RTAboveStrategyNumber:
    result = FPgt(key->high.y, query->y);
    break;
  case RTBelowStrategyNumber:
    result = FPlt(key->low.y, query->y);
    break;
  case RTSameStrategyNumber:
    if (isLeaf)
    {
                                                               
      result = (FPeq(key->low.x, query->x) && FPeq(key->low.y, query->y));
    }
    else
    {
      result = (FPle(query->x, key->high.x) && FPge(query->x, key->low.x) && FPle(query->y, key->high.y) && FPge(query->y, key->low.y));
    }
    break;
  default:
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    result = false;                          
    break;
  }

  return result;
}

#define GeoStrategyNumberOffset 20
#define PointStrategyNumberGroup 0
#define BoxStrategyNumberGroup 1
#define PolygonStrategyNumberGroup 2
#define CircleStrategyNumberGroup 3

Datum
gist_point_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  bool result;
  StrategyNumber strategyGroup = strategy / GeoStrategyNumberOffset;

  switch (strategyGroup)
  {
  case PointStrategyNumberGroup:
    result = gist_point_consistent_internal(strategy % GeoStrategyNumberOffset, GIST_LEAF(entry), DatumGetBoxP(entry->key), PG_GETARG_POINT_P(1));
    *recheck = false;
    break;
  case BoxStrategyNumberGroup:
  {
       
                                                                   
                                          
       
                                                                  
                                                                  
                                                                  
                                                              
                                                                 
                                                                 
                    
       
    BOX *query, *key;

    query = PG_GETARG_BOX_P(1);
    key = DatumGetBoxP(entry->key);

    result = (key->high.x >= query->low.x && key->low.x <= query->high.x && key->high.y >= query->low.y && key->low.y <= query->high.y);
    *recheck = false;
  }
  break;
  case PolygonStrategyNumberGroup:
  {
    POLYGON *query = PG_GETARG_POLYGON_P(1);

    result = DatumGetBool(DirectFunctionCall5(gist_poly_consistent, PointerGetDatum(entry), PolygonPGetDatum(query), Int16GetDatum(RTOverlapStrategyNumber), 0, PointerGetDatum(recheck)));

    if (GIST_LEAF(entry) && result)
    {
         
                                                               
                                             
         
      BOX *box = DatumGetBoxP(entry->key);

      Assert(box->high.x == box->low.x && box->high.y == box->low.y);
      result = DatumGetBool(DirectFunctionCall2(poly_contain_pt, PolygonPGetDatum(query), PointPGetDatum(&box->high)));
      *recheck = false;
    }
  }
  break;
  case CircleStrategyNumberGroup:
  {
    CIRCLE *query = PG_GETARG_CIRCLE_P(1);

    result = DatumGetBool(DirectFunctionCall5(gist_circle_consistent, PointerGetDatum(entry), CirclePGetDatum(query), Int16GetDatum(RTOverlapStrategyNumber), 0, PointerGetDatum(recheck)));

    if (GIST_LEAF(entry) && result)
    {
         
                                                               
                                             
         
      BOX *box = DatumGetBoxP(entry->key);

      Assert(box->high.x == box->low.x && box->high.y == box->low.y);
      result = DatumGetBool(DirectFunctionCall2(circle_contain_pt, CirclePGetDatum(query), PointPGetDatum(&box->high)));
      *recheck = false;
    }
  }
  break;
  default:
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    result = false;                          
    break;
  }

  PG_RETURN_BOOL(result);
}

Datum
gist_point_distance(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);
  float8 distance;
  StrategyNumber strategyGroup = strategy / GeoStrategyNumberOffset;

  switch (strategyGroup)
  {
  case PointStrategyNumberGroup:
    distance = computeDistance(GIST_LEAF(entry), DatumGetBoxP(entry->key), PG_GETARG_POINT_P(1));
    break;
  default:
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    distance = 0.0;                          
    break;
  }

  PG_RETURN_FLOAT8(distance);
}

   
                                                                            
          
   
                                                                              
                                                                         
                                                                               
                                                                              
         
   
static float8
gist_bbox_distance(GISTENTRY *entry, Datum query, StrategyNumber strategy, bool *recheck)
{
  float8 distance;
  StrategyNumber strategyGroup = strategy / GeoStrategyNumberOffset;

                                                
  *recheck = true;

  switch (strategyGroup)
  {
  case PointStrategyNumberGroup:
    distance = computeDistance(false, DatumGetBoxP(entry->key), DatumGetPointP(query));
    break;
  default:
    elog(ERROR, "unrecognized strategy number: %d", strategy);
    distance = 0.0;                          
  }

  return distance;
}

Datum
gist_circle_distance(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  Datum query = PG_GETARG_DATUM(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

                                       
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  float8 distance;

  distance = gist_bbox_distance(entry, query, strategy, recheck);

  PG_RETURN_FLOAT8(distance);
}

Datum
gist_poly_distance(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  Datum query = PG_GETARG_DATUM(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

                                       
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  float8 distance;

  distance = gist_bbox_distance(entry, query, strategy, recheck);

  PG_RETURN_FLOAT8(distance);
}
