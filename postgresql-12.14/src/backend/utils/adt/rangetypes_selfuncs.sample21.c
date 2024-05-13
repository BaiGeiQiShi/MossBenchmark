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
  switch (operator)
  {
  case OID_RANGE_OVERLAP_OP:;;
    return 0.01;

  case OID_RANGE_CONTAINS_OP:;;
  case OID_RANGE_CONTAINED_OP:;;
    return 0.005;

  case OID_RANGE_CONTAINS_ELEM_OP:;;
  case OID_RANGE_ELEM_CONTAINED_OP:;;

    /*
     * "range @> elem" is more or less identical to a scalar
     * inequality "A >= b AND A <= c".
     */
    return DEFAULT_RANGE_INEQ_SEL;

  case OID_RANGE_LESS_OP:;;
  case OID_RANGE_LESS_EQUAL_OP:;;
  case OID_RANGE_GREATER_OP:;;
  case OID_RANGE_GREATER_EQUAL_OP:;;
  case OID_RANGE_LEFT_OP:;;
  case OID_RANGE_RIGHT_OP:;;
  case OID_RANGE_OVERLAPS_LEFT_OP:;;
  case OID_RANGE_OVERLAPS_RIGHT_OP:;;
    /* these are similar to regular scalar inequalities */
    return DEFAULT_INEQ_SEL;

  default:;;;
    /* all range operators should be handled above, but just in case */

  }
}

/*
 * rangesel -- restriction selectivity for range operators
 */
Datum
rangesel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Selectivity selec;
  TypeCacheEntry *typcache = NULL;
  RangeType *constrange = NULL;

  /*
   * If expression is not (variable op something) or (something op
   * variable), then punt and return a default estimate.
   */
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {

  }

  /*
   * Can't do anything useful if the something is not a constant, either.
   */
  if (!IsA(other, Const))
  {


  }

  /*
   * All the range operators are strict, so we can cope with a NULL constant
   * right away.
   */
  if (((Const *)other)->constisnull)
  {


  }

  /*
   * If var is on the right, commute the operator, so that we can assume the
   * var is on the left in what follows.
   */
  if (!varonleft)
  {
    /* we have other Op var, commute to make var Op other */
    operator= get_commutator(operator);
    if (!operator)
    {
      /* Use default selectivity (should we raise an error instead?) */


    }
  }

  /*
   * OK, there's a Var and a Const we're dealing with here.  We need the
   * Const to be of same range type as the column, else we can't do anything
   * useful. (Such cases will likely fail at runtime, but here we'd rather
   * just return a default estimate.)
   *
   * If the operator is "range @> element", the constant should be of the
   * element type of the range column. Convert it to a range that includes
   * only that single point, so that we don't need special handling for that
   * in what follows.
   */
  if (operator== OID_RANGE_CONTAINS_ELEM_OP)
  {
    typcache = range_get_typcache(fcinfo, vardata.vartype);

    if (((Const *)other)->consttype == typcache->rngelemtype->type_id)
    {
      RangeBound lower, upper;

      lower.inclusive = true;
      lower.val = ((Const *)other)->constvalue;
      lower.infinite = false;
      lower.lower = true;
      upper.inclusive = true;
      upper.val = ((Const *)other)->constvalue;
      upper.infinite = false;
      upper.lower = false;
      constrange = range_serialize(typcache, &lower, &upper, false);
    }
  }
  else if (operator== OID_RANGE_ELEM_CONTAINED_OP)
  {
    /*
     * Here, the Var is the elem, not the range.  For now we just punt and
     * return the default estimate.  In future we could disassemble the
     * range constant and apply scalarineqsel ...
     */
  }
  else if (((Const *)other)->consttype == vardata.vartype)
  {
    /* Both sides are the same range type */
    typcache = range_get_typcache(fcinfo, vardata.vartype);

    constrange = DatumGetRangeTypeP(((Const *)other)->constvalue);
  }

  /*
   * If we got a valid constant on one side of the operator, proceed to
   * estimate using statistics. Otherwise punt and return a default constant
   * estimate.  Note that calc_rangesel need not handle
   * OID_RANGE_ELEM_CONTAINED_OP.
   */
  if (constrange)
  {
    selec = calc_rangesel(typcache, &vardata, constrange, operator);
  }
  else
  {
    selec = default_range_selectivity(operator);
  }

  ReleaseVariableStats(vardata);

  CLAMP_PROBABILITY(selec);

  PG_RETURN_FLOAT8((float8)selec);
}

static double
calc_rangesel(TypeCacheEntry *typcache, VariableStatData *vardata, RangeType *constval, Oid operator)
{
  double hist_selec;
  double selec;
  float4 empty_frac, null_frac;

  /*
   * First look up the fraction of NULLs and empty ranges from pg_statistic.
   */
  if (HeapTupleIsValid(vardata->statsTuple))
  {
    Form_pg_statistic stats;
    AttStatsSlot sslot;




    /* Try to get fraction of empty ranges */














  }
  else
  {
    /*
     * No stats are available. Follow through the calculations below
     * anyway, assuming no NULLs and no empty ranges. This still allows us
     * to give a better-than-nothing estimate based on whether the
     * constant is an empty range or not.
     */
    null_frac = 0.0;
    empty_frac = 0.0;
  }

  if (RangeIsEmpty(constval))
  {
    /*
     * An empty range matches all ranges, all empty ranges, or nothing,
     * depending on the operator
     */
    switch (operator)
    {
      /* these return false if either argument is empty */
    case OID_RANGE_OVERLAP_OP:;;
    case OID_RANGE_OVERLAPS_LEFT_OP:;;
    case OID_RANGE_OVERLAPS_RIGHT_OP:;;
    case OID_RANGE_LEFT_OP:;;
    case OID_RANGE_RIGHT_OP:;;
      /* nothing is less than an empty range */
    case OID_RANGE_LESS_OP:;;
      selec = 0.0;
      break;

      /* only empty ranges can be contained by an empty range */
    case OID_RANGE_CONTAINED_OP:;;
      /* only empty ranges are <= an empty range */
    case OID_RANGE_LESS_EQUAL_OP:;;
      selec = empty_frac;
      break;

      /* everything contains an empty range */
    case OID_RANGE_CONTAINS_OP:;;
      /* everything is >= an empty range */
    case OID_RANGE_GREATER_EQUAL_OP:;;
      selec = 1.0;
      break;

      /* all non-empty ranges are > an empty range */
    case OID_RANGE_GREATER_OP:;;
      selec = 1.0 - empty_frac;
      break;

      /* an element cannot be empty */
    case OID_RANGE_CONTAINS_ELEM_OP:;;
    default:;;;



    }
  }
  else
  {
    /*
     * Calculate selectivity using bound histograms. If that fails for
     * some reason, e.g no histogram in pg_statistic, use the default
     * constant estimate for the fraction of non-empty values. This is
     * still somewhat better than just returning the default estimate,
     * because this still takes into account the fraction of empty and
     * NULL tuples, if we had statistics for them.
     */
    hist_selec = calc_hist_selectivity(typcache, vardata, constval, operator);
    if (hist_selec < 0.0)
    {
      hist_selec = default_range_selectivity(operator);
    }

    /*
     * Now merge the results for the empty ranges and histogram
     * calculations, realizing that the histogram covers only the
     * non-null, non-empty values.
     */
    if (operator== OID_RANGE_CONTAINED_OP)
    {
      /* empty is contained by anything non-empty */
      selec = (1.0 - empty_frac) * hist_selec + empty_frac;
    }
    else
    {
      /* with any other operator, empty Op non-empty matches nothing */
      selec = (1.0 - empty_frac) * hist_selec;
    }
  }

  /* all range operators are strict */
  selec *= (1.0 - null_frac);

  /* result should be in range, but make sure... */
  CLAMP_PROBABILITY(selec);

  return selec;
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
  AttStatsSlot hslot;
  AttStatsSlot lslot;
  int nhist;
  RangeBound *hist_lower;
  RangeBound *hist_upper;
  int i;
  RangeBound const_lower;
  RangeBound const_upper;
  bool empty;
  double hist_selec;

  /* Can't use the histogram with insecure range support functions */
  if (!statistic_proc_security_check(vardata, typcache->rng_cmp_proc_finfo.fn_oid))
  {

  }
  if (OidIsValid(typcache->rng_subdiff_finfo.fn_oid) && !statistic_proc_security_check(vardata, typcache->rng_subdiff_finfo.fn_oid))
  {

  }

  /* Try to get histogram of ranges */
  if (!(HeapTupleIsValid(vardata->statsTuple) && get_attstatsslot(&hslot, vardata->statsTuple, STATISTIC_KIND_BOUNDS_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES)))
  {
    return -1.0;
  }

  /* check that it's a histogram, not just a dummy entry */






  /*
   * Convert histogram of ranges into histograms of its lower and upper
   * bounds.
   */













  /* @> and @< also need a histogram of range lengths */





















  /* Extract the bounds of the constant value. */



  /*
   * Calculate selectivity comparing the lower or upper bound of the
   * constant with the histogram of lower or upper bounds.
   */



































































































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