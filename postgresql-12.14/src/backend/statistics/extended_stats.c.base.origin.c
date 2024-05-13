/*-------------------------------------------------------------------------
 *
 * extended_stats.c
 *	  POSTGRES extended statistics
 *
 * Generic code supporting statistics objects created via CREATE STATISTICS.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/statistics/extended_stats.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/tuptoaster.h"
#include "catalog/indexing.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "parser/parsetree.h"
#include "postmaster/autovacuum.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"

/*
 * To avoid consuming too much memory during analysis and/or too much space
 * in the resulting pg_statistic rows, we ignore varlena datums that are wider
 * than WIDTH_THRESHOLD (after detoasting!).  This is legitimate for MCV
 * and distinct-value calculations since a wide value is unlikely to be
 * duplicated at all, much less be a most-common value.  For the same reason,
 * ignoring wide values will not affect our estimates of histogram bin
 * boundaries very much.
 */
#define WIDTH_THRESHOLD 1024

/*
 * Used internally to refer to an individual statistics object, i.e.,
 * a pg_statistic_ext entry.
 */
typedef struct StatExtEntry
{
  Oid statOid;        /* OID of pg_statistic_ext entry */
  char *schema;       /* statistics object's schema */
  char *name;         /* statistics object's name */
  Bitmapset *columns; /* attribute numbers covered by the object */
  List *types;        /* 'char' list of enabled statistics kinds */
} StatExtEntry;

static List *
fetch_statentries_for_relation(Relation pg_statext, Oid relid);
static VacAttrStats **
lookup_var_attr_stats(Relation rel, Bitmapset *attrs, int nvacatts, VacAttrStats **vacatts);
static void
statext_store(Oid relid, MVNDistinct *ndistinct, MVDependencies *dependencies, MCVList *mcv, VacAttrStats **stats);

/*
 * Compute requested extended stats, using the rows sampled for the plain
 * (single-column) stats.
 *
 * This fetches a list of stats types from pg_statistic_ext, computes the
 * requested stats, and serializes them back into the catalog.
 */
void
BuildRelationExtStatistics(Relation onerel, double totalrows, int numrows, HeapTuple *rows, int natts, VacAttrStats **vacattrstats)
{







































































}

/*
 * statext_is_kind_built
 *		Is this stat kind built in the given pg_statistic_ext_data
 *tuple?
 */
bool
statext_is_kind_built(HeapTuple htup, char type)
{





















}

/*
 * Return a list (of StatExtEntry) of statistics objects for the given relation.
 */
static List *
fetch_statentries_for_relation(Relation pg_statext, Oid relid)
{






















































}

/*
 * Using 'vacatts' of size 'nvacatts' as input data, return a newly built
 * VacAttrStats array which includes only the items corresponding to
 * attributes indicated by 'stxkeys'. If we don't have all of the per column
 * stats available to compute the extended stats, then we return NULL to
 * indicate to the caller that the stats should not be built.
 */
static VacAttrStats **
lookup_var_attr_stats(Relation rel, Bitmapset *attrs, int nvacatts, VacAttrStats **vacatts)
{










































}

/*
 * statext_store
 *	Serializes the statistics and stores them into the pg_statistic_ext_data
 *	tuple.
 */
static void
statext_store(Oid statOid, MVNDistinct *ndistinct, MVDependencies *dependencies, MCVList *mcv, VacAttrStats **stats)
{



























































}

/* initialize multi-dimensional sort */
MultiSortSupport
multi_sort_init(int ndims)
{









}

/*
 * Prepare sort support info using the given sort operator and collation
 * at the position 'sortdim'
 */
void
multi_sort_add_dimension(MultiSortSupport mss, int sortdim, Oid oper, Oid collation)
{







}

/* compare all the dimensions in the selected order */
int
multi_sort_compare(const void *a, const void *b, void *arg)
{



















}

/* compare selected dimension */
int
multi_sort_compare_dim(int dim, const SortItem *a, const SortItem *b, MultiSortSupport mss)
{

}

int
multi_sort_compare_dims(int start, int end, const SortItem *a, const SortItem *b, MultiSortSupport mss)
{













}

int
compare_scalars_simple(const void *a, const void *b, void *arg)
{

}

int
compare_datums_simple(Datum a, Datum b, SortSupport ssup)
{

}

/* simple counterpart to qsort_arg */
void *
bsearch_arg(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)
{



























}

/*
 * build_attnums_array
 *		Transforms a bitmap into an array of AttrNumber values.
 *
 * This is used for extended statistics only, so all the attribute must be
 * user-defined. That means offsetting by FirstLowInvalidHeapAttributeNumber
 * is not necessary here (and when querying the bitmap).
 */
AttrNumber *
build_attnums_array(Bitmapset *attrs, int *numattrs)
{































}

/*
 * build_sorted_items
 *		build a sorted array of SortItem with values from rows
 *
 * Note: All the memory is allocated in a single chunk, so that the caller
 * can simply pfree the return value to release all of it.
 */
SortItem *
build_sorted_items(int numrows, int *nitems, HeapTuple *rows, TupleDesc tdesc, MultiSortSupport mss, int numattrs, AttrNumber *attnums)
{




























































































}

/*
 * has_stats_of_kind
 *		Check whether the list contains statistic of a given kind
 */
bool
has_stats_of_kind(List *stats, char requiredkind)
{













}

/*
 * choose_best_statistics
 *		Look for and return statistics with the specified 'requiredkind'
 *which have keys that match at least two of the given attnums.  Return NULL if
 *		there's no match.
 *
 * The current selection criteria is very simple - we choose the statistics
 * object referencing the most attributes in covered (and still unestimated
 * clauses), breaking ties in favor of objects with fewer keys overall.
 *
 * The clause_attnums is an array of bitmaps, storing attnums for individual
 * clauses. A NULL element means the clause is either incompatible or already
 * estimated.
 *
 * XXX If multiple statistics objects tie on both criteria, then which object
 * is chosen depends on the order that they appear in the stats list. Perhaps
 * further tiebreakers are needed.
 */
StatisticExtInfo *
choose_best_statistics(List *stats, char requiredkind, Bitmapset **clause_attnums, int nclauses)
{































































}

/*
 * statext_is_compatible_clause_internal
 *		Determines if the clause is compatible with MCV lists.
 *
 * Does the heavy lifting of actually inspecting the clauses for
 * statext_is_compatible_clause. It needs to be split like this because
 * of recursion.  The attnums bitmap is an input/output parameter collecting
 * attribute numbers from all compatible clauses (recursively).
 */
static bool
statext_is_compatible_clause_internal(PlannerInfo *root, Node *clause, Index relid, Bitmapset **attnums)
{



















































































































































}

/*
 * statext_is_compatible_clause
 *		Determines if the clause is compatible with MCV lists.
 *
 * Currently, we only support three types of clauses:
 *
 * (a) OpExprs of the form (Var op Const), or (Const op Var), where the op
 * is one of ("=", "<", ">", ">=", "<=")
 *
 * (b) (Var IS [NOT] NULL)
 *
 * (c) combinations using AND/OR/NOT
 *
 * In the future, the range of supported clauses may be expanded to more
 * complex cases, for example (Var op Var).
 */
static bool
statext_is_compatible_clause(PlannerInfo *root, Node *clause, Index relid, Bitmapset **attnums)
{





























































}

/*
 * statext_mcv_clauselist_selectivity
 *		Estimate clauses using the best multi-column statistics.
 *
 * Selects the best extended (multi-column) statistic on a table (measured by
 * the number of attributes extracted from the clauses and covered by it), and
 * computes the selectivity for the supplied clauses.
 *
 * One of the main challenges with using MCV lists is how to extrapolate the
 * estimate to the data not covered by the MCV list. To do that, we compute
 * not only the "MCV selectivity" (selectivities for MCV items matching the
 * supplied clauses), but also a couple of derived selectivities:
 *
 * - simple selectivity:  Computed without extended statistic, i.e. as if the
 * columns/clauses were independent
 *
 * - base selectivity:  Similar to simple selectivity, but is computed using
 * the extended statistic by adding up the base frequencies (that we compute
 * and store for each MCV item) of matching MCV items.
 *
 * - total selectivity: Selectivity covered by the whole MCV list.
 *
 * - other selectivity: A selectivity estimate for data not covered by the MCV
 * list (i.e. satisfying the clauses, but not common enough to make it into
 * the MCV list)
 *
 * Note: While simple and base selectivities are defined in a quite similar
 * way, the values are computed differently and are not therefore equal. The
 * simple selectivity is computed as a product of per-clause estimates, while
 * the base selectivity is computed by adding up base frequencies of matching
 * items of the multi-column MCV list. So the values may differ for two main
 * reasons - (a) the MCV list may not cover 100% of the data and (b) some of
 * the MCV items did not match the estimated clauses.
 *
 * As both (a) and (b) reduce the base selectivity value, it generally holds
 * that (simple_selectivity >= base_selectivity). If the MCV list covers all
 * the data, the values may be equal.
 *
 * So, (simple_selectivity - base_selectivity) is an estimate for the part
 * not covered by the MCV list, and (mcv_selectivity - base_selectivity) may
 * be seen as a correction for the part covered by the MCV list. Those two
 * statements are actually equivalent.
 *
 * Note: Due to rounding errors and minor differences in how the estimates
 * are computed, the inequality may not always hold. Which is why we clamp
 * the selectivities to prevent strange estimate (negative etc.).
 *
 * 'estimatedclauses' is an input/output parameter.  We set bits for the
 * 0-based 'clauses' indexes we estimate for and also skip clause items that
 * already have a bit set.
 *
 * XXX If we were to use multiple statistics, this is where it would happen.
 * We would simply repeat this on a loop on the "remaining" clauses, possibly
 * using the already estimated clauses as conditions (and combining the values
 * using conditional probability formula).
 */
static Selectivity
statext_mcv_clauselist_selectivity(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Bitmapset **estimatedclauses)
{





















































































































}

/*
 * statext_clauselist_selectivity
 *		Estimate clauses using the best multi-column statistics.
 */
Selectivity
statext_clauselist_selectivity(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Bitmapset **estimatedclauses)
{





















}

/*
 * examine_operator_expression
 *		Split expression into Var and Const parts.
 *
 * Attempts to match the arguments to either (Var op Const) or (Const op Var),
 * possibly with a RelabelType on top. When the expression matches this form,
 * returns true, otherwise returns false.
 *
 * Optionally returns pointers to the extracted Var/Const nodes, when passed
 * non-null pointers (varp, cstp and varonleftp). The varonleftp flag specifies
 * on which side of the operator we found the Var node.
 */
bool
examine_opclause_expression(OpExpr *expr, Var **varp, Const **cstp, bool *varonleftp)
{
























































}