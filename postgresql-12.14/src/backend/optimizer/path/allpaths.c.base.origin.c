/*-------------------------------------------------------------------------
 *
 * allpaths.c
 *	  Routines to find possible search paths for processing a query
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/allpaths.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/sysattr.h"
#include "access/tsmapi.h"
#include "catalog/pg_class.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#ifdef OPTIMIZER_DEBUG
#include "nodes/print.h"
#endif
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/geqo.h"
#include "optimizer/inherit.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "optimizer/planner.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "partitioning/partbounds.h"
#include "partitioning/partprune.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"

/* results of subquery_is_pushdown_safe */
typedef struct pushdown_safety_info
{
  bool *unsafeColumns; /* which output columns are unsafe to use */
  bool unsafeVolatile; /* don't push down volatile quals */
  bool unsafeLeaky;    /* don't push down leaky quals */
} pushdown_safety_info;

/* These parameters are set by GUC */
bool enable_geqo = false; /* just in case GUC doesn't set it */
int geqo_threshold;
int min_parallel_table_scan_size;
int min_parallel_index_scan_size;

/* Hook for plugins to get control in set_rel_pathlist() */
set_rel_pathlist_hook_type set_rel_pathlist_hook = NULL;

/* Hook for plugins to replace standard_join_search() */
join_search_hook_type join_search_hook = NULL;

static void
set_base_rel_consider_startup(PlannerInfo *root);
static void
set_base_rel_sizes(PlannerInfo *root);
static void
set_base_rel_pathlists(PlannerInfo *root);
static void
set_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_plain_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
create_plain_partial_paths(PlannerInfo *root, RelOptInfo *rel);
static void
set_rel_consider_parallel(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_plain_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_tablesample_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_tablesample_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_foreign_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_foreign_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_append_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_append_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
generate_orderedappend_paths(PlannerInfo *root, RelOptInfo *rel, List *live_childrels, List *all_child_pathkeys, List *partitioned_rels);
static Path *
get_cheapest_parameterized_child_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer);
static void
accumulate_append_subpath(Path *path, List **subpaths, List **special_subpaths);
static Path *
get_singleton_append_subpath(Path *path);
static void
set_dummy_rel_pathlist(RelOptInfo *rel);
static void
set_subquery_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte);
static void
set_function_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_values_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_tablefunc_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_cte_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_namedtuplestore_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_result_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static void
set_worktable_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte);
static RelOptInfo *
make_rel_from_joinlist(PlannerInfo *root, List *joinlist);
static bool
subquery_is_pushdown_safe(Query *subquery, Query *topquery, pushdown_safety_info *safetyInfo);
static bool
recurse_pushdown_safe(Node *setOp, Query *topquery, pushdown_safety_info *safetyInfo);
static void
check_output_expressions(Query *subquery, pushdown_safety_info *safetyInfo);
static void
compare_tlist_datatypes(List *tlist, List *colTypes, pushdown_safety_info *safetyInfo);
static bool
targetIsInAllPartitionLists(TargetEntry *tle, Query *query);
static bool
qual_is_pushdown_safe(Query *subquery, Index rti, Node *qual, pushdown_safety_info *safetyInfo);
static void
subquery_push_qual(Query *subquery, RangeTblEntry *rte, Index rti, Node *qual);
static void
recurse_push_qual(Node *setOp, Query *topquery, RangeTblEntry *rte, Index rti, Node *qual);
static void
remove_unused_subquery_outputs(Query *subquery, RelOptInfo *rel);

/*
 * make_one_rel
 *	  Finds all possible access paths for executing a query, returning a
 *	  single rel that represents the join of all base rels in the query.
 */
RelOptInfo *
make_one_rel(PlannerInfo *root, List *joinlist)
{



























































































}

/*
 * set_base_rel_consider_startup
 *	  Set the consider_[param_]startup flags for each base-relation entry.
 *
 * For the moment, we only deal with consider_param_startup here; because the
 * logic for consider_startup is pretty trivial and is the same for every base
 * relation, we just let build_simple_rel() initialize that flag correctly to
 * start with.  If that logic ever gets more complicated it would probably
 * be better to move it here.
 */
static void
set_base_rel_consider_startup(PlannerInfo *root)
{



























}

/*
 * set_base_rel_sizes
 *	  Set the size estimates (rows and widths) for each base-relation entry.
 *	  Also determine whether to consider parallel paths for base relations.
 *
 * We do this in a separate pass over the base rels so that rowcount
 * estimates are available for parameterized path generation, and also so
 * that each rel's consider_parallel flag is set correctly before we begin to
 * generate paths.
 */
static void
set_base_rel_sizes(PlannerInfo *root)
{






































}

/*
 * set_base_rel_pathlists
 *	  Finds all paths available for scanning each base-relation entry.
 *	  Sequential scan and any available indices are considered.
 *	  Each useful path is attached to its relation's 'pathlist' field.
 */
static void
set_base_rel_pathlists(PlannerInfo *root)
{






















}

/*
 * set_rel_size
 *	  Set size estimates for a base relation
 */
static void
set_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{






































































































}

/*
 * set_rel_pathlist
 *	  Build access paths for a base relation
 */
static void
set_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{

































































































}

/*
 * set_plain_rel_size
 *	  Set size estimates for a plain relation (no subquery, no inheritance)
 */
static void
set_plain_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{








}

/*
 * If this relation could possibly be scanned from within a worker, then set
 * its consider_parallel flag.
 */
static void
set_rel_consider_parallel(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{

























































































































































































}

/*
 * set_plain_rel_pathlist
 *	  Build access paths for a plain relation (no subquery, no inheritance)
 */
static void
set_plain_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{























}

/*
 * create_plain_partial_paths
 *	  Build partial access paths for parallel scan of a plain relation
 */
static void
create_plain_partial_paths(PlannerInfo *root, RelOptInfo *rel)
{












}

/*
 * set_tablesample_rel_size
 *	  Set size estimates for a sampled relation
 */
static void
set_tablesample_rel_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{






























}

/*
 * set_tablesample_rel_pathlist
 *	  Build access paths for a sampled relation
 */
static void
set_tablesample_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{





































}

/*
 * set_foreign_size
 *		Set size estimates for a foreign table RTE
 */
static void
set_foreign_size(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{











}

/*
 * set_foreign_pathlist
 *		Build access paths for a foreign table RTE
 */
static void
set_foreign_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{


}

/*
 * set_append_rel_size
 *	  Set size estimates for a simple "append relation"
 *
 * The passed-in rel and RTE represent the entire append relation.  The
 * relation's contents are computed by appending together the output of the
 * individual member relations.  Note that in the non-partitioned inheritance
 * case, the first member relation is actually the same table as is mentioned
 * in the parent RTE ... but it has a different RTE and RelOptInfo.  This is
 * a good thing because their outputs are not the same size.
 */
static void
set_append_rel_size(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{























































































































































































































































































}

/*
 * set_append_rel_pathlist
 *	  Build access paths for an "append relation"
 */
static void
set_append_rel_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{
































































}

/*
 * add_paths_to_append_rel
 *		Generate paths for the given append relation given the set of
 *non-dummy child rels.
 *
 * The function collects all parameterizations and orderings supported by the
 * non-dummy children. For every such parameterization or ordering, it creates
 * an append path collecting one path from each non-dummy child with given
 * parameterization or ordering. Similarly it collects partial paths from
 * non-dummy children to create partial append paths.
 */
void
add_paths_to_append_rel(PlannerInfo *root, RelOptInfo *rel, List *live_childrels)
{











































































































































































































































































































































































































}

/*
 * generate_orderedappend_paths
 *		Generate ordered append paths for an append relation
 *
 * Usually we generate MergeAppend paths here, but there are some special
 * cases where we can generate simple Append paths, because the subpaths
 * can provide tuples in the required order already.
 *
 * We generate a path for each ordering (pathkey list) appearing in
 * all_child_pathkeys.
 *
 * We consider both cheapest-startup and cheapest-total cases, ie, for each
 * interesting ordering, collect all the cheapest startup subpaths and all the
 * cheapest total paths, and build a suitable path for each case.
 *
 * We don't currently generate any parameterized ordered paths here.  While
 * it would not take much more code here to do so, it's very unclear that it
 * is worth the planning cycles to investigate such paths: there's little
 * use for an ordered path on the inside of a nestloop.  In fact, it's likely
 * that the current coding of add_path would reject such paths out of hand,
 * because add_path gives no credit for sort ordering of parameterized paths,
 * and a parameterized MergeAppend is going to be more expensive than the
 * corresponding parameterized Append path.  If we ever try harder to support
 * parameterized mergejoin plans, it might be worth adding support for
 * parameterized paths here to feed such joins.  (See notes in
 * optimizer/README for why that might not ever happen, though.)
 */
static void
generate_orderedappend_paths(PlannerInfo *root, RelOptInfo *rel, List *live_childrels, List *all_child_pathkeys, List *partitioned_rels)
{





















































































































































}

/*
 * get_cheapest_parameterized_child_path
 *		Get cheapest path for this relation that has exactly the
 *requested parameterization.
 *
 * Returns NULL if unable to create such a path.
 */
static Path *
get_cheapest_parameterized_child_path(PlannerInfo *root, RelOptInfo *rel, Relids required_outer)
{

































































}

/*
 * accumulate_append_subpath
 *		Add a subpath to the list being built for an Append or
 *MergeAppend.
 *
 * It's possible that the child is itself an Append or MergeAppend path, in
 * which case we can "cut out the middleman" and just add its child paths to
 * our own list.  (We don't try to do this earlier because we need to apply
 * both levels of transformation to the quals.)
 *
 * Note that if we omit a child MergeAppend in this way, we are effectively
 * omitting a sort step, which seems fine: if the parent is to be an Append,
 * its result would be unsorted anyway, while if the parent is to be a
 * MergeAppend, there's no point in a separate sort on a child.
 *
 * Normally, either path is a partial path and subpaths is a list of partial
 * paths, or else path is a non-partial plan and subpaths is a list of those.
 * However, if path is a parallel-aware Append, then we add its partial path
 * children to subpaths and the rest to special_subpaths.  If the latter is
 * NULL, we don't flatten the path at all (unless it contains only partial
 * paths).
 */
static void
accumulate_append_subpath(Path *path, List **subpaths, List **special_subpaths)
{































}

/*
 * get_singleton_append_subpath
 *		Returns the single subpath of an Append/MergeAppend, or just
 *		return 'path' if it's not a single sub-path Append/MergeAppend.
 *
 * Note: 'path' must not be a parallel-aware path.
 */
static Path *
get_singleton_append_subpath(Path *path)
{






















}

/*
 * set_dummy_rel_pathlist
 *	  Build a dummy path for a relation that's been excluded by constraints
 *
 * Rather than inventing a special "dummy" path type, we represent this as an
 * AppendPath with no members (see also IS_DUMMY_APPEND/IS_DUMMY_REL macros).
 *
 * (See also mark_dummy_rel, which does basically the same thing, but is
 * typically used to change a rel into dummy state after we already made
 * paths for it.)
 */
static void
set_dummy_rel_pathlist(RelOptInfo *rel)
{


















}

/* quick-and-dirty test to see if any joining is needed */
static bool
has_multiple_baserels(PlannerInfo *root)
{






















}

/*
 * set_subquery_pathlist
 *		Generate SubqueryScan access paths for a subquery RTE
 *
 * We don't currently support generating parameterized paths for subqueries
 * by pushing join clauses down into them; it seems too expensive to re-plan
 * the subquery multiple times to consider different alternatives.
 * (XXX that could stand to be reconsidered, now that we use Paths.)
 * So the paths made here will be parameterized if the subquery contains
 * LATERAL references, otherwise not.  As long as that's true, there's no need
 * for a separate set_subquery_size phase: just make the paths right away.
 */
static void
set_subquery_pathlist(PlannerInfo *root, RelOptInfo *rel, Index rti, RangeTblEntry *rte)
{















































































































































































}

/*
 * set_function_pathlist
 *		Build the (single) access path for a function RTE
 */
static void
set_function_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{





















































}

/*
 * set_values_pathlist
 *		Build the (single) access path for a VALUES RTE
 */
static void
set_values_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{











}

/*
 * set_tablefunc_pathlist
 *		Build the (single) access path for a table func RTE
 */
static void
set_tablefunc_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{











}

/*
 * set_cte_pathlist
 *		Build the (single) access path for a non-self-reference CTE RTE
 *
 * There's no need for a separate set_cte_size phase, since we don't
 * support join-qual-parameterized paths for CTEs.
 */
static void
set_cte_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{

































































}

/*
 * set_namedtuplestore_pathlist
 *		Build the (single) access path for a named tuplestore RTE
 *
 * There's no need for a separate set_namedtuplestore_size phase, since we
 * don't support join-qual-parameterized paths for tuplestores.
 */
static void
set_namedtuplestore_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{

















}

/*
 * set_result_pathlist
 *		Build the (single) access path for an RTE_RESULT RTE
 *
 * There's no need for a separate set_result_size phase, since we
 * don't support join-qual-parameterized paths for these RTEs.
 */
static void
set_result_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{

















}

/*
 * set_worktable_pathlist
 *		Build the (single) access path for a self-reference CTE RTE
 *
 * There's no need for a separate set_worktable_size phase, since we don't
 * support join-qual-parameterized paths for CTEs.
 */
static void
set_worktable_pathlist(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{












































}

/*
 * generate_gather_paths
 *		Generate parallel access paths for a relation by pushing a
 *Gather or Gather Merge on top of a partial path.
 *
 * This must not be called until after we're done creating all partial paths
 * for the specified relation.  (Otherwise, add_partial_path might delete a
 * path that some GatherPath or GatherMergePath has a reference to.)
 *
 * If we're generating paths for a scan or join relation, override_rows will
 * be false, and we'll just use the relation's size estimate.  When we're
 * being called for a partially-grouped path, though, we need to override
 * the rowcount estimate.  (It's not clear that the particular value we're
 * using here is actually best, but the underlying rel has no estimate so
 * we must do something.)
 */
void
generate_gather_paths(PlannerInfo *root, RelOptInfo *rel, bool override_rows)
{














































}

/*
 * make_rel_from_joinlist
 *	  Build access paths using a "joinlist" to guide the join path search.
 *
 * See comments for deconstruct_jointree() for definition of the joinlist
 * data structure.
 */
static RelOptInfo *
make_rel_from_joinlist(PlannerInfo *root, List *joinlist)
{














































































}

/*
 * standard_join_search
 *	  Find possible joinpaths for a query by successively finding ways
 *	  to join component relations into join relations.
 *
 * 'levels_needed' is the number of iterations needed, ie, the number of
 *		independent jointree items in the query.  This is > 1.
 *
 * 'initial_rels' is a list of RelOptInfo nodes for each independent
 *		jointree item.  These are the components to be joined together.
 *		Note that levels_needed == list_length(initial_rels).
 *
 * Returns the final level of join relations, i.e., the relation that is
 * the result of joining all the original relations together.
 * At least one implementation path must be provided for this relation and
 * all required sub-relations.
 *
 * To support loadable plugins that modify planner behavior by changing the
 * join searching algorithm, we provide a hook variable that lets a plugin
 * replace or supplement this function.  Any such hook must return the same
 * final join relation as the standard code would, but it might have a
 * different set of implementation paths attached, and only the sub-joinrels
 * needed for these paths need have been instantiated.
 *
 * Note to plugin authors: the functions invoked during standard_join_search()
 * modify root->join_rel_list and root->join_rel_hash.  If you want to do more
 * than one join-order search, you'll probably need to save and restore the
 * original states of those data structures.  See geqo_eval() for an example.
 */
RelOptInfo *
standard_join_search(PlannerInfo *root, int levels_needed, List *initial_rels)
{




















































































}

/*****************************************************************************
 *			PUSHING QUALS DOWN INTO SUBQUERIES
 *****************************************************************************/

/*
 * subquery_is_pushdown_safe - is a subquery safe for pushing down quals?
 *
 * subquery is the particular component query being checked.  topquery
 * is the top component of a set-operations tree (the same Query if no
 * set-op is involved).
 *
 * Conditions checked here:
 *
 * 1. If the subquery has a LIMIT clause, we must not push down any quals,
 * since that could change the set of rows returned.
 *
 * 2. If the subquery contains EXCEPT or EXCEPT ALL set ops we cannot push
 * quals into it, because that could change the results.
 *
 * 3. If the subquery uses DISTINCT, we cannot push volatile quals into it.
 * This is because upper-level quals should semantically be evaluated only
 * once per distinct row, not once per original row, and if the qual is
 * volatile then extra evaluations could change the results.  (This issue
 * does not apply to other forms of aggregation such as GROUP BY, because
 * when those are present we push into HAVING not WHERE, so that the quals
 * are still applied after aggregation.)
 *
 * 4. If the subquery contains window functions, we cannot push volatile quals
 * into it.  The issue here is a bit different from DISTINCT: a volatile qual
 * might succeed for some rows of a window partition and fail for others,
 * thereby changing the partition contents and thus the window functions'
 * results for rows that remain.
 *
 * 5. If the subquery contains any set-returning functions in its targetlist,
 * we cannot push volatile quals into it.  That would push them below the SRFs
 * and thereby change the number of times they are evaluated.  Also, a
 * volatile qual could succeed for some SRF output rows and fail for others,
 * a behavior that cannot occur if it's evaluated before SRF expansion.
 *
 * 6. If the subquery has nonempty grouping sets, we cannot push down any
 * quals.  The concern here is that a qual referencing a "constant" grouping
 * column could get constant-folded, which would be improper because the value
 * is potentially nullable by grouping-set expansion.  This restriction could
 * be removed if we had a parsetree representation that shows that such
 * grouping columns are not really constant.  (There are other ideas that
 * could be used to relax this restriction, but that's the approach most
 * likely to get taken in the future.  Note that there's not much to be gained
 * so long as subquery_planner can't move HAVING clauses to WHERE within such
 * a subquery.)
 *
 * In addition, we make several checks on the subquery's output columns to see
 * if it is safe to reference them in pushed-down quals.  If output column k
 * is found to be unsafe to reference, we set safetyInfo->unsafeColumns[k]
 * to true, but we don't reject the subquery overall since column k might not
 * be referenced by some/all quals.  The unsafeColumns[] array will be
 * consulted later by qual_is_pushdown_safe().  It's better to do it this way
 * than to make the checks directly in qual_is_pushdown_safe(), because when
 * the subquery involves set operations we have to check the output
 * expressions in each arm of the set op.
 *
 * Note: pushing quals into a DISTINCT subquery is theoretically dubious:
 * we're effectively assuming that the quals cannot distinguish values that
 * the DISTINCT's equality operator sees as equal, yet there are many
 * counterexamples to that assumption.  However use of such a qual with a
 * DISTINCT subquery would be unsafe anyway, since there's no guarantee which
 * "equal" value will be chosen as the output value by the DISTINCT operation.
 * So we don't worry too much about that.  Another objection is that if the
 * qual is expensive to evaluate, running it for each original row might cost
 * more than we save by eliminating rows before the DISTINCT step.  But it
 * would be very hard to estimate that at this stage, and in practice pushdown
 * seldom seems to make things worse, so we ignore that problem too.
 *
 * Note: likewise, pushing quals into a subquery with window functions is a
 * bit dubious: the quals might remove some rows of a window partition while
 * leaving others, causing changes in the window functions' results for the
 * surviving rows.  We insist that such a qual reference only partitioning
 * columns, but again that only protects us if the qual does not distinguish
 * values that the partitioning equality operator sees as equal.  The risks
 * here are perhaps larger than for DISTINCT, since no de-duplication of rows
 * occurs and thus there is no theoretical problem with such a qual.  But
 * we'll do this anyway because the potential performance benefits are very
 * large, and we've seen no field complaints about the longstanding comparable
 * behavior with DISTINCT.
 */
static bool
subquery_is_pushdown_safe(Query *subquery, Query *topquery, pushdown_safety_info *safetyInfo)
{
























































}

/*
 * Helper routine to recurse through setOperations tree
 */
static bool
recurse_pushdown_safe(Node *setOp, Query *topquery, pushdown_safety_info *safetyInfo)
{

































}

/*
 * check_output_expressions - check subquery's output expressions for safety
 *
 * There are several cases in which it's unsafe to push down an upper-level
 * qual if it references a particular output column of a subquery.  We check
 * each output column of the subquery and set unsafeColumns[k] to true if
 * that column is unsafe for a pushed-down qual to reference.  The conditions
 * checked here are:
 *
 * 1. We must not push down any quals that refer to subselect outputs that
 * return sets, else we'd introduce functions-returning-sets into the
 * subquery's WHERE/HAVING quals.
 *
 * 2. We must not push down any quals that refer to subselect outputs that
 * contain volatile functions, for fear of introducing strange results due
 * to multiple evaluation of a volatile function.
 *
 * 3. If the subquery uses DISTINCT ON, we must not push down any quals that
 * refer to non-DISTINCT output columns, because that could change the set
 * of rows returned.  (This condition is vacuous for DISTINCT, because then
 * there are no non-DISTINCT output columns, so we needn't check.  Note that
 * subquery_is_pushdown_safe already reported that we can't use volatile
 * quals if there's DISTINCT or DISTINCT ON.)
 *
 * 4. If the subquery has any window functions, we must not push down quals
 * that reference any output columns that are not listed in all the subquery's
 * window PARTITION BY clauses.  We can push down quals that use only
 * partitioning columns because they should succeed or fail identically for
 * every row of any one window partition, and totally excluding some
 * partitions will not change a window function's results for remaining
 * partitions.  (Again, this also requires nonvolatile quals, but
 * subquery_is_pushdown_safe handles that.)
 */
static void
check_output_expressions(Query *subquery, pushdown_safety_info *safetyInfo)
{















































}

/*
 * For subqueries using UNION/UNION ALL/INTERSECT/INTERSECT ALL, we can
 * push quals into each component query, but the quals can only reference
 * subquery columns that suffer no type coercions in the set operation.
 * Otherwise there are possible semantic gotchas.  So, we check the
 * component queries to see if any of them have output types different from
 * the top-level setop outputs.  unsafeColumns[k] is set true if column k
 * has different type in any component.
 *
 * We don't have to care about typmods here: the only allowed difference
 * between set-op input and output typmods is input is a specific typmod
 * and output is -1, and that does not require a coercion.
 *
 * tlist is a subquery tlist.
 * colTypes is an OID list of the top-level setop's output column types.
 * safetyInfo->unsafeColumns[] is the result array.
 */
static void
compare_tlist_datatypes(List *tlist, List *colTypes, pushdown_safety_info *safetyInfo)
{

























}

/*
 * targetIsInAllPartitionLists
 *		True if the TargetEntry is listed in the PARTITION BY clause
 *		of every window defined in the query.
 *
 * It would be safe to ignore windows not actually used by any window
 * function, but it's not easy to get that info at this stage; and it's
 * unlikely to be useful to spend any extra cycles getting it, since
 * unreferenced window definitions are probably infrequent in practice.
 */
static bool
targetIsInAllPartitionLists(TargetEntry *tle, Query *query)
{












}

/*
 * qual_is_pushdown_safe - is a particular qual safe to push down?
 *
 * qual is a restriction clause applying to the given subquery (whose RTE
 * has index rti in the parent query).
 *
 * Conditions checked here:
 *
 * 1. The qual must not contain any SubPlans (mainly because I'm not sure
 * it will work correctly: SubLinks will already have been transformed into
 * SubPlans in the qual, but not in the subquery).  Note that SubLinks that
 * transform to initplans are safe, and will be accepted here because what
 * we'll see in the qual is just a Param referencing the initplan output.
 *
 * 2. If unsafeVolatile is set, the qual must not contain any volatile
 * functions.
 *
 * 3. If unsafeLeaky is set, the qual must not contain any leaky functions
 * that are passed Var nodes, and therefore might reveal values from the
 * subquery as side effects.
 *
 * 4. The qual must not refer to the whole-row output of the subquery
 * (since there is no easy way to name that within the subquery itself).
 *
 * 5. The qual must not refer to any subquery output columns that were
 * found to be unsafe to reference by subquery_is_pushdown_safe().
 */
static bool
qual_is_pushdown_safe(Query *subquery, Index rti, Node *qual, pushdown_safety_info *safetyInfo)
{






















































































}

/*
 * subquery_push_qual - push down a qual that we have determined is safe
 */
static void
subquery_push_qual(Query *subquery, RangeTblEntry *rte, Index rti, Node *qual)
{






































}

/*
 * Helper routine to recurse through setOperations tree
 */
static void
recurse_push_qual(Node *setOp, Query *topquery, RangeTblEntry *rte, Index rti, Node *qual)
{




















}

/*****************************************************************************
 *			SIMPLIFYING SUBQUERY TARGETLISTS
 *****************************************************************************/

/*
 * remove_unused_subquery_outputs
 *		Remove subquery targetlist items we don't need
 *
 * It's possible, even likely, that the upper query does not read all the
 * output columns of the subquery.  We can remove any such outputs that are
 * not needed by the subquery itself (e.g., as sort/group columns) and do not
 * affect semantics otherwise (e.g., volatile functions can't be removed).
 * This is useful not only because we might be able to remove expensive-to-
 * compute expressions, but because deletion of output columns might allow
 * optimizations such as join removal to occur within the subquery.
 *
 * To avoid affecting column numbering in the targetlist, we don't physically
 * remove unused tlist entries, but rather replace their expressions with NULL
 * constants.  This is implemented by modifying subquery->targetList.
 */
static void
remove_unused_subquery_outputs(Query *subquery, RelOptInfo *rel)
{










































































































}

/*
 * create_partial_bitmap_paths
 *	  Build partial bitmap heap path for the relation
 */
void
create_partial_bitmap_paths(PlannerInfo *root, RelOptInfo *rel, Path *bitmapqual)
{














}

/*
 * Compute the number of parallel workers that should be used to scan a
 * relation.  We compute the parallel workers based on the size of the heap to
 * be scanned and the size of the index to be scanned, then choose a minimum
 * of those.
 *
 * "heap_pages" is the number of pages from the table that we expect to scan, or
 * -1 if we don't expect to scan any.
 *
 * "index_pages" is the number of pages from the index that we expect to scan,
 * or -1 if we don't expect to scan any.
 *
 * "max_workers" is caller's limit on the number of workers.  This typically
 * comes from a GUC.
 */
int
compute_parallel_worker(RelOptInfo *rel, double heap_pages, double index_pages, int max_workers)
{



















































































}

/*
 * generate_partitionwise_join_paths
 * 		Create paths representing partitionwise join for given
 * partitioned join relation.
 *
 * This must not be called until after we are done adding paths for all
 * child-joins. Otherwise, add_path might delete a path to which some path
 * generated here has a reference.
 */
void
generate_partitionwise_join_paths(PlannerInfo *root, RelOptInfo *rel)
{













































































}

/*****************************************************************************
 *			DEBUG SUPPORT
 *****************************************************************************/

#ifdef OPTIMIZER_DEBUG

static void
print_relids(PlannerInfo *root, Relids relids)
{
  int x;
  bool first = true;

  x = -1;
  while ((x = bms_next_member(relids, x)) >= 0)
  {
    if (!first)
    {
      printf(" ");
    }
    if (x < root->simple_rel_array_size && root->simple_rte_array[x])
    {
      printf("%s", root->simple_rte_array[x]->eref->aliasname);
    }
    else
    {
      printf("%d", x);
    }
    first = false;
  }
}

static void
print_restrictclauses(PlannerInfo *root, List *clauses)
{
  ListCell *l;

  foreach (l, clauses)
  {
    RestrictInfo *c = lfirst(l);

    print_expr((Node *)c->clause, root->parse->rtable);
    if (lnext(l))
    {
      printf(", ");
    }
  }
}

static void
print_path(PlannerInfo *root, Path *path, int indent)
{
  const char *ptype;
  bool join = false;
  Path *subpath = NULL;
  int i;

  switch (nodeTag(path))
  {
  case T_Path:
    switch (path->pathtype)
    {
    case T_SeqScan:
      ptype = "SeqScan";
      break;
    case T_SampleScan:
      ptype = "SampleScan";
      break;
    case T_FunctionScan:
      ptype = "FunctionScan";
      break;
    case T_TableFuncScan:
      ptype = "TableFuncScan";
      break;
    case T_ValuesScan:
      ptype = "ValuesScan";
      break;
    case T_CteScan:
      ptype = "CteScan";
      break;
    case T_NamedTuplestoreScan:
      ptype = "NamedTuplestoreScan";
      break;
    case T_Result:
      ptype = "Result";
      break;
    case T_WorkTableScan:
      ptype = "WorkTableScan";
      break;
    default:;
      ptype = "???Path";
      break;
    }
    break;
  case T_IndexPath:
    ptype = "IdxScan";
    break;
  case T_BitmapHeapPath:
    ptype = "BitmapHeapScan";
    break;
  case T_BitmapAndPath:
    ptype = "BitmapAndPath";
    break;
  case T_BitmapOrPath:
    ptype = "BitmapOrPath";
    break;
  case T_TidPath:
    ptype = "TidScan";
    break;
  case T_SubqueryScanPath:
    ptype = "SubqueryScan";
    break;
  case T_ForeignPath:
    ptype = "ForeignScan";
    break;
  case T_CustomPath:
    ptype = "CustomScan";
    break;
  case T_NestPath:
    ptype = "NestLoop";
    join = true;
    break;
  case T_MergePath:
    ptype = "MergeJoin";
    join = true;
    break;
  case T_HashPath:
    ptype = "HashJoin";
    join = true;
    break;
  case T_AppendPath:
    ptype = "Append";
    break;
  case T_MergeAppendPath:
    ptype = "MergeAppend";
    break;
  case T_GroupResultPath:
    ptype = "GroupResult";
    break;
  case T_MaterialPath:
    ptype = "Material";
    subpath = ((MaterialPath *)path)->subpath;
    break;
  case T_UniquePath:
    ptype = "Unique";
    subpath = ((UniquePath *)path)->subpath;
    break;
  case T_GatherPath:
    ptype = "Gather";
    subpath = ((GatherPath *)path)->subpath;
    break;
  case T_GatherMergePath:
    ptype = "GatherMerge";
    subpath = ((GatherMergePath *)path)->subpath;
    break;
  case T_ProjectionPath:
    ptype = "Projection";
    subpath = ((ProjectionPath *)path)->subpath;
    break;
  case T_ProjectSetPath:
    ptype = "ProjectSet";
    subpath = ((ProjectSetPath *)path)->subpath;
    break;
  case T_SortPath:
    ptype = "Sort";
    subpath = ((SortPath *)path)->subpath;
    break;
  case T_GroupPath:
    ptype = "Group";
    subpath = ((GroupPath *)path)->subpath;
    break;
  case T_UpperUniquePath:
    ptype = "UpperUnique";
    subpath = ((UpperUniquePath *)path)->subpath;
    break;
  case T_AggPath:
    ptype = "Agg";
    subpath = ((AggPath *)path)->subpath;
    break;
  case T_GroupingSetsPath:
    ptype = "GroupingSets";
    subpath = ((GroupingSetsPath *)path)->subpath;
    break;
  case T_MinMaxAggPath:
    ptype = "MinMaxAgg";
    break;
  case T_WindowAggPath:
    ptype = "WindowAgg";
    subpath = ((WindowAggPath *)path)->subpath;
    break;
  case T_SetOpPath:
    ptype = "SetOp";
    subpath = ((SetOpPath *)path)->subpath;
    break;
  case T_RecursiveUnionPath:
    ptype = "RecursiveUnion";
    break;
  case T_LockRowsPath:
    ptype = "LockRows";
    subpath = ((LockRowsPath *)path)->subpath;
    break;
  case T_ModifyTablePath:
    ptype = "ModifyTable";
    break;
  case T_LimitPath:
    ptype = "Limit";
    subpath = ((LimitPath *)path)->subpath;
    break;
  default:;
    ptype = "???Path";
    break;
  }

  for (i = 0; i < indent; i++)
  {
    printf("\t");
  }
  printf("%s", ptype);

  if (path->parent)
  {
    printf("(");
    print_relids(root, path->parent->relids);
    printf(")");
  }
  if (path->param_info)
  {
    printf(" required_outer (");
    print_relids(root, path->param_info->ppi_req_outer);
    printf(")");
  }
  printf(" rows=%.0f cost=%.2f..%.2f\n", path->rows, path->startup_cost, path->total_cost);

  if (path->pathkeys)
  {
    for (i = 0; i < indent; i++)
    {
      printf("\t");
    }
    printf("  pathkeys: ");
    print_pathkeys(path->pathkeys, root->parse->rtable);
  }

  if (join)
  {
    JoinPath *jp = (JoinPath *)path;

    for (i = 0; i < indent; i++)
    {
      printf("\t");
    }
    printf("  clauses: ");
    print_restrictclauses(root, jp->joinrestrictinfo);
    printf("\n");

    if (IsA(path, MergePath))
    {
      MergePath *mp = (MergePath *)path;

      for (i = 0; i < indent; i++)
      {
        printf("\t");
      }
      printf("  sortouter=%d sortinner=%d materializeinner=%d\n", ((mp->outersortkeys) ? 1 : 0), ((mp->innersortkeys) ? 1 : 0), ((mp->materialize_inner) ? 1 : 0));
    }

    print_path(root, jp->outerjoinpath, indent + 1);
    print_path(root, jp->innerjoinpath, indent + 1);
  }

  if (subpath)
  {
    print_path(root, subpath, indent + 1);
  }
}

void
debug_print_rel(PlannerInfo *root, RelOptInfo *rel)
{
  ListCell *l;

  printf("RELOPTINFO (");
  print_relids(root, rel->relids);
  printf("): rows=%.0f width=%d\n", rel->rows, rel->reltarget->width);

  if (rel->baserestrictinfo)
  {
    printf("\tbaserestrictinfo: ");
    print_restrictclauses(root, rel->baserestrictinfo);
    printf("\n");
  }

  if (rel->joininfo)
  {
    printf("\tjoininfo: ");
    print_restrictclauses(root, rel->joininfo);
    printf("\n");
  }

  printf("\tpath list:\n");
  foreach (l, rel->pathlist)
  {
    print_path(root, lfirst(l), 1);
  }
  if (rel->cheapest_parameterized_paths)
  {
    printf("\n\tcheapest parameterized paths:\n");
    foreach (l, rel->cheapest_parameterized_paths)
    {
      print_path(root, lfirst(l), 1);
    }
  }
  if (rel->cheapest_startup_path)
  {
    printf("\n\tcheapest startup path:\n");
    print_path(root, rel->cheapest_startup_path, 1);
  }
  if (rel->cheapest_total_path)
  {
    printf("\n\tcheapest total path:\n");
    print_path(root, rel->cheapest_total_path, 1);
  }
  printf("\n");
  fflush(stdout);
}

#endif /* OPTIMIZER_DEBUG */