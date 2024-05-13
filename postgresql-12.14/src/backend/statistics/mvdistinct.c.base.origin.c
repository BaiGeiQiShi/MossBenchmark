/*-------------------------------------------------------------------------
 *
 * mvdistinct.c
 *	  POSTGRES multivariate ndistinct coefficients
 *
 * Estimating number of groups in a combination of columns (e.g. for GROUP BY)
 * is tricky, and the estimation error is often significant.

 * The multivariate ndistinct coefficients address this by storing ndistinct
 * estimates for combinations of the user-specified columns.  So for example
 * given a statistics object on three columns (a,b,c), this module estimates
 * and stores n-distinct for (a,b), (a,c), (b,c) and (a,b,c).  The per-column
 * estimates are already available in pg_statistic.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/statistics/mvdistinct.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "lib/stringinfo.h"
#include "utils/syscache.h"
#include "utils/typcache.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"

static double
ndistinct_for_combination(double totalrows, int numrows, HeapTuple *rows, VacAttrStats **stats, int k, int *combination);
static double
estimate_ndistinct(double totalrows, int numrows, int d, int f1);
static int
n_choose_k(int n, int k);
static int
num_combinations(int n);

/* size of the struct header fields (magic, type, nitems) */
#define SizeOfHeader (3 * sizeof(uint32))

/* size of a serialized ndistinct item (coefficient, natts, atts) */
#define SizeOfItem(natts) (sizeof(double) + sizeof(int) + (natts) * sizeof(AttrNumber))

/* minimal size of a ndistinct item (with two attributes) */
#define MinSizeOfItem SizeOfItem(2)

/* minimal size of mvndistinct, when all items are minimal */
#define MinSizeOfItems(nitems) (SizeOfHeader + (nitems) * MinSizeOfItem)

/* Combination generator API */

/* internal state for generator of k-combinations of n elements */
typedef struct CombinationGenerator
{
  int k;             /* size of the combination */
  int n;             /* total number of elements */
  int current;       /* index of the next combination to return */
  int ncombinations; /* number of combinations (size of array) */
  int *combinations; /* array of pre-built combinations */
} CombinationGenerator;

static CombinationGenerator *
generator_init(int n, int k);
static void
generator_free(CombinationGenerator *state);
static int *
generator_next(CombinationGenerator *state);
static void
generate_combinations(CombinationGenerator *state);

/*
 * statext_ndistinct_build
 *		Compute ndistinct coefficient for the combination of attributes.
 *
 * This computes the ndistinct estimate using the same estimator used
 * in analyze.c and then computes the coefficient.
 */
MVNDistinct *
statext_ndistinct_build(double totalrows, int numrows, HeapTuple *rows, Bitmapset *attrs, VacAttrStats **stats)
{











































}

/*
 * statext_ndistinct_load
 *		Load the ndistinct value for the indicated pg_statistic_ext
 *tuple
 */
MVNDistinct *
statext_ndistinct_load(Oid mvoid)
{






















}

/*
 * statext_ndistinct_serialize
 *		serialize ndistinct to the on-disk bytea format
 */
bytea *
statext_ndistinct_serialize(MVNDistinct *ndistinct)
{





































































}

/*
 * statext_ndistinct_deserialize
 *		Read an on-disk bytea format MVNDistinct to in-memory format
 */
MVNDistinct *
statext_ndistinct_deserialize(bytea *data)
{


























































































}

/*
 * pg_ndistinct_in
 *		input routine for type pg_ndistinct
 *
 * pg_ndistinct is real enough to be a table column, but it has no
 * operations of its own, and disallows input (jus like pg_node_tree).
 */
Datum
pg_ndistinct_in(PG_FUNCTION_ARGS)
{



}

/*
 * pg_ndistinct
 *		output routine for type pg_ndistinct
 *
 * Produces a human-readable representation of the value.
 */
Datum
pg_ndistinct_out(PG_FUNCTION_ARGS)
{






























}

/*
 * pg_ndistinct_recv
 *		binary input routine for type pg_ndistinct
 */
Datum
pg_ndistinct_recv(PG_FUNCTION_ARGS)
{



}

/*
 * pg_ndistinct_send
 *		binary output routine for type pg_ndistinct
 *
 * n-distinct is serialized into a bytea value, so let's send that.
 */
Datum
pg_ndistinct_send(PG_FUNCTION_ARGS)
{

}

/*
 * ndistinct_for_combination
 *		Estimates number of distinct values in a combination of columns.
 *
 * This uses the same ndistinct estimator as compute_scalar_stats() in
 * ANALYZE, i.e.,
 *		n*d / (n - f1 + f1*n/N)
 *
 * except that instead of values in a single column we are dealing with
 * combination of multiple columns.
 */
static double
ndistinct_for_combination(double totalrows, int numrows, HeapTuple *rows, VacAttrStats **stats, int k, int *combination)
{



















































































}

/* The Duj1 estimator (already used in analyze.c). */
static double
estimate_ndistinct(double totalrows, int numrows, int d, int f1)
{




















}

/*
 * n_choose_k
 *		computes binomial coefficients using an algorithm that is both
 *		efficient and prevents overflows
 */
static int
n_choose_k(int n, int k)
{















}

/*
 * num_combinations
 *		number of combinations, excluding single-value combinations
 */
static int
num_combinations(int n)
{











}

/*
 * generator_init
 *		initialize the generator of combinations
 *
 * The generator produces combinations of K elements in the interval (0..N).
 * We prebuild all the combinations in this method, which is simpler than
 * generating them on the fly.
 */
static CombinationGenerator *
generator_init(int n, int k)
{


























}

/*
 * generator_next
 *		returns the next combination from the prebuilt list
 *
 * Returns a combination of K array indexes (0 .. N), as specified to
 * generator_init), or NULL when there are no more combination.
 */
static int *
generator_next(CombinationGenerator *state)
{






}

/*
 * generator_free
 *		free the internal state of the generator
 *
 * Releases the generator internal state (pre-built combinations).
 */
static void
generator_free(CombinationGenerator *state)
{


}

/*
 * generate_combinations_recurse
 *		given a prefix, generate all possible combinations
 *
 * Given a prefix (first few elements of the combination), generate following
 * elements recursively. We generate the combinations in lexicographic order,
 * which eliminates permutations of the same combination.
 */
static void
generate_combinations_recurse(CombinationGenerator *state, int index, int start, int *current)
{
























}

/*
 * generate_combinations
 *		generate all k-combinations of N elements
 */
static void
generate_combinations(CombinationGenerator *state)
{





}