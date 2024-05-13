/*-------------------------------------------------------------------------
 *
 * network_selfuncs.c
 *	  Functions for selectivity estimation of inet/cidr operators
 *
 * This module provides estimators for the subnet inclusion and overlap
 * operators.  Estimates are based on null fraction, most common values,
 * and histogram of inet/cidr columns.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/network_selfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "utils/builtins.h"
#include "utils/inet.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"

/* Default selectivity for the inet overlap operator */
#define DEFAULT_OVERLAP_SEL 0.01

/* Default selectivity for the various inclusion operators */
#define DEFAULT_INCLUSION_SEL 0.005

/* Default selectivity for specified operator */
#define DEFAULT_SEL(operator) ((operator) == OID_INET_OVERLAP_OP ? DEFAULT_OVERLAP_SEL : DEFAULT_INCLUSION_SEL)

/* Maximum number of items to consider in join selectivity calculations */
#define MAX_CONSIDERED_ELEMS 1024

static Selectivity
networkjoinsel_inner(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2);
static Selectivity
networkjoinsel_semi(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2);
static Selectivity
mcv_population(float4 *mcv_numbers, int mcv_nvalues);
static Selectivity
inet_hist_value_sel(Datum *values, int nvalues, Datum constvalue, int opr_codenum);
static Selectivity
inet_mcv_join_sel(Datum *mcv1_values, float4 *mcv1_numbers, int mcv1_nvalues, Datum *mcv2_values, float4 *mcv2_numbers, int mcv2_nvalues, Oid operator);
static Selectivity
inet_mcv_hist_sel(Datum *mcv_values, float4 *mcv_numbers, int mcv_nvalues, Datum *hist_values, int hist_nvalues, int opr_codenum);
static Selectivity
inet_hist_inclusion_join_sel(Datum *hist1_values, int hist1_nvalues, Datum *hist2_values, int hist2_nvalues, int opr_codenum);
static Selectivity
inet_semi_join_sel(Datum lhs_value, bool mcv_exists, Datum *mcv_values, int mcv_nvalues, bool hist_exists, Datum *hist_values, int hist_nvalues, double hist_weight, FmgrInfo *proc, int opr_codenum);
static int inet_opr_codenum(Oid
    operator);
static int
inet_inclusion_cmp(inet *left, inet *right, int opr_codenum);
static int
inet_masklen_inclusion_cmp(inet *left, inet *right, int opr_codenum);
static int
inet_hist_match_divider(inet *boundary, inet *query, int opr_codenum);

/*
 * Selectivity estimation for the subnet inclusion/overlap operators
 */
Datum
networksel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Selectivity selec, mcv_selec, non_mcv_selec;
  Datum constvalue;
  Form_pg_statistic stats;
  AttStatsSlot hslot;
  double sumcommon, nullfrac;
  FmgrInfo proc;

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

  /* All of the operators handled here are strict. */
  if (((Const *)other)->constisnull)
  {


  }
  constvalue = ((Const *)other)->constvalue;

  /* Otherwise, we need stats in order to produce a non-default estimate. */
  if (!HeapTupleIsValid(vardata.statsTuple))
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(DEFAULT_SEL(operator));
  }




  /*
   * If we have most-common-values info, add up the fractions of the MCV
   * entries that satisfy MCV OP CONST.  These fractions contribute directly
   * to the result selectivity.  Also add up the total fraction represented
   * by MCV entries.
   */



  /*
   * If we have a histogram, use it to estimate the proportion of the
   * non-MCV population that satisfies the clause.  If we don't, apply the
   * default selectivity to that population.
   */


















  /* Combine selectivities for MCV and non-MCV populations */


  /* Result should be in range, but make sure... */





}

/*
 * Join selectivity estimation for the subnet inclusion/overlap operators
 *
 * This function has the same structure as eqjoinsel() in selfuncs.c.
 *
 * Throughout networkjoinsel and its subroutines, we have a performance issue
 * in that the amount of work to be done is O(N^2) in the length of the MCV
 * and histogram arrays.  To keep the runtime from getting out of hand when
 * large statistics targets have been set, we arbitrarily limit the number of
 * values considered to 1024 (MAX_CONSIDERED_ELEMS).  For the MCV arrays, this
 * is easy: just consider at most the first N elements.  (Since the MCVs are
 * sorted by decreasing frequency, this correctly gets us the first N MCVs.)
 * For the histogram arrays, we decimate; that is consider only every k'th
 * element, where k is chosen so that no more than MAX_CONSIDERED_ELEMS
 * elements are considered.  This should still give us a good random sample of
 * the non-MCV population.  Decimation is done on-the-fly in the loops that
 * iterate over the histogram arrays.
 */
Datum
networkjoinsel(PG_FUNCTION_ARGS)
{



















































}

/*
 * Inner join selectivity estimation for subnet inclusion/overlap operators
 *
 * Calculates MCV vs MCV, MCV vs histogram and histogram vs histogram
 * selectivity for join using the subnet inclusion operators.  Unlike the
 * join selectivity function for the equality operator, eqjoinsel_inner(),
 * one to one matching of the values is not enough.  Network inclusion
 * operators are likely to match many to many, so we must check all pairs.
 * (Note: it might be possible to exploit understanding of the histogram's
 * btree ordering to reduce the work needed, but we don't currently try.)
 * Also, MCV vs histogram selectivity is not neglected as in eqjoinsel_inner().
 */
static Selectivity
networkjoinsel_inner(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2)
{




































































































}

/*
 * Semi join selectivity estimation for subnet inclusion/overlap operators
 *
 * Calculates MCV vs MCV, MCV vs histogram, histogram vs MCV, and histogram vs
 * histogram selectivity for semi/anti join cases.
 */
static Selectivity
networkjoinsel_semi(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2)
{





















































































































}

/*
 * Compute the fraction of a relation's population that is represented
 * by the MCV list.
 */
static Selectivity
mcv_population(float4 *mcv_numbers, int mcv_nvalues)
{









}

/*
 * Inet histogram vs single value selectivity estimation
 *
 * Estimate the fraction of the histogram population that satisfies
 * "value OPR CONST".  (The result needs to be scaled to reflect the
 * proportion of the total population represented by the histogram.)
 *
 * The histogram is originally for the inet btree comparison operators.
 * Only the common bits of the network part and the length of the network part
 * (masklen) are interesting for the subnet inclusion operators.  Fortunately,
 * btree comparison treats the network part as the major sort key.  Even so,
 * the length of the network part would not really be significant in the
 * histogram.  This would lead to big mistakes for data sets with uneven
 * masklen distribution.  To reduce this problem, comparisons with the left
 * and the right sides of the buckets are used together.
 *
 * Histogram bucket matches are calculated in two forms.  If the constant
 * matches both bucket endpoints the bucket is considered as fully matched.
 * The second form is to match the bucket partially; we recognize this when
 * the constant matches just one endpoint, or the two endpoints fall on
 * opposite sides of the constant.  (Note that when the constant matches an
 * interior histogram element, it gets credit for partial matches to the
 * buckets on both sides, while a match to a histogram endpoint gets credit
 * for only one partial match.  This is desirable.)
 *
 * The divider in the partial bucket match is imagined as the distance
 * between the decisive bits and the common bits of the addresses.  It will
 * be used as a power of two as it is the natural scale for the IP network
 * inclusion.  This partial bucket match divider calculation is an empirical
 * formula and subject to change with more experiment.
 *
 * For a partial match, we try to calculate dividers for both of the
 * boundaries.  If the address family of a boundary value does not match the
 * constant or comparison of the length of the network parts is not correct
 * for the operator, the divider for that boundary will not be taken into
 * account.  If both of the dividers are valid, the greater one will be used
 * to minimize the mistake in buckets that have disparate masklens.  This
 * calculation is unfair when dividers can be calculated for both of the
 * boundaries but they are far from each other; but it is not a common
 * situation as the boundaries are expected to share most of their significant
 * bits of their masklens.  The mistake would be greater, if we would use the
 * minimum instead of the maximum, and we don't know a sensible way to combine
 * them.
 *
 * For partial match in buckets that have different address families on the
 * left and right sides, only the boundary with the same address family is
 * taken into consideration.  This can cause more mistakes for these buckets
 * if the masklens of their boundaries are also disparate.  But this can only
 * happen in one bucket, since only two address families exist.  It seems a
 * better option than not considering these buckets at all.
 */
static Selectivity
inet_hist_value_sel(Datum *values, int nvalues, Datum constvalue, int opr_codenum)
{





















































}

/*
 * Inet MCV vs MCV join selectivity estimation
 *
 * We simply add up the fractions of the populations that satisfy the clause.
 * The result is exact and does not need to be scaled further.
 */
static Selectivity
inet_mcv_join_sel(Datum *mcv1_values, float4 *mcv1_numbers, int mcv1_nvalues, Datum *mcv2_values, float4 *mcv2_numbers, int mcv2_nvalues, Oid operator)
{

















}

/*
 * Inet MCV vs histogram join selectivity estimation
 *
 * For each MCV on the lefthand side, estimate the fraction of the righthand's
 * histogram population that satisfies the join clause, and add those up,
 * scaling by the MCV's frequency.  The result still needs to be scaled
 * according to the fraction of the righthand's population represented by
 * the histogram.
 */
static Selectivity
inet_mcv_hist_sel(Datum *mcv_values, float4 *mcv_numbers, int mcv_nvalues, Datum *hist_values, int hist_nvalues, int opr_codenum)
{














}

/*
 * Inet histogram vs histogram join selectivity estimation
 *
 * Here, we take all values listed in the second histogram (except for the
 * first and last elements, which are excluded on the grounds of possibly
 * not being very representative) and treat them as a uniform sample of
 * the non-MCV population for that relation.  For each one, we apply
 * inet_hist_value_selec to see what fraction of the first histogram
 * it matches.
 *
 * We could alternatively do this the other way around using the operator's
 * commutator.  XXX would it be worthwhile to do it both ways and take the
 * average?  That would at least avoid non-commutative estimation results.
 */
static Selectivity
inet_hist_inclusion_join_sel(Datum *hist1_values, int hist1_nvalues, Datum *hist2_values, int hist2_nvalues, int opr_codenum)
{



















}

/*
 * Inet semi join selectivity estimation for one value
 *
 * The function calculates the probability that there is at least one row
 * in the RHS table that satisfies the "lhs_value op column" condition.
 * It is used in semi join estimation to check a sample from the left hand
 * side table.
 *
 * The MCV and histogram from the right hand side table should be provided as
 * arguments with the lhs_value from the left hand side table for the join.
 * hist_weight is the total number of rows represented by the histogram.
 * For example, if the table has 1000 rows, and 10% of the rows are in the MCV
 * list, and another 10% are NULLs, hist_weight would be 800.
 *
 * First, the lhs_value will be matched to the most common values.  If it
 * matches any of them, 1.0 will be returned, because then there is surely
 * a match.
 *
 * Otherwise, the histogram will be used to estimate the number of rows in
 * the second table that match the condition.  If the estimate is greater
 * than 1.0, 1.0 will be returned, because it means there is a greater chance
 * that the lhs_value will match more than one row in the table.  If it is
 * between 0.0 and 1.0, it will be returned as the probability.
 */
static Selectivity
inet_semi_join_sel(Datum lhs_value, bool mcv_exists, Datum *mcv_values, int mcv_nvalues, bool hist_exists, Datum *hist_values, int hist_nvalues, double hist_weight, FmgrInfo *proc, int opr_codenum)
{



























}

/*
 * Assign useful code numbers for the subnet inclusion/overlap operators
 *
 * Only inet_masklen_inclusion_cmp() and inet_hist_match_divider() depend
 * on the exact codes assigned here; but many other places in this file
 * know that they can negate a code to obtain the code for the commutator
 * operator.
 */
static int
inet_opr_codenum(Oid operator)
{
















}

/*
 * Comparison function for the subnet inclusion/overlap operators
 *
 * If the comparison is okay for the specified inclusion operator, the return
 * value will be 0.  Otherwise the return value will be less than or greater
 * than 0 as appropriate for the operator.
 *
 * Comparison is compatible with the basic comparison function for the inet
 * type.  See network_cmp_internal() in network.c for the original.  Basic
 * comparison operators are implemented with the network_cmp_internal()
 * function.  It is possible to implement the subnet inclusion operators with
 * this function.
 *
 * Comparison is first on the common bits of the network part, then on the
 * length of the network part (masklen) as in the network_cmp_internal()
 * function.  Only the first part is in this function.  The second part is
 * separated to another function for reusability.  The difference between the
 * second part and the original network_cmp_internal() is that the inclusion
 * operator is considered while comparing the lengths of the network parts.
 * See the inet_masklen_inclusion_cmp() function below.
 */
static int
inet_inclusion_cmp(inet *left, inet *right, int opr_codenum)
{














}

/*
 * Masklen comparison function for the subnet inclusion/overlap operators
 *
 * Compares the lengths of the network parts of the inputs.  If the comparison
 * is okay for the specified inclusion operator, the return value will be 0.
 * Otherwise the return value will be less than or greater than 0 as
 * appropriate for the operator.
 */
static int
inet_masklen_inclusion_cmp(inet *left, inet *right, int opr_codenum)
{



















}

/*
 * Inet histogram partial match divider calculation
 *
 * First the families and the lengths of the network parts are compared using
 * the subnet inclusion operator.  If those are acceptable for the operator,
 * the divider will be calculated using the masklens and the common bits of
 * the addresses.  -1 will be returned if it cannot be calculated.
 *
 * See commentary for inet_hist_value_sel() for some rationale for this.
 */
static int
inet_hist_match_divider(inet *boundary, inet *query, int opr_codenum)
{



































}