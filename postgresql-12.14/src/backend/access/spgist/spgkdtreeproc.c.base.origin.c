/*-------------------------------------------------------------------------
 *
 * spgkdtreeproc.c
 *	  implementation of k-d tree over points for SP-GiST
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/spgist/spgkdtreeproc.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/spgist.h"
#include "access/spgist_private.h"
#include "access/stratnum.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/geo_decls.h"

Datum
spg_kd_config(PG_FUNCTION_ARGS)
{








}

static int
getSide(double coord, bool isX, Point *tst)
{














}

Datum
spg_kd_choose(PG_FUNCTION_ARGS)
{





















}

typedef struct SortedPoint
{
  Point *p;
  int i;
} SortedPoint;

static int
x_cmp(const void *a, const void *b)
{








}

static int
y_cmp(const void *a, const void *b)
{








}

Datum
spg_kd_picksplit(PG_FUNCTION_ARGS)
{














































}

Datum
spg_kd_inner_consistent(PG_FUNCTION_ARGS)
{















































































































































































































}

/*
 * spg_kd_leaf_consistent() is the same as spg_quad_leaf_consistent(),
 * since we support the same operators and the same leaf data type.
 * So we just borrow that function.
 */