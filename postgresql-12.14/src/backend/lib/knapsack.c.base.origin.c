/*-------------------------------------------------------------------------
 *
 * knapsack.c
 *	  Knapsack problem solver
 *
 * Given input vectors of integral item weights (must be >= 0) and values
 * (double >= 0), compute the set of items which produces the greatest total
 * value without exceeding a specified total weight; each item is included at
 * most once (this is the 0/1 knapsack problem).  Weight 0 items will always be
 * included.
 *
 * The performance of this algorithm is pseudo-polynomial, O(nW) where W is the
 * weight limit.  To use with non-integral weights or approximate solutions,
 * the caller should pre-scale the input weights to a suitable range.  This
 * allows approximate solutions in polynomial time (the general case of the
 * exact problem is NP-hard).
 *
 * Copyright (c) 2017-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/lib/knapsack.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>
#include <limits.h>

#include "lib/knapsack.h"
#include "miscadmin.h"
#include "nodes/bitmapset.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

/*
 * DiscreteKnapsack
 *
 * The item_values input is optional; if omitted, all the items are assumed to
 * have value 1.
 *
 * Returns a Bitmapset of the 0..(n-1) indexes of the items chosen for
 * inclusion in the solution.
 *
 * This uses the usual dynamic-programming algorithm, adapted to reuse the
 * memory on each pass (by working from larger weights to smaller).  At the
 * start of pass number i, the values[w] array contains the largest value
 * computed with total weight <= w, using only items with indices < i; and
 * sets[w] contains the bitmap of items actually used for that value.  (The
 * bitmapsets are all pre-initialized with an unused high bit so that memory
 * allocation is done only once.)
 */
Bitmapset *
DiscreteKnapsack(int max_weight, int num_items, int *item_weights, double *item_values)
{



















































}