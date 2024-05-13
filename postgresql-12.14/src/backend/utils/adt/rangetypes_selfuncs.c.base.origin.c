/*-------------------------------------------------------------------------
 *
 * rangetypes_selfuncs.c
 *	  Functions for selectivity estimation of range operators
 *
 * Estimates are based on histograms of lower and upper bounds, and the
 * fraction of empty ranges.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/rangetypes_selfuncs.c
 *
 *-------------------------------------------------------------------------
 */
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

/*
 * Returns a default selectivity estimate for given operator, when we don't
 * have statistics or cannot use them for some reason.
 */
static double
default_range_selectivity(Oid operator)
{

































}

/*
 * rangesel -- restriction selectivity for range operators
 */
Datum
rangesel(PG_FUNCTION_ARGS)
{

























































































































}

static double
calc_rangesel(TypeCacheEntry *typcache, VariableStatData *vardata, RangeType *constval, Oid operator)
{

































































































































}

/*
 * Calculate range operator selectivity using histograms of range bounds.
 *
 * This estimate is for the portion of values that are not empty and not
 * NULL.
 */
static double
calc_hist_selectivity(TypeCacheEntry *typcache, VariableStatData *vardata, RangeType *constval, Oid operator)
{




















































































































































































}

/*
 * Look up the fraction of values less than (or equal, if 'equal' argument
 * is true) a given const in a histogram of range bounds.
 */
static double
calc_hist_selectivity_scalar(TypeCacheEntry *typcache, RangeBound *constbound, RangeBound *hist, int hist_nvalues, bool equal)
{

















}

/*
 * Binary search on an array of range bounds. Returns greatest index of range
 * bound in array which is less(less or equal) than given range bound. If all
 * range bounds in array are greater or equal(greater) than given range bound,
 * return -1. When "equal" flag is set conditions in brackets are used.
 *
 * This function is used in scalar operator selectivity estimation. Another
 * goal of this function is to find a histogram bin where to stop
 * interpolation of portion of bounds which are less than or equal to given
 * bound.
 */
static int
rbound_bsearch(TypeCacheEntry *typcache, RangeBound *value, RangeBound *hist, int hist_length, bool equal)
{

















}

/*
 * Binary search on length histogram. Returns greatest index of range length in
 * histogram which is less than (less than or equal) the given length value. If
 * all lengths in the histogram are greater than (greater than or equal) the
 * given length, returns -1.
 */
static int
length_hist_bsearch(Datum *length_hist_values, int length_hist_nvalues, double value, bool equal)
{



















}

/*
 * Get relative position of value in histogram bin in [0,1] range.
 */
static float8
get_position(TypeCacheEntry *typcache, RangeBound *value, RangeBound *hist1, RangeBound *hist2)
{






































































}

/*
 * Get relative position of value in a length histogram bin in [0,1] range.
 */
static double
get_len_position(double value, double hist1, double hist2)
{







































}

/*
 * Measure distance between two range bounds.
 */
static float8
get_distance(TypeCacheEntry *typcache, RangeBound *bound1, RangeBound *bound2)
{













































}

/*
 * Calculate the average of function P(x), in the interval [length1, length2],
 * where P(x) is the fraction of tuples with length < x (or length <= x if
 * 'equal' is true).
 */
static double
calc_length_hist_frac(Datum *length_hist_values, int length_hist_nvalues, double length1, double length2, bool equal)
{







































































































































































}

/*
 * Calculate selectivity of "var <@ const" operator, ie. estimate the fraction
 * of ranges that fall within the constant lower and upper bounds. This uses
 * the histograms of range lower bounds and range lengths, on the assumption
 * that the range lengths are independent of the lower bounds.
 *
 * The caller has already checked that constant lower and upper bounds are
 * finite.
 */
static double
calc_hist_selectivity_contained(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, RangeBound *hist_lower, int hist_nvalues, Datum *length_hist_values, int length_hist_nvalues)
{












































































































}

/*
 * Calculate selectivity of "var @> const" operator, ie. estimate the fraction
 * of ranges that contain the constant lower and upper bounds. This uses
 * the histograms of range lower bounds and range lengths, on the assumption
 * that the range lengths are independent of the lower bounds.
 */
static double
calc_hist_selectivity_contains(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, RangeBound *hist_lower, int hist_nvalues, Datum *length_hist_values, int length_hist_nvalues)
{










































































}