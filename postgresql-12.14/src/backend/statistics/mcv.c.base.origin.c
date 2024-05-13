/*-------------------------------------------------------------------------
 *
 * mcv.c
 *	  POSTGRES multivariate MCV lists
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/statistics/mcv.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "fmgr.h"
#include "funcapi.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/fmgroids.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

/*
 * Computes size of a serialized MCV item, depending on the number of
 * dimensions (columns) the statistic is defined on. The datum values are
 * stored in a separate array (deduplicated, to minimize the size), and
 * so the serialized items only store uint16 indexes into that array.
 *
 * Each serialized item stores (in this order):
 *
 * - indexes to values	  (ndim * sizeof(uint16))
 * - null flags			  (ndim * sizeof(bool))
 * - frequency			  (sizeof(double))
 * - base_frequency		  (sizeof(double))
 *
 * There is no alignment padding within an MCV item.
 * So in total each MCV item requires this many bytes:
 *
 *	 ndim * (sizeof(uint16) + sizeof(bool)) + 2 * sizeof(double)
 */
#define ITEM_SIZE(ndims) ((ndims) * (sizeof(uint16) + sizeof(bool)) + 2 * sizeof(double))

/*
 * Used to compute size of serialized MCV list representation.
 */
#define MinSizeOfMCVList (VARHDRSZ + sizeof(uint32) * 3 + sizeof(AttrNumber))

/*
 * Size of the serialized MCV list, excluding the space needed for
 * deduplicated per-dimension values. The macro is meant to be used
 * when it's not yet safe to access the serialized info about amount
 * of data for each column.
 */
#define SizeOfMCVList(ndims, nitems) ((MinSizeOfMCVList + sizeof(Oid) * (ndims)) + ((ndims) * sizeof(DimensionInfo)) + ((nitems) * ITEM_SIZE(ndims)))

static MultiSortSupport
build_mss(VacAttrStats **stats, int numattrs);

static SortItem *
build_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss, int *ndistinct);

static SortItem **
build_column_frequencies(SortItem *groups, int ngroups, MultiSortSupport mss, int *ncounts);

static int
count_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss);

/*
 * Compute new value for bitmap item, considering whether it's used for
 * clauses connected by AND/OR.
 */
#define RESULT_MERGE(value, is_or, match) ((is_or) ? ((value) || (match)) : ((value) && (match)))

/*
 * When processing a list of clauses, the bitmap item may get set to a value
 * such that additional clauses can't change it. For example, when processing
 * a list of clauses connected to AND, as soon as the item gets set to 'false'
 * then it'll remain like that. Similarly clauses connected by OR and 'true'.
 *
 * Returns true when the value in the bitmap can't change no matter how the
 * remaining clauses are evaluated.
 */
#define RESULT_IS_FINAL(value, is_or) ((is_or) ? (value) : (!(value)))

/*
 * get_mincount_for_mcv_list
 * 		Determine the minimum number of times a value needs to appear in
 * 		the sample for it to be included in the MCV list.
 *
 * We want to keep only values that appear sufficiently often in the
 * sample that it is reasonable to extrapolate their sample frequencies to
 * the entire table.  We do this by placing an upper bound on the relative
 * standard error of the sample frequency, so that any estimates the
 * planner generates from the MCV statistics can be expected to be
 * reasonably accurate.
 *
 * Since we are sampling without replacement, the sample frequency of a
 * particular value is described by a hypergeometric distribution.  A
 * common rule of thumb when estimating errors in this situation is to
 * require at least 10 instances of the value in the sample, in which case
 * the distribution can be approximated by a normal distribution, and
 * standard error analysis techniques can be applied.  Given a sample size
 * of n, a population size of N, and a sample frequency of p=cnt/n, the
 * standard error of the proportion p is given by
 *		SE = sqrt(p*(1-p)/n) * sqrt((N-n)/(N-1))
 * where the second term is the finite population correction.  To get
 * reasonably accurate planner estimates, we impose an upper bound on the
 * relative standard error of 20% -- i.e., SE/p < 0.2.  This 20% relative
 * error bound is fairly arbitrary, but has been found empirically to work
 * well.  Rearranging this formula gives a lower bound on the number of
 * instances of the value seen:
 *		cnt > n*(N-n) / (N-n+0.04*n*(N-1))
 * This bound is at most 25, and approaches 0 as n approaches 0 or N. The
 * case where n approaches 0 cannot happen in practice, since the sample
 * size is at least 300.  The case where n approaches N corresponds to
 * sampling the whole the table, in which case it is reasonable to keep
 * the whole MCV list (have no lower bound), so it makes sense to apply
 * this formula for all inputs, even though the above derivation is
 * technically only valid when the right hand side is at least around 10.
 *
 * An alternative way to look at this formula is as follows -- assume that
 * the number of instances of the value seen scales up to the entire
 * table, so that the population count is K=N*cnt/n. Then the distribution
 * in the sample is a hypergeometric distribution parameterised by N, n
 * and K, and the bound above is mathematically equivalent to demanding
 * that the standard deviation of that distribution is less than 20% of
 * its mean.  Thus the relative errors in any planner estimates produced
 * from the MCV statistics are likely to be not too large.
 */
static double
get_mincount_for_mcv_list(int samplerows, double totalrows)
{














}

/*
 * Builds MCV list from the set of sampled rows.
 *
 * The algorithm is quite simple:
 *
 *	   (1) sort the data (default collation, '<' for the data type)
 *
 *	   (2) count distinct groups, decide how many to keep
 *
 *	   (3) build the MCV list using the threshold determined in (2)
 *
 *	   (4) remove rows represented by the MCV from the sample
 *
 */
MCVList *
statext_mcv_build(int numrows, HeapTuple *rows, Bitmapset *attrs, VacAttrStats **stats, double totalrows)
{































































































































































}

/*
 * build_mss
 *	build MultiSortSupport for the attributes passed in attrs
 */
static MultiSortSupport
build_mss(VacAttrStats **stats, int numattrs)
{





















}

/*
 * count_distinct_groups
 *	count distinct combinations of SortItems in the array
 *
 * The array is assumed to be sorted according to the MultiSortSupport.
 */
static int
count_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss)
{
















}

/*
 * compare_sort_item_count
 *	comparator for sorting items by count (frequencies) in descending order
 */
static int
compare_sort_item_count(const void *a, const void *b)
{













}

/*
 * build_distinct_groups
 *	build an array of SortItems for distinct groups and counts matching
 *items
 *
 * The input array is assumed to be sorted
 */
static SortItem *
build_distinct_groups(int numrows, SortItem *items, MultiSortSupport mss, int *ndistinct)
{
































}

/* compare sort items (single dimension) */
static int
sort_item_compare(const void *a, const void *b, void *arg)
{





}

/*
 * build_column_frequencies
 *	compute frequencies of values in each column
 *
 * This returns an array of SortItems for each attibute the MCV is built
 * on, with a frequency (number of occurrences) for each value. This is
 * then used to compute "base" frequency of MCV items.
 *
 * All the memory is allocated in a single chunk, so that a single pfree
 * is enough to release it. We do not allocate space for values/isnull
 * arrays in the SortItems, because we can simply point into the input
 * groups directly.
 */
static SortItem **
build_column_frequencies(SortItem *groups, int ngroups, MultiSortSupport mss, int *ncounts)
{























































}

/*
 * statext_mcv_load
 *		Load the MCV list for the indicated pg_statistic_ext tuple
 */
MCVList *
statext_mcv_load(Oid mvoid)
{






















}

/*
 * statext_mcv_serialize
 *		Serialize MCV list into a pg_mcv_list value.
 *
 * The MCV items may include values of various data types, and it's reasonable
 * to expect redundancy (values for a given attribute, repeated for multiple
 * MCV list items). So we deduplicate the values into arrays, and then replace
 * the values by indexes into those arrays.
 *
 * The overall structure of the serialized representation looks like this:
 *
 * +---------------+----------------+---------------------+-------+
 * | header fields | dimension info | deduplicated values | items |
 * +---------------+----------------+---------------------+-------+
 *
 * Where dimension info stores information about type of K-th attribute (e.g.
 * typlen, typbyval and length of deduplicated values).  Deduplicated values
 * store deduplicated values for each attribute.  And items store the actual
 * MCV list items, with values replaced by indexes into the arrays.
 *
 * When serializing the items, we use uint16 indexes. The number of MCV items
 * is limited by the statistics target (which is capped to 10k at the moment).
 * We might increase this to 65k and still fit into uint16, so there's a bit of
 * slack. Furthermore, this limit is on the number of distinct values per
 *column, and we usually have few of those (and various combinations of them for
 *the those MCV list). So uint16 seems fine for now.
 *
 * We don't really expect the serialization to save as much space as for
 * histograms, as we are not doing any bucket splits (which is the source
 * of high redundancy in histograms).
 *
 * TODO: Consider packing boolean flags (NULL) for each item into a single char
 * (or a longer type) instead of using an array of bool items.
 */
bytea *
statext_mcv_serialize(MCVList *mcvlist, VacAttrStats **stats)
{















































































































































































































































































































































































}

/*
 * statext_mcv_deserialize
 *		Reads serialized MCV list into MCVList structure.
 *
 * All the memory needed by the MCV list is allocated as a single chunk, so
 * it's possible to simply pfree() it at once.
 */
MCVList *
statext_mcv_deserialize(bytea *data)
{


















































































































































































































































































































































}

/*
 * SRF with details about buckets of a histogram:
 *
 * - item ID (0...nitems)
 * - values (string array)
 * - nulls only (boolean array)
 * - frequency (double precision)
 * - base_frequency (double precision)
 *
 * The input is the OID of the statistics, and there are no rows returned if
 * the statistics contains no histogram.
 */
Datum
pg_stats_ext_mcvlist_items(PG_FUNCTION_ARGS)
{

















































































































}

/*
 * pg_mcv_list_in		- input routine for type pg_mcv_list.
 *
 * pg_mcv_list is real enough to be a table column, but it has no operations
 * of its own, and disallows input too
 */
Datum
pg_mcv_list_in(PG_FUNCTION_ARGS)
{







}

/*
 * pg_mcv_list_out		- output routine for type pg_mcv_list.
 *
 * MCV lists are serialized into a bytea value, so we simply call byteaout()
 * to serialize the value into text. But it'd be nice to serialize that into
 * a meaningful representation (e.g. for inspection by people).
 *
 * XXX This should probably return something meaningful, similar to what
 * pg_dependencies_out does. Not sure how to deal with the deduplicated
 * values, though - do we want to expand that or not?
 */
Datum
pg_mcv_list_out(PG_FUNCTION_ARGS)
{

}

/*
 * pg_mcv_list_recv		- binary input routine for type pg_mcv_list.
 */
Datum
pg_mcv_list_recv(PG_FUNCTION_ARGS)
{



}

/*
 * pg_mcv_list_send		- binary output routine for type pg_mcv_list.
 *
 * MCV lists are serialized in a bytea value (although the type is named
 * differently), so let's just send that.
 */
Datum
pg_mcv_list_send(PG_FUNCTION_ARGS)
{

}

/*
 * mcv_get_match_bitmap
 *	Evaluate clauses using the MCV list, and update the match bitmap.
 *
 * A match bitmap keeps match/mismatch status for each MCV item, and we
 * update it based on additional clauses. We also use it to skip items
 * that can't possibly match (e.g. item marked as "mismatch" can't change
 * to "match" when evaluating AND clause list).
 *
 * The function also returns a flag indicating whether there was an
 * equality condition for all attributes, the minimum frequency in the MCV
 * list, and a total MCV frequency (sum of frequencies for all items).
 *
 * XXX Currently the match bitmap uses a bool for each MCV item, which is
 * somewhat wasteful as we could do with just a single bit, thus reducing
 * the size to ~1/8. It would also allow us to combine bitmaps simply using
 * & and |, which should be faster than min/max. The bitmaps are fairly
 * small, though (thanks to the cap on the MCV list size).
 */
static bool *
mcv_get_match_bitmap(PlannerInfo *root, List *clauses, Bitmapset *keys, MCVList *mcvlist, bool is_or)
{
















































































































































































































































}

/*
 * mcv_clauselist_selectivity
 *		Return the selectivity estimate computed using an MCV list.
 *
 * First builds a bitmap of MCV items matching the clauses, and then sums
 * the frequencies of matching items.
 *
 * It also produces two additional interesting selectivities - total
 * selectivity of all the MCV items (not just the matching ones), and the
 * base frequency computed on the assumption of independence.
 */
Selectivity
mcv_clauselist_selectivity(PlannerInfo *root, StatisticExtInfo *stat, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Selectivity *basesel, Selectivity *totalsel)
{





























}