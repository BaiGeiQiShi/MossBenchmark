/*-------------------------------------------------------------------------
 *
 * joinpath.c
 *	  Routines to find all possible paths for processing a set of joins
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/joinpath.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>

#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"

/* Hook for plugins to get control in add_paths_to_joinrel() */
set_join_pathlist_hook_type set_join_pathlist_hook = NULL;

/*
 * Paths parameterized by the parent can be considered to be parameterized by
 * any of its child.
 */
#define PATH_PARAM_BY_PARENT(path, rel) ((path)->param_info && bms_overlap(PATH_REQ_OUTER(path), (rel)->top_parent_relids))
#define PATH_PARAM_BY_REL_SELF(path, rel) ((path)->param_info && bms_overlap(PATH_REQ_OUTER(path), (rel)->relids))

#define PATH_PARAM_BY_REL(path, rel) (PATH_PARAM_BY_REL_SELF(path, rel) || PATH_PARAM_BY_PARENT(path, rel))

static void
try_partial_mergejoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, List *mergeclauses, List *outersortkeys, List *innersortkeys, JoinType jointype, JoinPathExtraData *extra);
static void
sort_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static void
match_unsorted_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static void
consider_parallel_nestloop(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static void
consider_parallel_mergejoin(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra, Path *inner_cheapest_total);
static void
hash_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra);
static List *
select_mergejoin_clauses(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, List *restrictlist, JoinType jointype, bool *mergejoin_allowed);
static void
generate_mergejoin_paths(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *innerrel, Path *outerpath, JoinType jointype, JoinPathExtraData *extra, bool useallclauses, Path *inner_cheapest_total, List *merge_pathkeys, bool is_partial);

/*
 * add_paths_to_joinrel
 *	  Given a join relation and two component rels from which it can be
 *made, consider all possible paths that use the two component rels as outer and
 *inner rel respectively.  Add these paths to the join rel's pathlist if they
 *survive comparison with other paths (and remove any existing paths that are
 *dominated by these paths).
 *
 * Modifies the pathlist field of the joinrel node to contain the best
 * paths found so far.
 *
 * jointype is not necessarily the same as sjinfo->jointype; it might be
 * "flipped around" if we are considering joining the rels in the opposite
 * direction from what's indicated in sjinfo.
 *
 * Also, this routine and others in this module accept the special JoinTypes
 * JOIN_UNIQUE_OUTER and JOIN_UNIQUE_INNER to indicate that we should
 * unique-ify the outer or inner relation and then apply a regular inner
 * join.  These values are not allowed to propagate outside this module,
 * however.  Path cost estimation code may need to recognize that it's
 * dealing with such a case --- the combination of nominal jointype INNER
 * with sjinfo->jointype == JOIN_SEMI indicates that.
 */
void
add_paths_to_joinrel(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, SpecialJoinInfo *sjinfo, List *restrictlist)
{





























































































































































































}

/*
 * We override the param_source_rels heuristic to accept nestloop paths in
 * which the outer rel satisfies some but not all of the inner path's
 * parameterization.  This is necessary to get good plans for star-schema
 * scenarios, in which a parameterized path for a large table may require
 * parameters from multiple small tables that will not get joined directly to
 * each other.  We can handle that by stacking nestloops that have the small
 * tables on the outside; but this breaks the rule the param_source_rels
 * heuristic is based on, namely that parameters should not be passed down
 * across joins unless there's a join-order-constraint-based reason to do so.
 * So we ignore the param_source_rels restriction when this case applies.
 *
 * allow_star_schema_join() returns true if the param_source_rels restriction
 * should be overridden, ie, it's okay to perform this join.
 */
static inline bool
allow_star_schema_join(PlannerInfo *root, Relids outerrelids, Relids inner_paramrels)
{





}

/*
 * try_nestloop_path
 *	  Consider a nestloop join path; if it appears useful, push it into
 *	  the joinrel's pathlist via add_path().
 */
static void
try_nestloop_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, JoinType jointype, JoinPathExtraData *extra)
{






















































































}

/*
 * try_partial_nestloop_path
 *	  Consider a partial nestloop join path; if it appears useful, push it
 *into the joinrel's partial_pathlist via add_partial_path().
 */
static void
try_partial_nestloop_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, JoinType jointype, JoinPathExtraData *extra)
{
































































}

/*
 * try_mergejoin_path
 *	  Consider a merge join path; if it appears useful, push it into
 *	  the joinrel's pathlist via add_path().
 */
static void
try_mergejoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, List *mergeclauses, List *outersortkeys, List *innersortkeys, JoinType jointype, JoinPathExtraData *extra, bool is_partial)
{
















































}

/*
 * try_partial_mergejoin_path
 *	  Consider a partial merge join path; if it appears useful, push it into
 *	  the joinrel's pathlist via add_partial_path().
 */
static void
try_partial_mergejoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *pathkeys, List *mergeclauses, List *outersortkeys, List *innersortkeys, JoinType jointype, JoinPathExtraData *extra)
{









































}

/*
 * try_hashjoin_path
 *	  Consider a hash join path; if it appears useful, push it into
 *	  the joinrel's pathlist via add_path().
 */
static void
try_hashjoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *hashclauses, JoinType jointype, JoinPathExtraData *extra)
{































}

/*
 * try_partial_hashjoin_path
 *	  Consider a partial hashjoin join path; if it appears useful, push it
 *into the joinrel's partial_pathlist via add_partial_path(). The outer side is
 *partial.  If parallel_hash is true, then the inner path must be partial and
 *will be run in parallel to create one or more shared hash tables; otherwise
 *the inner path must be complete and a copy of it is run in every process to
 *create separate identical private hash tables.
 */
static void
try_partial_hashjoin_path(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, List *hashclauses, JoinType jointype, JoinPathExtraData *extra, bool parallel_hash)
{































}

/*
 * clause_sides_match_join
 *	  Determine whether a join clause is of the right form to use in this
 *join.
 *
 * We already know that the clause is a binary opclause referencing only the
 * rels in the current join.  The point here is to check whether it has the
 * form "outerrel_expr op innerrel_expr" or "innerrel_expr op outerrel_expr",
 * rather than mixing outer and inner vars on either side.  If it matches,
 * we set the transient flag outer_is_left to identify which side is which.
 */
static inline bool
clause_sides_match_join(RestrictInfo *rinfo, RelOptInfo *outerrel, RelOptInfo *innerrel)
{













}

/*
 * sort_inner_and_outer
 *	  Create mergejoin join paths by explicitly sorting both the outer and
 *	  inner join relations on each available merge ordering.
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'jointype' is the type of join to do
 * 'extra' contains additional input values
 */
static void
sort_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{
























































































































































}

/*
 * generate_mergejoin_paths
 *	Creates possible mergejoin paths for input outerpath.
 *
 * We generate mergejoins if mergejoin clauses are available.  We have
 * two ways to generate the inner path for a mergejoin: sort the cheapest
 * inner path, or use an inner path that is already suitably ordered for the
 * merge.  If we have several mergeclauses, it could be that there is no inner
 * path (or only a very expensive one) for the full list of mergeclauses, but
 * better paths exist if we truncate the mergeclause list (thereby discarding
 * some sort key requirements).  So, we consider truncations of the
 * mergeclause list as well as the full list.  (Ideally we'd consider all
 * subsets of the mergeclause list, but that seems way too expensive.)
 */
static void
generate_mergejoin_paths(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *innerrel, Path *outerpath, JoinType jointype, JoinPathExtraData *extra, bool useallclauses, Path *inner_cheapest_total, List *merge_pathkeys, bool is_partial)
{














































































































































































}

/*
 * match_unsorted_outer
 *	  Creates possible join paths for processing a single join relation
 *	  'joinrel' by employing either iterative substitution or
 *	  mergejoining on each of its possible outer paths (considering
 *	  only outer paths that are already ordered well enough for merging).
 *
 * We always generate a nestloop path for each available outer path.
 * In fact we may generate as many as five: one on the cheapest-total-cost
 * inner path, one on the same with materialization, one on the
 * cheapest-startup-cost inner path (if different), one on the
 * cheapest-total inner-indexscan path (if any), and one on the
 * cheapest-startup inner-indexscan path (if different).
 *
 * We also consider mergejoins if mergejoin clauses are available.  See
 * detailed comments in generate_mergejoin_paths.
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'jointype' is the type of join to do
 * 'extra' contains additional input values
 */
static void
match_unsorted_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{





































































































































































































}

/*
 * consider_parallel_mergejoin
 *	  Try to build partial paths for a joinrel by joining a partial path
 *	  for the outer relation to a complete path for the inner relation.
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'jointype' is the type of join to do
 * 'extra' contains additional input values
 * 'inner_cheapest_total' cheapest total path for innerrel
 */
static void
consider_parallel_mergejoin(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra, Path *inner_cheapest_total)
{















}

/*
 * consider_parallel_nestloop
 *	  Try to build partial paths for a joinrel by joining a partial path for
 *the outer relation to a complete path for the inner relation.
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'jointype' is the type of join to do
 * 'extra' contains additional input values
 */
static void
consider_parallel_nestloop(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{





















































}

/*
 * hash_inner_and_outer
 *	  Create hashjoin join paths by explicitly hashing both the outer and
 *	  inner keys of each available hash clause.
 *
 * 'joinrel' is the join relation
 * 'outerrel' is the outer join relation
 * 'innerrel' is the inner join relation
 * 'jointype' is the type of join to do
 * 'extra' contains additional input values
 */
static void
hash_inner_and_outer(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, JoinPathExtraData *extra)
{































































































































































































}

/*
 * select_mergejoin_clauses
 *	  Select mergejoin clauses that are usable for a particular join.
 *	  Returns a list of RestrictInfo nodes for those clauses.
 *
 * *mergejoin_allowed is normally set to true, but it is set to false if
 * this is a right/full join and there are nonmergejoinable join clauses.
 * The executor's mergejoin machinery cannot handle such cases, so we have
 * to avoid generating a mergejoin plan.  (Note that this flag does NOT
 * consider whether there are actually any mergejoinable clauses.  This is
 * correct because in some cases we need to build a clauseless mergejoin.
 * Simply returning NIL is therefore not enough to distinguish safe from
 * unsafe cases.)
 *
 * We also mark each selected RestrictInfo to show which side is currently
 * being considered as outer.  These are transient markings that are only
 * good for the duration of the current add_paths_to_joinrel() call!
 *
 * We examine each restrictinfo clause known for the join to see
 * if it is mergejoinable and involves vars from the two sub-relations
 * currently of interest.
 */
static List *
select_mergejoin_clauses(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, List *restrictlist, JoinType jointype, bool *mergejoin_allowed)
{



























































































}