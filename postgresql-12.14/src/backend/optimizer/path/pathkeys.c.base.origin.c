/*-------------------------------------------------------------------------
 *
 * pathkeys.c
 *	  Utilities for matching and building path keys
 *
 * See src/backend/optimizer/README for a great deal of information about
 * the nature and use of path keys.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/pathkeys.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/stratnum.h"
#include "catalog/pg_opfamily.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/plannodes.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "partitioning/partbounds.h"
#include "utils/lsyscache.h"

static bool
pathkey_is_redundant(PathKey *new_pathkey, List *pathkeys);
static bool
matches_boolean_partition_clause(RestrictInfo *rinfo, RelOptInfo *partrel, int partkeycol);
static Var *
find_var_for_subquery_tle(RelOptInfo *rel, TargetEntry *tle);
static bool
right_merge_direction(PlannerInfo *root, PathKey *pathkey);

/****************************************************************************
 *		PATHKEY CONSTRUCTION AND REDUNDANCY TESTING
 ****************************************************************************/

/*
 * make_canonical_pathkey
 *	  Given the parameters for a PathKey, find any pre-existing matching
 *	  pathkey in the query's list of "canonical" pathkeys.  Make a new
 *	  entry if there's not one already.
 *
 * Note that this function must not be used until after we have completed
 * merging EquivalenceClasses.  (We don't try to enforce that here; instead,
 * equivclass.c will complain if a merge occurs after root->canon_pathkeys
 * has become nonempty.)
 */
PathKey *
make_canonical_pathkey(PlannerInfo *root, EquivalenceClass *eclass, Oid opfamily, int strategy, bool nulls_first)
{




































}

/*
 * pathkey_is_redundant
 *	   Is a pathkey redundant with one already in the given list?
 *
 * We detect two cases:
 *
 * 1. If the new pathkey's equivalence class contains a constant, and isn't
 * below an outer join, then we can disregard it as a sort key.  An example:
 *			SELECT ... WHERE x = 42 ORDER BY x, y;
 * We may as well just sort by y.  Note that because of opfamily matching,
 * this is semantically correct: we know that the equality constraint is one
 * that actually binds the variable to a single value in the terms of any
 * ordering operator that might go with the eclass.  This rule not only lets
 * us simplify (or even skip) explicit sorts, but also allows matching index
 * sort orders to a query when there are don't-care index columns.
 *
 * 2. If the new pathkey's equivalence class is the same as that of any
 * existing member of the pathkey list, then it is redundant.  Some examples:
 *			SELECT ... ORDER BY x, x;
 *			SELECT ... ORDER BY x, x DESC;
 *			SELECT ... WHERE x = y ORDER BY x, y;
 * In all these cases the second sort key cannot distinguish values that are
 * considered equal by the first, and so there's no point in using it.
 * Note in particular that we need not compare opfamily (all the opfamilies
 * of the EC have the same notion of equality) nor sort direction.
 *
 * Both the given pathkey and the list members must be canonical for this
 * to work properly, but that's okay since we no longer ever construct any
 * non-canonical pathkeys.  (Note: the notion of a pathkey *list* being
 * canonical includes the additional requirement of no redundant entries,
 * which is exactly what we are checking for here.)
 *
 * Because the equivclass.c machinery forms only one copy of any EC per query,
 * pointer comparison is enough to decide whether canonical ECs are the same.
 */
static bool
pathkey_is_redundant(PathKey *new_pathkey, List *pathkeys)
{





















}

/*
 * make_pathkey_from_sortinfo
 *	  Given an expression and sort-order information, create a PathKey.
 *	  The result is always a "canonical" PathKey, but it might be redundant.
 *
 * expr is the expression, and nullable_relids is the set of base relids
 * that are potentially nullable below it.
 *
 * If the PathKey is being generated from a SortGroupClause, sortref should be
 * the SortGroupClause's SortGroupRef; otherwise zero.
 *
 * If rel is not NULL, it identifies a specific relation we're considering
 * a path for, and indicates that child EC members for that relation can be
 * considered.  Otherwise child members are ignored.  (See the comments for
 * get_eclass_for_sort_expr.)
 *
 * create_it is true if we should create any missing EquivalenceClass
 * needed to represent the sort key.  If it's false, we return NULL if the
 * sort key isn't already present in any EquivalenceClass.
 */
static PathKey *
make_pathkey_from_sortinfo(PlannerInfo *root, Expr *expr, Relids nullable_relids, Oid opfamily, Oid opcintype, Oid collation, bool reverse_sort, bool nulls_first, Index sortref, Relids rel, bool create_it)
{



































}

/*
 * make_pathkey_from_sortop
 *	  Like make_pathkey_from_sortinfo, but work from a sort operator.
 *
 * This should eventually go away, but we need to restructure SortGroupClause
 * first.
 */
static PathKey *
make_pathkey_from_sortop(PlannerInfo *root, Expr *expr, Relids nullable_relids, Oid ordering_op, bool nulls_first, Index sortref, bool create_it)
{













}

/****************************************************************************
 *		PATHKEY COMPARISONS
 ****************************************************************************/

/*
 * compare_pathkeys
 *	  Compare two pathkeys to see if they are equivalent, and if not whether
 *	  one is "better" than the other.
 *
 *	  We assume the pathkeys are canonical, and so they can be checked for
 *	  equality by simple pointer comparison.
 */
PathKeysComparison
compare_pathkeys(List *keys1, List *keys2)
{




































}

/*
 * pathkeys_contained_in
 *	  Common special case of compare_pathkeys: we just want to know
 *	  if keys2 are at least as well sorted as keys1.
 */
bool
pathkeys_contained_in(List *keys1, List *keys2)
{









}

/*
 * get_cheapest_path_for_pathkeys
 *	  Find the cheapest path (according to the specified criterion) that
 *	  satisfies the given pathkeys and parameterization.
 *	  Return NULL if no such path.
 *
 * 'paths' is a list of possible paths that all generate the same relation
 * 'pathkeys' represents a required ordering (in canonical form!)
 * 'required_outer' denotes allowable outer relations for parameterized paths
 * 'cost_criterion' is STARTUP_COST or TOTAL_COST
 * 'require_parallel_safe' causes us to consider only parallel-safe paths
 */
Path *
get_cheapest_path_for_pathkeys(List *paths, List *pathkeys, Relids required_outer, CostSelector cost_criterion, bool require_parallel_safe)
{



























}

/*
 * get_cheapest_fractional_path_for_pathkeys
 *	  Find the cheapest path (for retrieving a specified fraction of all
 *	  the tuples) that satisfies the given pathkeys and parameterization.
 *	  Return NULL if no such path.
 *
 * See compare_fractional_path_costs() for the interpretation of the fraction
 * parameter.
 *
 * 'paths' is a list of possible paths that all generate the same relation
 * 'pathkeys' represents a required ordering (in canonical form!)
 * 'required_outer' denotes allowable outer relations for parameterized paths
 * 'fraction' is the fraction of the total tuples expected to be retrieved
 */
Path *
get_cheapest_fractional_path_for_pathkeys(List *paths, List *pathkeys, Relids required_outer, double fraction)
{






















}

/*
 * get_cheapest_parallel_safe_total_inner
 *	  Find the unparameterized parallel-safe path with the least total cost.
 */
Path *
get_cheapest_parallel_safe_total_inner(List *paths)
{













}

/****************************************************************************
 *		NEW PATHKEY FORMATION
 ****************************************************************************/

/*
 * build_index_pathkeys
 *	  Build a pathkeys list that describes the ordering induced by an index
 *	  scan using the given index.  (Note that an unordered index doesn't
 *	  induce any ordering, so we return NIL.)
 *
 * If 'scandir' is BackwardScanDirection, build pathkeys representing a
 * backwards scan of the index.
 *
 * We iterate only key columns of covering indexes, since non-key columns
 * don't influence index ordering.  The result is canonical, meaning that
 * redundant pathkeys are removed; it may therefore have fewer entries than
 * there are key columns in the index.
 *
 * Another reason for stopping early is that we may be able to tell that
 * an index column's sort order is uninteresting for this query.  However,
 * that test is just based on the existence of an EquivalenceClass and not
 * on position in pathkey lists, so it's not complete.  Caller should call
 * truncate_useless_pathkeys() to possibly remove more pathkeys.
 */
List *
build_index_pathkeys(PlannerInfo *root, IndexOptInfo *index, ScanDirection scandir)
{
















































































}

/*
 * partkey_is_bool_constant_for_query
 *
 * If a partition key column is constrained to have a constant value by the
 * query's WHERE conditions, then it's irrelevant for sort-order
 * considerations.  Usually that means we have a restriction clause
 * WHERE partkeycol = constant, which gets turned into an EquivalenceClass
 * containing a constant, which is recognized as redundant by
 * build_partition_pathkeys().  But if the partition key column is a
 * boolean variable (or expression), then we are not going to see such a
 * WHERE clause, because expression preprocessing will have simplified it
 * to "WHERE partkeycol" or "WHERE NOT partkeycol".  So we are not going
 * to have a matching EquivalenceClass (unless the query also contains
 * "ORDER BY partkeycol").  To allow such cases to work the same as they would
 * for non-boolean values, this function is provided to detect whether the
 * specified partition key column matches a boolean restriction clause.
 */
static bool
partkey_is_bool_constant_for_query(RelOptInfo *partrel, int partkeycol)
{




























}

/*
 * matches_boolean_partition_clause
 *		Determine if the boolean clause described by rinfo matches
 *		partrel's partkeycol-th partition key column.
 *
 * "Matches" can be either an exact match (equivalent to partkey = true),
 * or a NOT above an exact match (equivalent to partkey = false).
 */
static bool
matches_boolean_partition_clause(RestrictInfo *rinfo, RelOptInfo *partrel, int partkeycol)
{




















}

/*
 * build_partition_pathkeys
 *	  Build a pathkeys list that describes the ordering induced by the
 *	  partitions of partrel, under either forward or backward scan
 *	  as per scandir.
 *
 * Caller must have checked that the partitions are properly ordered,
 * as detected by partitions_are_ordered().
 *
 * Sets *partialkeys to true if pathkeys were only built for a prefix of the
 * partition key, or false if the pathkeys include all columns of the
 * partition key.
 */
List *
build_partition_pathkeys(PlannerInfo *root, RelOptInfo *partrel, ScanDirection scandir, bool *partialkeys)
{

























































}

/*
 * build_expression_pathkey
 *	  Build a pathkeys list that describes an ordering by a single
 *expression using the given sort operator.
 *
 * expr, nullable_relids, and rel are as for make_pathkey_from_sortinfo.
 * We induce the other arguments assuming default sort order for the operator.
 *
 * Similarly to make_pathkey_from_sortinfo, the result is NIL if create_it
 * is false and the expression isn't already in some EquivalenceClass.
 */
List *
build_expression_pathkey(PlannerInfo *root, Expr *expr, Relids nullable_relids, Oid opno, Relids rel, bool create_it)
{























}

/*
 * convert_subquery_pathkeys
 *	  Build a pathkeys list that describes the ordering of a subquery's
 *	  result, in the terms of the outer query.  This is essentially a
 *	  task of conversion.
 *
 * 'rel': outer query's RelOptInfo for the subquery relation.
 * 'subquery_pathkeys': the subquery's output pathkeys, in its terms.
 * 'subquery_tlist': the subquery's output targetlist, in its terms.
 *
 * We intentionally don't do truncate_useless_pathkeys() here, because there
 * are situations where seeing the raw ordering of the subquery is helpful.
 * For example, if it returns ORDER BY x DESC, that may prompt us to
 * construct a mergejoin using DESC order rather than ASC order; but the
 * right_merge_direction heuristic would have us throw the knowledge away.
 */
List *
convert_subquery_pathkeys(PlannerInfo *root, RelOptInfo *rel, List *subquery_pathkeys, List *subquery_tlist)
{












































































































































































}

/*
 * find_var_for_subquery_tle
 *
 * If the given subquery tlist entry is due to be emitted by the subquery's
 * scan node, return a Var for it, else return NULL.
 *
 * We need this to ensure that we don't return pathkeys describing values
 * that are unavailable above the level of the subquery scan.
 */
static Var *
find_var_for_subquery_tle(RelOptInfo *rel, TargetEntry *tle)
{



























}

/*
 * build_join_pathkeys
 *	  Build the path keys for a join relation constructed by mergejoin or
 *	  nestloop join.  This is normally the same as the outer path's keys.
 *
 *	  EXCEPTION: in a FULL or RIGHT join, we cannot treat the result as
 *	  having the outer path's path keys, because null lefthand rows may be
 *	  inserted at random points.  It must be treated as unsorted.
 *
 *	  We truncate away any pathkeys that are uninteresting for higher joins.
 *
 * 'joinrel' is the join relation that paths are being formed for
 * 'jointype' is the join type (inner, left, full, etc)
 * 'outer_pathkeys' is the list of the current outer path's path keys
 *
 * Returns the list of new path keys.
 */
List *
build_join_pathkeys(PlannerInfo *root, RelOptInfo *joinrel, JoinType jointype, List *outer_pathkeys)
{















}

/****************************************************************************
 *		PATHKEYS AND SORT CLAUSES
 ****************************************************************************/

/*
 * make_pathkeys_for_sortclauses
 *		Generate a pathkeys list that represents the sort order
 *specified by a list of SortGroupClauses
 *
 * The resulting PathKeys are always in canonical form.  (Actually, there
 * is no longer any code anywhere that creates non-canonical PathKeys.)
 *
 * We assume that root->nullable_baserels is the set of base relids that could
 * have gone to NULL below the SortGroupClause expressions.  This is okay if
 * the expressions came from the query's top level (ORDER BY, DISTINCT, etc)
 * and if this function is only invoked after deconstruct_jointree.  In the
 * future we might have to make callers pass in the appropriate
 * nullable-relids set, but for now it seems unnecessary.
 *
 * 'sortclauses' is a list of SortGroupClause nodes
 * 'tlist' is the targetlist to find the referenced tlist entries in
 */
List *
make_pathkeys_for_sortclauses(PlannerInfo *root, List *sortclauses, List *tlist)
{




















}

/****************************************************************************
 *		PATHKEYS AND MERGECLAUSES
 ****************************************************************************/

/*
 * initialize_mergeclause_eclasses
 *		Set the EquivalenceClass links in a mergeclause restrictinfo.
 *
 * RestrictInfo contains fields in which we may cache pointers to
 * EquivalenceClasses for the left and right inputs of the mergeclause.
 * (If the mergeclause is a true equivalence clause these will be the
 * same EquivalenceClass, otherwise not.)  If the mergeclause is either
 * used to generate an EquivalenceClass, or derived from an EquivalenceClass,
 * then it's easy to set up the left_ec and right_ec members --- otherwise,
 * this function should be called to set them up.  We will generate new
 * EquivalenceClauses if necessary to represent the mergeclause's left and
 * right sides.
 *
 * Note this is called before EC merging is complete, so the links won't
 * necessarily point to canonical ECs.  Before they are actually used for
 * anything, update_mergeclause_eclasses must be called to ensure that
 * they've been updated to point to canonical ECs.
 */
void
initialize_mergeclause_eclasses(PlannerInfo *root, RestrictInfo *restrictinfo)
{















}

/*
 * update_mergeclause_eclasses
 *		Make the cached EquivalenceClass links valid in a mergeclause
 *		restrictinfo.
 *
 * These pointers should have been set by process_equivalence or
 * initialize_mergeclause_eclasses, but they might have been set to
 * non-canonical ECs that got merged later.  Chase up to the canonical
 * merged parent if so.
 */
void
update_mergeclause_eclasses(PlannerInfo *root, RestrictInfo *restrictinfo)
{















}

/*
 * find_mergeclauses_for_outer_pathkeys
 *	  This routine attempts to find a list of mergeclauses that can be
 *	  used with a specified ordering for the join's outer relation.
 *	  If successful, it returns a list of mergeclauses.
 *
 * 'pathkeys' is a pathkeys list showing the ordering of an outer-rel path.
 * 'restrictinfos' is a list of mergejoinable restriction clauses for the
 *			join relation being formed, in no particular order.
 *
 * The restrictinfos must be marked (via outer_is_left) to show which side
 * of each clause is associated with the current outer path.  (See
 * select_mergejoin_clauses())
 *
 * The result is NIL if no merge can be done, else a maximal list of
 * usable mergeclauses (represented as a list of their restrictinfo nodes).
 * The list is ordered to match the pathkeys, as required for execution.
 */
List *
find_mergeclauses_for_outer_pathkeys(PlannerInfo *root, List *pathkeys, List *restrictinfos)
{





















































































}

/*
 * select_outer_pathkeys_for_merge
 *	  Builds a pathkey list representing a possible sort ordering
 *	  that can be used with the given mergeclauses.
 *
 * 'mergeclauses' is a list of RestrictInfos for mergejoin clauses
 *			that will be used in a merge join.
 * 'joinrel' is the join relation we are trying to construct.
 *
 * The restrictinfos must be marked (via outer_is_left) to show which side
 * of each clause is associated with the current outer path.  (See
 * select_mergejoin_clauses())
 *
 * Returns a pathkeys list that can be applied to the outer relation.
 *
 * Since we assume here that a sort is required, there is no particular use
 * in matching any available ordering of the outerrel.  (joinpath.c has an
 * entirely separate code path for considering sort-free mergejoins.)  Rather,
 * it's interesting to try to match the requested query_pathkeys so that a
 * second output sort may be avoided; and failing that, we try to list "more
 * popular" keys (those with the most unmatched EquivalenceClass peers)
 * earlier, in hopes of making the resulting ordering useful for as many
 * higher-level mergejoins as possible.
 */
List *
select_outer_pathkeys_for_merge(PlannerInfo *root, List *mergeclauses, RelOptInfo *joinrel)
{





























































































































































}

/*
 * make_inner_pathkeys_for_merge
 *	  Builds a pathkey list representing the explicit sort order that
 *	  must be applied to an inner path to make it usable with the
 *	  given mergeclauses.
 *
 * 'mergeclauses' is a list of RestrictInfos for the mergejoin clauses
 *			that will be used in a merge join, in order.
 * 'outer_pathkeys' are the already-known canonical pathkeys for the outer
 *			side of the join.
 *
 * The restrictinfos must be marked (via outer_is_left) to show which side
 * of each clause is associated with the current outer path.  (See
 * select_mergejoin_clauses())
 *
 * Returns a pathkeys list that can be applied to the inner relation.
 *
 * Note that it is not this routine's job to decide whether sorting is
 * actually needed for a particular input path.  Assume a sort is necessary;
 * just make the keys, eh?
 */
List *
make_inner_pathkeys_for_merge(PlannerInfo *root, List *mergeclauses, List *outer_pathkeys)
{













































































}

/*
 * trim_mergeclauses_for_inner_pathkeys
 *	  This routine trims a list of mergeclauses to include just those that
 *	  work with a specified ordering for the join's inner relation.
 *
 * 'mergeclauses' is a list of RestrictInfos for mergejoin clauses for the
 *			join relation being formed, in an order known to work
 *for the currently-considered sort ordering of the join's outer rel. 'pathkeys'
 *is a pathkeys list showing the ordering of an inner-rel path; it should be
 *equal to, or a truncation of, the result of make_inner_pathkeys_for_merge for
 *these mergeclauses.
 *
 * What we return will be a prefix of the given mergeclauses list.
 *
 * We need this logic because make_inner_pathkeys_for_merge's result isn't
 * necessarily in the same order as the mergeclauses.  That means that if we
 * consider an inner-rel pathkey list that is a truncation of that result,
 * we might need to drop mergeclauses even though they match a surviving inner
 * pathkey.  This happens when they are to the right of a mergeclause that
 * matches a removed inner pathkey.
 *
 * The mergeclauses must be marked (via outer_is_left) to show which side
 * of each clause is associated with the current outer path.  (See
 * select_mergejoin_clauses())
 */
List *
trim_mergeclauses_for_inner_pathkeys(PlannerInfo *root, List *mergeclauses, List *pathkeys)
{
































































}

/****************************************************************************
 *		PATHKEY USEFULNESS CHECKS
 *
 * We only want to remember as many of the pathkeys of a path as have some
 * potential use, either for subsequent mergejoins or for meeting the query's
 * requested output ordering.  This ensures that add_path() won't consider
 * a path to have a usefully different ordering unless it really is useful.
 * These routines check for usefulness of given pathkeys.
 ****************************************************************************/

/*
 * pathkeys_useful_for_merging
 *		Count the number of pathkeys that may be useful for mergejoins
 *		above the given relation.
 *
 * We consider a pathkey potentially useful if it corresponds to the merge
 * ordering of either side of any joinclause for the rel.  This might be
 * overoptimistic, since joinclauses that require different other relations
 * might never be usable at the same time, but trying to be exact is likely
 * to be more trouble than it's worth.
 *
 * To avoid doubling the number of mergejoin paths considered, we would like
 * to consider only one of the two scan directions (ASC or DESC) as useful
 * for merging for any given target column.  The choice is arbitrary unless
 * one of the directions happens to match an ORDER BY key, in which case
 * that direction should be preferred, in hopes of avoiding a final sort step.
 * right_merge_direction() implements this heuristic.
 */
static int
pathkeys_useful_for_merging(PlannerInfo *root, RelOptInfo *rel, List *pathkeys)
{

































































}

/*
 * right_merge_direction
 *		Check whether the pathkey embodies the preferred sort direction
 *		for merging its target column.
 */
static bool
right_merge_direction(PlannerInfo *root, PathKey *pathkey)
{





















}

/*
 * pathkeys_useful_for_ordering
 *		Count the number of pathkeys that are useful for meeting the
 *		query's requested output ordering.
 *
 * Unlike merge pathkeys, this is an all-or-nothing affair: it does us
 * no good to order by just the first key(s) of the requested ordering.
 * So the result is always either 0 or list_length(root->query_pathkeys).
 */
static int
pathkeys_useful_for_ordering(PlannerInfo *root, List *pathkeys)
{

















}

/*
 * truncate_useless_pathkeys
 *		Shorten the given pathkey list to just the useful pathkeys.
 */
List *
truncate_useless_pathkeys(PlannerInfo *root, RelOptInfo *rel, List *pathkeys)
{


























}

/*
 * has_useful_pathkeys
 *		Detect whether the specified rel could have any pathkeys that
 *are useful according to truncate_useless_pathkeys().
 *
 * This is a cheap test that lets us skip building pathkeys at all in very
 * simple queries.  It's OK to err in the direction of returning "true" when
 * there really aren't any usable pathkeys, but erring in the other direction
 * is bad --- so keep this in sync with the routines above!
 *
 * We could make the test more complex, for example checking to see if any of
 * the joinclauses are really mergejoinable, but that likely wouldn't win
 * often enough to repay the extra cycles.  Queries with neither a join nor
 * a sort are reasonably common, though, so this much work seems worthwhile.
 */
bool
has_useful_pathkeys(PlannerInfo *root, RelOptInfo *rel)
{









}