/*-------------------------------------------------------------------------
 *
 * array_selfuncs.c
 *	  Functions for selectivity estimation of array operators
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/array_selfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "utils/typcache.h"

/* Default selectivity constant for "@>" and "<@" operators */
#define DEFAULT_CONTAIN_SEL 0.005

/* Default selectivity constant for "&&" operator */
#define DEFAULT_OVERLAP_SEL 0.01

/* Default selectivity for given operator */
#define DEFAULT_SEL(operator) ((operator) == OID_ARRAY_OVERLAP_OP ? DEFAULT_OVERLAP_SEL : DEFAULT_CONTAIN_SEL)

static Selectivity
calc_arraycontsel(VariableStatData *vardata, Datum constval, Oid elemtype, Oid operator);
static Selectivity
mcelem_array_selec(ArrayType *array, TypeCacheEntry *typentry, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, float4 *hist, int nhist, Oid operator);
static Selectivity
mcelem_array_contain_overlap_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, Oid operator, TypeCacheEntry * typentry);
static Selectivity
mcelem_array_contained_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, float4 *hist, int nhist, Oid operator, TypeCacheEntry * typentry);
static float *
calc_hist(const float4 *hist, int nhist, int n);
static float *
calc_distr(const float *p, int n, int m, float rest);
static int
floor_log2(uint32 n);
static bool
find_next_mcelem(Datum *mcelem, int nmcelem, Datum value, int *index, TypeCacheEntry *typentry);
static int
element_compare(const void *key1, const void *key2, void *arg);
static int
float_compare_desc(const void *key1, const void *key2);

/*
 * scalararraysel_containment
 *		Estimate selectivity of ScalarArrayOpExpr via array containment.
 *
 * If we have const =/<> ANY/ALL (array_var) then we can estimate the
 * selectivity as though this were an array containment operator,
 * array_var op ARRAY[const].
 *
 * scalararraysel() has already verified that the ScalarArrayOpExpr's operator
 * is the array element type's default equality or inequality operator, and
 * has aggressively simplified both inputs to constants.
 *
 * Returns selectivity (0..1), or -1 if we fail to estimate selectivity.
 */
Selectivity
scalararraysel_containment(PlannerInfo *root, Node *leftop, Node *rightop, Oid elemtype, bool isEquality, bool useOr, int varRelid)
{

































































































































}

/*
 * arraycontsel -- restriction selectivity for array @>, &&, <@ operators
 */
Datum
arraycontsel(PG_FUNCTION_ARGS)
{











































































}

/*
 * arraycontjoinsel -- join selectivity for array @>, &&, <@ operators
 */
Datum
arraycontjoinsel(PG_FUNCTION_ARGS)
{




}

/*
 * Calculate selectivity for "arraycolumn @> const", "arraycolumn && const"
 * or "arraycolumn <@ const" based on the statistics
 *
 * This function is mainly responsible for extracting the pg_statistic data
 * to be used; we then pass the problem on to mcelem_array_selec().
 */
static Selectivity
calc_arraycontsel(VariableStatData *vardata, Datum constval, Oid elemtype, Oid operator)
{






































































}

/*
 * Array selectivity estimation based on most common elements statistics
 *
 * This function just deconstructs and sorts the array constant's contents,
 * and then passes the problem on to mcelem_array_contain_overlap_selec or
 * mcelem_array_contained_selec depending on the operator.
 */
static Selectivity
mcelem_array_selec(ArrayType *array, TypeCacheEntry *typentry, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, float4 *hist, int nhist, Oid operator)
{





























































}

/*
 * Estimate selectivity of "column @> const" and "column && const" based on
 * most common element statistics.  This estimation assumes element
 * occurrences are independent.
 *
 * mcelem (of length nmcelem) and numbers (of length nnumbers) are from
 * the array column's MCELEM statistics slot, or are NULL/0 if stats are
 * not available.  array_data (of length nitems) is the constant's elements.
 *
 * Both the mcelem and array_data arrays are assumed presorted according
 * to the element type's cmpfunc.  Null elements are not present.
 *
 * TODO: this estimate probably could be improved by using the distinct
 * elements count histogram.  For example, excepting the special case of
 * "column @> '{}'", we can multiply the calculated selectivity by the
 * fraction of nonempty arrays in the column.
 */
static Selectivity
mcelem_array_contain_overlap_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, Oid operator, TypeCacheEntry * typentry)
{






























































































































}

/*
 * Estimate selectivity of "column <@ const" based on most common element
 * statistics.
 *
 * mcelem (of length nmcelem) and numbers (of length nnumbers) are from
 * the array column's MCELEM statistics slot, or are NULL/0 if stats are
 * not available.  array_data (of length nitems) is the constant's elements.
 * hist (of length nhist) is from the array column's DECHIST statistics slot,
 * or is NULL/0 if those stats are not available.
 *
 * Both the mcelem and array_data arrays are assumed presorted according
 * to the element type's cmpfunc.  Null elements are not present.
 *
 * Independent element occurrence would imply a particular distribution of
 * distinct element counts among matching rows.  Real data usually falsifies
 * that assumption.  For example, in a set of 11-element integer arrays having
 * elements in the range [0..10], element occurrences are typically not
 * independent.  If they were, a sufficiently-large set would include all
 * distinct element counts 0 through 11.  We correct for this using the
 * histogram of distinct element counts.
 *
 * In the "column @> const" and "column && const" cases, we usually have a
 * "const" with low number of elements (otherwise we have selectivity close
 * to 0 or 1 respectively).  That's why the effect of dependence related
 * to distinct element count distribution is negligible there.  In the
 * "column <@ const" case, number of elements is usually high (otherwise we
 * have selectivity close to 0).  That's why we should do a correction with
 * the array distinct element count distribution here.
 *
 * Using the histogram of distinct element counts produces a different
 * distribution law than independent occurrences of elements.  This
 * distribution law can be described as follows:
 *
 * P(o1, o2, ..., on) = f1^o1 * (1 - f1)^(1 - o1) * f2^o2 *
 *	  (1 - f2)^(1 - o2) * ... * fn^on * (1 - fn)^(1 - on) * hist[m] / ind[m]
 *
 * where:
 * o1, o2, ..., on - occurrences of elements 1, 2, ..., n
 *		(1 - occurrence, 0 - no occurrence) in row
 * f1, f2, ..., fn - frequencies of elements 1, 2, ..., n
 *		(scalar values in [0..1]) according to collected statistics
 * m = o1 + o2 + ... + on = total number of distinct elements in row
 * hist[m] - histogram data for occurrence of m elements.
 * ind[m] - probability of m occurrences from n events assuming their
 *	  probabilities to be equal to frequencies of array elements.
 *
 * ind[m] = sum(f1^o1 * (1 - f1)^(1 - o1) * f2^o2 * (1 - f2)^(1 - o2) *
 * ... * fn^on * (1 - fn)^(1 - on), o1, o2, ..., on) | o1 + o2 + .. on = m
 */
static Selectivity
mcelem_array_contained_selec(Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers, Datum *array_data, int nitems, float4 *hist, int nhist, Oid operator, TypeCacheEntry * typentry)
{









































































































































































































}

/*
 * Calculate the first n distinct element count probabilities from a
 * histogram of distinct element counts.
 *
 * Returns a palloc'd array of n+1 entries, with array[k] being the
 * probability of element count k, k in [0..n].
 *
 * We assume that a histogram box with bounds a and b gives 1 / ((b - a + 1) *
 * (nhist - 1)) probability to each value in (a,b) and an additional half of
 * that to a and b themselves.
 */
static float *
calc_hist(const float4 *hist, int nhist, int n)
{














































































}

/*
 * Consider n independent events with probabilities p[].  This function
 * calculates probabilities of exact k of events occurrence for k in [0..m].
 * Returns a palloc'd array of size m+1.
 *
 * "rest" is the sum of the probabilities of all low-probability events not
 * included in p.
 *
 * Imagine matrix M of size (n + 1) x (m + 1).  Element M[i,j] denotes the
 * probability that exactly j of first i events occur.  Obviously M[0,0] = 1.
 * For any constant j, each increment of i increases the probability iff the
 * event occurs.  So, by the law of total probability:
 *	M[i,j] = M[i - 1, j] * (1 - p[i]) + M[i - 1, j - 1] * p[i]
 *		for i > 0, j > 0.
 *	M[i,0] = M[i - 1, 0] * (1 - p[i]) for i > 0.
 */
static float *
calc_distr(const float *p, int n, int m, float rest)
{














































































}

/* Fast function for floor value of 2 based logarithm calculation. */
static int
floor_log2(uint32 n)
{































}

/*
 * find_next_mcelem binary-searches a most common elements array, starting
 * from *index, for the first member >= value.  It saves the position of the
 * match into *index and returns true if it's an exact match.  (Note: we
 * assume the mcelem elements are distinct so there can't be more than one
 * exact match.)
 */
static bool
find_next_mcelem(Datum *mcelem, int nmcelem, Datum value, int *index, TypeCacheEntry *typentry)
{






















}

/*
 * Comparison function for elements.
 *
 * We use the element type's default btree opclass, and its default collation
 * if the type is collation-sensitive.
 *
 * XXX consider using SortSupport infrastructure
 */
static int
element_compare(const void *key1, const void *key2, void *arg)
{








}

/*
 * Comparison function for sorting floats into descending order.
 */
static int
float_compare_desc(const void *key1, const void *key2)
{















}