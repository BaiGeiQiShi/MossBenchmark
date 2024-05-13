/*-------------------------------------------------------------------------
 *
 * spgproc.c
 *	  Common supporting procedures for SP-GiST opclasses.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/spgist/spgproc.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "access/spgist_private.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/geo_decls.h"

#define point_point_distance(p1, p2) DatumGetFloat8(DirectFunctionCall2(point_distance, PointPGetDatum(p1), PointPGetDatum(p2)))

/* Point-box distance in the assumption that box is aligned by axis */
static double
point_box_distance(Point *point, BOX *box)
{


































}

/*
 * Returns distances from given key to array of ordering scan keys.  Leaf key
 * is expected to be point, non-leaf key is expected to be box.  Scan key
 * arguments are expected to be points.
 */
double *
spg_key_orderbys_distances(Datum key, bool isLeaf, ScanKey orderbys, int norderbys)
{











}

BOX *
box_copy(BOX *orig)
{




}