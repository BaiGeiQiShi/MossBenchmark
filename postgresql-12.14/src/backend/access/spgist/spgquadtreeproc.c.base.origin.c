/*-------------------------------------------------------------------------
 *
 * spgquadtreeproc.c
 *	  implementation of quad tree over points for SP-GiST
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/spgist/spgquadtreeproc.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/spgist.h"
#include "access/stratnum.h"
#include "access/spgist_private.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/geo_decls.h"

Datum
spg_quad_config(PG_FUNCTION_ARGS)
{








}

#define SPTEST(f, x, y) DatumGetBool(DirectFunctionCall2(f, PointPGetDatum(x), PointPGetDatum(y)))

/*
 * Determine which quadrant a point falls into, relative to the centroid.
 *
 * Quadrants are identified like this:
 *
 *	 4	|  1
 *	----+-----
 *	 3	|  2
 *
 * Points on one of the axes are taken to lie in the lowest-numbered
 * adjacent quadrant.
 */
static int16
getQuadrant(Point *centroid, Point *tst)
{






















}

/* Returns bounding box of a given quadrant inside given bounding box */
static BOX *
getQuadrantArea(BOX *bbox, Point *centroid, int quadrant)
{



























}

Datum
spg_quad_choose(PG_FUNCTION_ARGS)
{
























}

#ifdef USE_MEDIAN
static int
x_cmp(const void *a, const void *b, void *arg)
{
  Point *pa = *(Point **)a;
  Point *pb = *(Point **)b;

  if (pa->x == pb->x)
  {
    return 0;
  }
  return (pa->x > pb->x) ? 1 : -1;
}

static int
y_cmp(const void *a, const void *b, void *arg)
{
  Point *pa = *(Point **)a;
  Point *pb = *(Point **)b;

  if (pa->y == pb->y)
  {
    return 0;
  }
  return (pa->y > pb->y) ? 1 : -1;
}
#endif

Datum
spg_quad_picksplit(PG_FUNCTION_ARGS)
{






















































}

Datum
spg_quad_inner_consistent(PG_FUNCTION_ARGS)
{



















































































































































































}

Datum
spg_quad_leaf_consistent(PG_FUNCTION_ARGS)
{






























































}