                                                                            
   
                
                                                                  
   
                                                                         
                                                                      
                                                                       
                                                                       
                                                                      
                                                                       
                                        
   
                                                                    
                                                                         
                     
   
             
             
                    
             
             
                               
        
        
        
        
        
   
                                                                     
                                                                     
                                                                       
                                                                 
             
   
                                                                      
                                                                         
                                                                          
                                                                      
                                                                       
                         
   
                                         
                                        
                                      
   
                                                                           
                                                                        
                                                                         
                   
   
                                                                  
                                                              
                                                                     
                                                                   
                                                                       
                                                                      
                             
   
                                                                       
                                                                         
                                                                      
                                       
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   

#include "postgres.h"

#include "access/spgist.h"
#include "access/spgist_private.h"
#include "access/stratnum.h"
#include "catalog/pg_type.h"
#include "utils/float.h"
#include "utils/fmgroids.h"
#include "utils/fmgrprotos.h"
#include "utils/geo_decls.h"

   
                        
   
                                                                        
                                                                 
                                      
   
static int
compareDoubles(const void *a, const void *b)
{
  float8 x = *(float8 *)a;
  float8 y = *(float8 *)b;

  if (x == y)
  {
    return 0;
  }
  return (x > y) ? 1 : -1;
}

typedef struct
{
  float8 low;
  float8 high;
} Range;

typedef struct
{
  Range left;
  Range right;
} RangeBox;

typedef struct
{
  RangeBox range_box_x;
  RangeBox range_box_y;
} RectBox;

   
                          
   
                                                                    
                                                                 
                                                                          
                                     
   
static uint8
getQuadrant(BOX *centroid, BOX *inBox)
{
  uint8 quadrant = 0;

  if (inBox->low.x > centroid->low.x)
  {
    quadrant |= 0x8;
  }

  if (inBox->high.x > centroid->high.x)
  {
    quadrant |= 0x4;
  }

  if (inBox->low.y > centroid->low.y)
  {
    quadrant |= 0x2;
  }

  if (inBox->high.y > centroid->high.y)
  {
    quadrant |= 0x1;
  }

  return quadrant;
}

   
                          
   
                                                                        
                                                                      
                                          
   
static RangeBox *
getRangeBox(BOX *box)
{
  RangeBox *range_box = (RangeBox *)palloc(sizeof(RangeBox));

  range_box->left.low = box->low.x;
  range_box->left.high = box->high.x;

  range_box->right.low = box->low.y;
  range_box->right.high = box->high.y;

  return range_box;
}

   
                                  
   
                                                                 
                                                      
   
static RectBox *
initRectBox(void)
{
  RectBox *rect_box = (RectBox *)palloc(sizeof(RectBox));
  float8 infinity = get_float8_infinity();

  rect_box->range_box_x.left.low = -infinity;
  rect_box->range_box_x.left.high = infinity;

  rect_box->range_box_x.right.low = -infinity;
  rect_box->range_box_x.right.high = infinity;

  rect_box->range_box_y.left.low = -infinity;
  rect_box->range_box_y.left.high = infinity;

  rect_box->range_box_y.right.low = -infinity;
  rect_box->range_box_y.right.high = infinity;

  return rect_box;
}

   
                                      
   
                                                                
                                                                       
                                
   
static RectBox *
nextRectBox(RectBox *rect_box, RangeBox *centroid, uint8 quadrant)
{
  RectBox *next_rect_box = (RectBox *)palloc(sizeof(RectBox));

  memcpy(next_rect_box, rect_box, sizeof(RectBox));

  if (quadrant & 0x8)
  {
    next_rect_box->range_box_x.left.low = centroid->left.low;
  }
  else
  {
    next_rect_box->range_box_x.left.high = centroid->left.low;
  }

  if (quadrant & 0x4)
  {
    next_rect_box->range_box_x.right.low = centroid->left.high;
  }
  else
  {
    next_rect_box->range_box_x.right.high = centroid->left.high;
  }

  if (quadrant & 0x2)
  {
    next_rect_box->range_box_y.left.low = centroid->right.low;
  }
  else
  {
    next_rect_box->range_box_y.left.high = centroid->right.low;
  }

  if (quadrant & 0x1)
  {
    next_rect_box->range_box_y.right.low = centroid->right.high;
  }
  else
  {
    next_rect_box->range_box_y.right.high = centroid->right.high;
  }

  return next_rect_box;
}

                                                              
static bool
overlap2D(RangeBox *range_box, Range *query)
{
  return FPge(range_box->right.high, query->low) && FPle(range_box->left.low, query->high);
}

                                                                 
static bool
overlap4D(RectBox *rect_box, RangeBox *query)
{
  return overlap2D(&rect_box->range_box_x, &query->left) && overlap2D(&rect_box->range_box_y, &query->right);
}

                                                         
static bool
contain2D(RangeBox *range_box, Range *query)
{
  return FPge(range_box->right.high, query->high) && FPle(range_box->left.low, query->low);
}

                                                            
static bool
contain4D(RectBox *rect_box, RangeBox *query)
{
  return contain2D(&rect_box->range_box_x, &query->left) && contain2D(&rect_box->range_box_y, &query->right);
}

                                                                 
static bool
contained2D(RangeBox *range_box, Range *query)
{
  return FPle(range_box->left.low, query->high) && FPge(range_box->left.high, query->low) && FPle(range_box->right.low, query->high) && FPge(range_box->right.high, query->low);
}

                                                                    
static bool
contained4D(RectBox *rect_box, RangeBox *query)
{
  return contained2D(&rect_box->range_box_x, &query->left) && contained2D(&rect_box->range_box_y, &query->right);
}

                                                                  
static bool
lower2D(RangeBox *range_box, Range *query)
{
  return FPlt(range_box->left.low, query->low) && FPlt(range_box->right.low, query->low);
}

                                                                             
static bool
overLower2D(RangeBox *range_box, Range *query)
{
  return FPle(range_box->left.low, query->high) && FPle(range_box->right.low, query->high);
}

                                                                   
static bool
higher2D(RangeBox *range_box, Range *query)
{
  return FPgt(range_box->left.high, query->high) && FPgt(range_box->right.high, query->high);
}

                                                                            
static bool
overHigher2D(RangeBox *range_box, Range *query)
{
  return FPge(range_box->left.high, query->low) && FPge(range_box->right.high, query->low);
}

                                                               
static bool
left4D(RectBox *rect_box, RangeBox *query)
{
  return lower2D(&rect_box->range_box_x, &query->left);
}

                                                                                 
static bool
overLeft4D(RectBox *rect_box, RangeBox *query)
{
  return overLower2D(&rect_box->range_box_x, &query->left);
}

                                                                
static bool
right4D(RectBox *rect_box, RangeBox *query)
{
  return higher2D(&rect_box->range_box_x, &query->left);
}

                                                                                
static bool
overRight4D(RectBox *rect_box, RangeBox *query)
{
  return overHigher2D(&rect_box->range_box_x, &query->left);
}

                                                                
static bool
below4D(RectBox *rect_box, RangeBox *query)
{
  return lower2D(&rect_box->range_box_y, &query->right);
}

                                                                          
static bool
overBelow4D(RectBox *rect_box, RangeBox *query)
{
  return overLower2D(&rect_box->range_box_y, &query->right);
}

                                                                
static bool
above4D(RectBox *rect_box, RangeBox *query)
{
  return higher2D(&rect_box->range_box_y, &query->right);
}

                                                                             
static bool
overAbove4D(RectBox *rect_box, RangeBox *query)
{
  return overHigher2D(&rect_box->range_box_y, &query->right);
}

                                                             
static double
pointToRectBoxDistance(Point *point, RectBox *rect_box)
{
  double dx;
  double dy;

  if (point->x < rect_box->range_box_x.left.low)
  {
    dx = rect_box->range_box_x.left.low - point->x;
  }
  else if (point->x > rect_box->range_box_x.right.high)
  {
    dx = point->x - rect_box->range_box_x.right.high;
  }
  else
  {
    dx = 0;
  }

  if (point->y < rect_box->range_box_y.left.low)
  {
    dy = rect_box->range_box_y.left.low - point->y;
  }
  else if (point->y > rect_box->range_box_y.right.high)
  {
    dy = point->y - rect_box->range_box_y.right.high;
  }
  else
  {
    dy = 0;
  }

  return HYPOT(dx, dy);
}

   
                           
   
Datum
spg_box_quad_config(PG_FUNCTION_ARGS)
{
  spgConfigOut *cfg = (spgConfigOut *)PG_GETARG_POINTER(1);

  cfg->prefixType = BOXOID;
  cfg->labelType = VOIDOID;                                 
  cfg->canReturnData = true;
  cfg->longValuesOK = false;

  PG_RETURN_VOID();
}

   
                           
   
Datum
spg_box_quad_choose(PG_FUNCTION_ARGS)
{
  spgChooseIn *in = (spgChooseIn *)PG_GETARG_POINTER(0);
  spgChooseOut *out = (spgChooseOut *)PG_GETARG_POINTER(1);
  BOX *centroid = DatumGetBoxP(in->prefixDatum), *box = DatumGetBoxP(in->leafDatum);

  out->resultType = spgMatchNode;
  out->result.matchNode.restDatum = BoxPGetDatum(box);

                                                   
  if (!in->allTheSame)
  {
    out->result.matchNode.nodeN = getQuadrant(centroid, box);
  }

  PG_RETURN_VOID();
}

   
                               
   
                                                                     
                                                        
   
Datum
spg_box_quad_picksplit(PG_FUNCTION_ARGS)
{
  spgPickSplitIn *in = (spgPickSplitIn *)PG_GETARG_POINTER(0);
  spgPickSplitOut *out = (spgPickSplitOut *)PG_GETARG_POINTER(1);
  BOX *centroid;
  int median, i;
  float8 *lowXs = palloc(sizeof(float8) * in->nTuples);
  float8 *highXs = palloc(sizeof(float8) * in->nTuples);
  float8 *lowYs = palloc(sizeof(float8) * in->nTuples);
  float8 *highYs = palloc(sizeof(float8) * in->nTuples);

                                              
  for (i = 0; i < in->nTuples; i++)
  {
    BOX *box = DatumGetBoxP(in->datums[i]);

    lowXs[i] = box->low.x;
    highXs[i] = box->high.x;
    lowYs[i] = box->low.y;
    highYs[i] = box->high.y;
  }

  qsort(lowXs, in->nTuples, sizeof(float8), compareDoubles);
  qsort(highXs, in->nTuples, sizeof(float8), compareDoubles);
  qsort(lowYs, in->nTuples, sizeof(float8), compareDoubles);
  qsort(highYs, in->nTuples, sizeof(float8), compareDoubles);

  median = in->nTuples / 2;

  centroid = palloc(sizeof(BOX));

  centroid->low.x = lowXs[median];
  centroid->high.x = highXs[median];
  centroid->low.y = lowYs[median];
  centroid->high.y = highYs[median];

                       
  out->hasPrefix = true;
  out->prefixDatum = BoxPGetDatum(centroid);

  out->nNodes = 16;
  out->nodeLabels = NULL;                                 

  out->mapTuplesToNodes = palloc(sizeof(int) * in->nTuples);
  out->leafTupleDatums = palloc(sizeof(Datum) * in->nTuples);

     
                                                                             
                          
     
  for (i = 0; i < in->nTuples; i++)
  {
    BOX *box = DatumGetBoxP(in->datums[i]);
    uint8 quadrant = getQuadrant(centroid, box);

    out->leafTupleDatums[i] = BoxPGetDatum(box);
    out->mapTuplesToNodes[i] = quadrant;
  }

  PG_RETURN_VOID();
}

   
                                                                        
   
static bool
is_bounding_box_test_exact(StrategyNumber strategy)
{
  switch (strategy)
  {
  case RTLeftStrategyNumber:
  case RTOverLeftStrategyNumber:
  case RTOverRightStrategyNumber:
  case RTRightStrategyNumber:
  case RTOverBelowStrategyNumber:
  case RTBelowStrategyNumber:
  case RTAboveStrategyNumber:
  case RTOverAboveStrategyNumber:
    return true;

  default:
    return false;
  }
}

   
                                 
   
static BOX *
spg_box_quad_get_scankey_bbox(ScanKey sk, bool *recheck)
{
  switch (sk->sk_subtype)
  {
  case BOXOID:
    return DatumGetBoxP(sk->sk_argument);

  case POLYGONOID:
    if (recheck && !is_bounding_box_test_exact(sk->sk_strategy))
    {
      *recheck = true;
    }
    return &DatumGetPolygonP(sk->sk_argument)->boundbox;

  default:
    elog(ERROR, "unrecognized scankey subtype: %d", sk->sk_subtype);
    return NULL;
  }
}

   
                                     
   
Datum
spg_box_quad_inner_consistent(PG_FUNCTION_ARGS)
{
  spgInnerConsistentIn *in = (spgInnerConsistentIn *)PG_GETARG_POINTER(0);
  spgInnerConsistentOut *out = (spgInnerConsistentOut *)PG_GETARG_POINTER(1);
  int i;
  MemoryContext old_ctx;
  RectBox *rect_box;
  uint8 quadrant;
  RangeBox *centroid, **queries;

     
                                                                             
                                          
     
  if (in->traversalValue)
  {
    rect_box = in->traversalValue;
  }
  else
  {
    rect_box = initRectBox();
  }

  if (in->allTheSame)
  {
                                                 
    out->nNodes = in->nNodes;
    out->nodeNumbers = (int *)palloc(sizeof(int) * in->nNodes);
    for (i = 0; i < in->nNodes; i++)
    {
      out->nodeNumbers[i] = i;
    }

    if (in->norderbys > 0 && in->nNodes > 0)
    {
      double *distances = palloc(sizeof(double) * in->norderbys);
      int j;

      for (j = 0; j < in->norderbys; j++)
      {
        Point *pt = DatumGetPointP(in->orderbys[j].sk_argument);

        distances[j] = pointToRectBoxDistance(pt, rect_box);
      }

      out->distances = (double **)palloc(sizeof(double *) * in->nNodes);
      out->distances[0] = distances;

      for (i = 1; i < in->nNodes; i++)
      {
        out->distances[i] = palloc(sizeof(double) * in->norderbys);
        memcpy(out->distances[i], distances, sizeof(double) * in->norderbys);
      }
    }

    PG_RETURN_VOID();
  }

     
                                                                         
                           
     
  centroid = getRangeBox(DatumGetBoxP(in->prefixDatum));
  queries = (RangeBox **)palloc(in->nkeys * sizeof(RangeBox *));
  for (i = 0; i < in->nkeys; i++)
  {
    BOX *box = spg_box_quad_get_scankey_bbox(&in->scankeys[i], NULL);

    queries[i] = getRangeBox(box);
  }

                                        
  out->nNodes = 0;
  out->nodeNumbers = (int *)palloc(sizeof(int) * in->nNodes);
  out->traversalValues = (void **)palloc(sizeof(void *) * in->nNodes);
  if (in->norderbys > 0)
  {
    out->distances = (double **)palloc(sizeof(double *) * in->nNodes);
  }

     
                                                                          
                                                                         
                                    
     
  old_ctx = MemoryContextSwitchTo(in->traversalMemoryContext);

  for (quadrant = 0; quadrant < in->nNodes; quadrant++)
  {
    RectBox *next_rect_box = nextRectBox(rect_box, centroid, quadrant);
    bool flag = true;

    for (i = 0; i < in->nkeys; i++)
    {
      StrategyNumber strategy = in->scankeys[i].sk_strategy;

      switch (strategy)
      {
      case RTOverlapStrategyNumber:
        flag = overlap4D(next_rect_box, queries[i]);
        break;

      case RTContainsStrategyNumber:
        flag = contain4D(next_rect_box, queries[i]);
        break;

      case RTSameStrategyNumber:
      case RTContainedByStrategyNumber:
        flag = contained4D(next_rect_box, queries[i]);
        break;

      case RTLeftStrategyNumber:
        flag = left4D(next_rect_box, queries[i]);
        break;

      case RTOverLeftStrategyNumber:
        flag = overLeft4D(next_rect_box, queries[i]);
        break;

      case RTRightStrategyNumber:
        flag = right4D(next_rect_box, queries[i]);
        break;

      case RTOverRightStrategyNumber:
        flag = overRight4D(next_rect_box, queries[i]);
        break;

      case RTAboveStrategyNumber:
        flag = above4D(next_rect_box, queries[i]);
        break;

      case RTOverAboveStrategyNumber:
        flag = overAbove4D(next_rect_box, queries[i]);
        break;

      case RTBelowStrategyNumber:
        flag = below4D(next_rect_box, queries[i]);
        break;

      case RTOverBelowStrategyNumber:
        flag = overBelow4D(next_rect_box, queries[i]);
        break;

      default:
        elog(ERROR, "unrecognized strategy: %d", strategy);
      }

                                                             
      if (!flag)
      {
        break;
      }
    }

    if (flag)
    {
      out->traversalValues[out->nNodes] = next_rect_box;
      out->nodeNumbers[out->nNodes] = quadrant;

      if (in->norderbys > 0)
      {
        double *distances = palloc(sizeof(double) * in->norderbys);
        int j;

        out->distances[out->nNodes] = distances;

        for (j = 0; j < in->norderbys; j++)
        {
          Point *pt = DatumGetPointP(in->orderbys[j].sk_argument);

          distances[j] = pointToRectBoxDistance(pt, next_rect_box);
        }
      }

      out->nNodes++;
    }
    else
    {
         
                                                                      
                                                
         
      pfree(next_rect_box);
    }
  }

                   
  MemoryContextSwitchTo(old_ctx);

  PG_RETURN_VOID();
}

   
                                     
   
Datum
spg_box_quad_leaf_consistent(PG_FUNCTION_ARGS)
{
  spgLeafConsistentIn *in = (spgLeafConsistentIn *)PG_GETARG_POINTER(0);
  spgLeafConsistentOut *out = (spgLeafConsistentOut *)PG_GETARG_POINTER(1);
  Datum leaf = in->leafDatum;
  bool flag = true;
  int i;

                            
  out->recheck = false;

     
                                                                          
                                                                             
                               
     
  if (in->returnData)
  {
    out->leafValue = leaf;
  }

                                          
  for (i = 0; i < in->nkeys; i++)
  {
    StrategyNumber strategy = in->scankeys[i].sk_strategy;
    BOX *box = spg_box_quad_get_scankey_bbox(&in->scankeys[i], &out->recheck);
    Datum query = BoxPGetDatum(box);

    switch (strategy)
    {
    case RTOverlapStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_overlap, leaf, query));
      break;

    case RTContainsStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_contain, leaf, query));
      break;

    case RTContainedByStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_contained, leaf, query));
      break;

    case RTSameStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_same, leaf, query));
      break;

    case RTLeftStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_left, leaf, query));
      break;

    case RTOverLeftStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_overleft, leaf, query));
      break;

    case RTRightStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_right, leaf, query));
      break;

    case RTOverRightStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_overright, leaf, query));
      break;

    case RTAboveStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_above, leaf, query));
      break;

    case RTOverAboveStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_overabove, leaf, query));
      break;

    case RTBelowStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_below, leaf, query));
      break;

    case RTOverBelowStrategyNumber:
      flag = DatumGetBool(DirectFunctionCall2(box_overbelow, leaf, query));
      break;

    default:
      elog(ERROR, "unrecognized strategy: %d", strategy);
    }

                                                           
    if (!flag)
    {
      break;
    }
  }

  if (flag && in->norderbys > 0)
  {
    Oid distfnoid = in->orderbys[0].sk_func.fn_oid;

    out->distances = spg_key_orderbys_distances(leaf, false, in->orderbys, in->norderbys);

                                                                 
    out->recheckDistances = distfnoid == F_DIST_POLYP;
  }

  PG_RETURN_BOOL(flag);
}

   
                                                                             
                  
   
Datum
spg_bbox_quad_config(PG_FUNCTION_ARGS)
{
  spgConfigOut *cfg = (spgConfigOut *)PG_GETARG_POINTER(1);

  cfg->prefixType = BOXOID;                                             
  cfg->labelType = VOIDOID;                                 
  cfg->leafType = BOXOID;
  cfg->canReturnData = false;
  cfg->longValuesOK = false;

  PG_RETURN_VOID();
}

   
                                          
   
Datum
spg_poly_quad_compress(PG_FUNCTION_ARGS)
{
  POLYGON *polygon = PG_GETARG_POLYGON_P(0);
  BOX *box;

  box = (BOX *)palloc(sizeof(BOX));
  *box = polygon->boundbox;

  PG_RETURN_BOX_P(box);
}
