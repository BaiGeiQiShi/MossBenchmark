/*-------------------------------------------------------------------------
 *
 * planner.c
 *	  The query optimizer external interface.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/planner.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/parallel.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "executor/nodeAgg.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "jit/jit.h"
#include "lib/bipartite_match.h"
#include "lib/knapsack.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#ifdef OPTIMIZER_DEBUG
#include "nodes/print.h"
#endif
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/inherit.h"
#include "optimizer/optimizer.h"
#include "optimizer/paramassign.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "parser/analyze.h"
#include "parser/parsetree.h"
#include "parser/parse_agg.h"
#include "partitioning/partdesc.h"
#include "rewrite/rewriteManip.h"
#include "storage/dsm_impl.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/* GUC parameters */
double cursor_tuple_fraction = DEFAULT_CURSOR_TUPLE_FRACTION;
int force_parallel_mode = FORCE_PARALLEL_OFF;
bool parallel_leader_participation = true;

/* Hook for plugins to get control in planner() */
planner_hook_type planner_hook = NULL;

/* Hook for plugins to get control when grouping_planner() plans upper rels */
create_upper_paths_hook_type create_upper_paths_hook = NULL;

/* Expression kind codes for preprocess_expression */
#define EXPRKIND_QUAL 0
#define EXPRKIND_TARGET 1
#define EXPRKIND_RTFUNC 2
#define EXPRKIND_RTFUNC_LATERAL 3
#define EXPRKIND_VALUES 4
#define EXPRKIND_VALUES_LATERAL 5
#define EXPRKIND_LIMIT 6
#define EXPRKIND_APPINFO 7
#define EXPRKIND_PHV 8
#define EXPRKIND_TABLESAMPLE 9
#define EXPRKIND_ARBITER_ELEM 10
#define EXPRKIND_TABLEFUNC 11
#define EXPRKIND_TABLEFUNC_LATERAL 12

/* Passthrough data for standard_qp_callback */
typedef struct
{
  List *activeWindows; /* active windows, if any */
  List *groupClause;   /* overrides parse->groupClause */
} standard_qp_extra;

/*
 * Data specific to grouping sets
 */

typedef struct
{
  List *rollups;
  List *hash_sets_idx;
  double dNumHashGroups;
  bool any_hashable;
  Bitmapset *unsortable_refs;
  Bitmapset *unhashable_refs;
  List *unsortable_sets;
  int *tleref_to_colnum_map;
} grouping_sets_data;

/*
 * Temporary structure for use during WindowClause reordering in order to be
 * able to sort WindowClauses on partitioning/ordering prefix.
 */
typedef struct
{
  WindowClause *wc;
  List *uniqueOrder; /* A List of unique ordering/partitioning
                      * clauses per Window */
} WindowClauseSortData;

/* Local functions */
static Node *
preprocess_expression(PlannerInfo *root, Node *expr, int kind);
static void
preprocess_qual_conditions(PlannerInfo *root, Node *jtnode);
static void
inheritance_planner(PlannerInfo *root);
static void
grouping_planner(PlannerInfo *root, bool inheritance_update, double tuple_fraction);
static grouping_sets_data *
preprocess_grouping_sets(PlannerInfo *root);
static List *
remap_to_groupclause_idx(List *groupClause, List *gsets, int *tleref_to_colnum_map);
static void
preprocess_rowmarks(PlannerInfo *root);
static double
preprocess_limit(PlannerInfo *root, double tuple_fraction, int64 *offset_est, int64 *count_est);
static void
remove_useless_groupby_columns(PlannerInfo *root);
static List *
preprocess_groupclause(PlannerInfo *root, List *force);
static List *
extract_rollup_sets(List *groupingSets);
static List *
reorder_grouping_sets(List *groupingSets, List *sortclause);
static void
standard_qp_callback(PlannerInfo *root, void *extra);
static double
get_number_of_groups(PlannerInfo *root, double path_rows, grouping_sets_data *gd, List *target_list);
static RelOptInfo *
create_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, const AggClauseCosts *agg_costs, grouping_sets_data *gd);
static bool
is_degenerate_grouping(PlannerInfo *root);
static void
create_degenerate_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel);
static RelOptInfo *
make_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, Node *havingQual);
static void
create_ordinary_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, GroupPathExtraData *extra, RelOptInfo **partially_grouped_rel_p);
static void
consider_groupingsets_paths(PlannerInfo *root, RelOptInfo *grouped_rel, Path *path, bool is_sorted, bool can_hash, grouping_sets_data *gd, const AggClauseCosts *agg_costs, double dNumGroups);
static RelOptInfo *
create_window_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *input_target, PathTarget *output_target, bool output_target_parallel_safe, WindowFuncLists *wflists, List *activeWindows);
static void
create_one_window_path(PlannerInfo *root, RelOptInfo *window_rel, Path *path, PathTarget *input_target, PathTarget *output_target, WindowFuncLists *wflists, List *activeWindows);
static RelOptInfo *
create_distinct_paths(PlannerInfo *root, RelOptInfo *input_rel);
static RelOptInfo *
create_ordered_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, double limit_tuples);
static PathTarget *
make_group_input_target(PlannerInfo *root, PathTarget *final_target);
static PathTarget *
make_partial_grouping_target(PlannerInfo *root, PathTarget *grouping_target, Node *havingQual);
static List *
postprocess_setop_tlist(List *new_tlist, List *orig_tlist);
static List *
select_active_windows(PlannerInfo *root, WindowFuncLists *wflists);
static PathTarget *
make_window_input_target(PlannerInfo *root, PathTarget *final_target, List *activeWindows);
static List *
make_pathkeys_for_window(PlannerInfo *root, WindowClause *wc, List *tlist);
static PathTarget *
make_sort_input_target(PlannerInfo *root, PathTarget *final_target, bool *have_postponed_srfs);
static void
adjust_paths_for_srfs(PlannerInfo *root, RelOptInfo *rel, List *targets, List *targets_contain_srfs);
static void
add_paths_to_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, double dNumGroups, GroupPathExtraData *extra);
static RelOptInfo *
create_partial_grouping_paths(PlannerInfo *root, RelOptInfo *grouped_rel, RelOptInfo *input_rel, grouping_sets_data *gd, GroupPathExtraData *extra, bool force_rel_creation);
static void
gather_grouping_paths(PlannerInfo *root, RelOptInfo *rel);
static bool
can_partial_agg(PlannerInfo *root, const AggClauseCosts *agg_costs);
static void
apply_scanjoin_target_to_paths(PlannerInfo *root, RelOptInfo *rel, List *scanjoin_targets, List *scanjoin_targets_contain_srfs, bool scanjoin_target_parallel_safe, bool tlist_same_exprs);
static void
create_partitionwise_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, PartitionwiseAggregateType patype, GroupPathExtraData *extra);
static bool
group_by_has_partkey(RelOptInfo *input_rel, List *targetList, List *groupClause);
static int
common_prefix_cmp(const void *a, const void *b);

/*****************************************************************************
 *
 *	   Query optimizer entry point
 *
 * To support loadable plugins that monitor or modify planner behavior,
 * we provide a hook variable that lets a plugin get control before and
 * after the standard planning process.  The plugin would normally call
 * standard_planner().
 *
 * Note to plugin authors: standard_planner() scribbles on its Query input,
 * so you'd better copy that data structure if you want to plan more than once.
 *
 *****************************************************************************/
PlannedStmt *
planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{











}

PlannedStmt *
standard_planner(Query *parse, int cursorOptions, ParamListInfo boundParams)
{





























































































































































































































































































}

/*--------------------
 * subquery_planner
 *	  Invokes the planner on a subquery.  We recurse to here for each
 *	  sub-SELECT found in the query tree.
 *
 * glob is the global state for the current planner run.
 * parse is the querytree produced by the parser & rewriter.
 * parent_root is the immediate parent Query's info (NULL at the top level).
 * hasRecursion is true if this is a recursive WITH query.
 * tuple_fraction is the fraction of tuples we expect will be retrieved.
 * tuple_fraction is interpreted as explained for grouping_planner, below.
 *
 * Basically, this routine does the stuff that should only be done once
 * per Query object.  It then calls grouping_planner.  At one time,
 * grouping_planner could be invoked recursively on the same Query object;
 * that's not currently true, but we keep the separation between the two
 * routines anyway, in case we need it again someday.
 *
 * subquery_planner will be called recursively to handle sub-Query nodes
 * found within the query's expressions and rangetable.
 *
 * Returns the PlannerInfo struct ("root") that contains all data generated
 * while planning the subquery.  In particular, the Path(s) attached to
 * the (UPPERREL_FINAL, NULL) upperrel represent our conclusions about the
 * cheapest way(s) to implement the query.  The top level will select the
 * best Path and pass it through createplan.c to produce a finished Plan.
 *--------------------
 */
PlannerInfo *
subquery_planner(PlannerGlobal *glob, Query *parse, PlannerInfo *parent_root, bool hasRecursion, double tuple_fraction)
{











































































































































































































































































































































































































































}

/*
 * preprocess_expression
 *		Do subquery_planner's preprocessing work for an expression,
 *		which can be a targetlist, a WHERE clause (including JOIN/ON
 *		conditions), a HAVING clause, or a few other things.
 */
static Node *
preprocess_expression(PlannerInfo *root, Node *expr, int kind)
{



















































































}

/*
 * preprocess_qual_conditions
 *		Recursively scan the query's jointree and do subquery_planner's
 *		preprocessing work on each qual condition found therein.
 */
static void
preprocess_qual_conditions(PlannerInfo *root, Node *jtnode)
{

































}

/*
 * preprocess_phv_expression
 *	  Do preprocessing on a PlaceHolderVar expression that's been pulled up.
 *
 * If a LATERAL subquery references an output of another subquery, and that
 * output must be wrapped in a PlaceHolderVar because of an intermediate outer
 * join, then we'll push the PlaceHolderVar expression down into the subquery
 * and later pull it back up during find_lateral_references, which runs after
 * subquery_planner has preprocessed all the expressions that were in the
 * current query level to start with.  So we need to preprocess it then.
 */
Expr *
preprocess_phv_expression(PlannerInfo *root, Expr *expr)
{

}

/*
 * inheritance_planner
 *	  Generate Paths in the case where the result relation is an
 *	  inheritance set.
 *
 * We have to handle this case differently from cases where a source relation
 * is an inheritance set. Source inheritance is expanded at the bottom of the
 * plan tree (see allpaths.c), but target inheritance has to be expanded at
 * the top.  The reason is that for UPDATE, each target relation needs a
 * different targetlist matching its own column set.  Fortunately,
 * the UPDATE/DELETE target can never be the nullable side of an outer join,
 * so it's OK to generate the plan this way.
 *
 * Returns nothing; the useful output is in the Paths we attach to
 * the (UPPERREL_FINAL, NULL) upperrel stored in *root.
 *
 * Note that we have not done set_cheapest() on the final rel; it's convenient
 * to leave this to the caller.
 */
static void
inheritance_planner(PlannerInfo *root)
{














































































































































































































































































































































































































































































































































































































}

/*--------------------
 * grouping_planner
 *	  Perform planning steps related to grouping, aggregation, etc.
 *
 * This function adds all required top-level processing to the scan/join
 * Path(s) produced by query_planner.
 *
 * If inheritance_update is true, we're being called from inheritance_planner
 * and should not include a ModifyTable step in the resulting Path(s).
 * (inheritance_planner will create a single ModifyTable node covering all the
 * target tables.)
 *
 * tuple_fraction is the fraction of tuples we expect will be retrieved.
 * tuple_fraction is interpreted as follows:
 *	  0: expect all tuples to be retrieved (normal case)
 *	  0 < tuple_fraction < 1: expect the given fraction of tuples available
 *		from the plan to be retrieved
 *	  tuple_fraction >= 1: tuple_fraction is the absolute number of tuples
 *		expected to be retrieved (ie, a LIMIT specification)
 *
 * Returns nothing; the useful output is in the Paths we attach to the
 * (UPPERREL_FINAL, NULL) upperrel in *root.  In addition,
 * root->processed_tlist contains the final processed targetlist.
 *
 * Note that we have not done set_cheapest() on the final rel; it's convenient
 * to leave this to the caller.
 *--------------------
 */
static void
grouping_planner(PlannerInfo *root, bool inheritance_update, double tuple_fraction)
{
































































































































































































































































































































































































































































































































































}

/*
 * Do preprocessing for groupingSets clause and related data.  This handles the
 * preliminary steps of expanding the grouping sets, organizing them into lists
 * of rollups, and preparing annotations which will later be filled in with
 * size estimates.
 */
static grouping_sets_data *
preprocess_grouping_sets(PlannerInfo *root)
{















































































































































































}

/*
 * Given a groupclause and a list of GroupingSetData, return equivalent sets
 * (without annotation) mapped to indexes into the given groupclause.
 */
static List *
remap_to_groupclause_idx(List *groupClause, List *gsets, int *tleref_to_colnum_map)
{


























}

/*
 * preprocess_rowmarks - set up PlanRowMarks if needed
 */
static void
preprocess_rowmarks(PlannerInfo *root)
{













































































































}

/*
 * Select RowMarkType to use for a given table
 */
RowMarkType
select_rowmark_type(RangeTblEntry *rte, LockClauseStrength strength)
{














































}

/*
 * preprocess_limit - do pre-estimation for LIMIT and/or OFFSET clauses
 *
 * We try to estimate the values of the LIMIT/OFFSET clauses, and pass the
 * results back in *count_est and *offset_est.  These variables are set to
 * 0 if the corresponding clause is not present, and -1 if it's present
 * but we couldn't estimate the value for it.  (The "0" convention is OK
 * for OFFSET but a little bit bogus for LIMIT: effectively we estimate
 * LIMIT 0 as though it were LIMIT 1.  But this is in line with the planner's
 * usual practice of never estimating less than one row.)  These values will
 * be passed to create_limit_path, which see if you change this code.
 *
 * The return value is the suitably adjusted tuple_fraction to use for
 * planning the query.  This adjustment is not overridable, since it reflects
 * plan actions that grouping_planner() will certainly take, not assumptions
 * about context.
 */
static double
preprocess_limit(PlannerInfo *root, double tuple_fraction, int64 *offset_est, int64 *count_est)
{























































































































































































}

/*
 * limit_needed - do we actually need a Limit plan node?
 *
 * If we have constant-zero OFFSET and constant-null LIMIT, we can skip adding
 * a Limit node.  This is worth checking for because "OFFSET 0" is a common
 * locution for an optimization fence.  (Because other places in the planner
 * merely check whether parse->limitOffset isn't NULL, it will still work as
 * an optimization fence --- we're just suppressing unnecessary run-time
 * overhead.)
 *
 * This might look like it could be merged into preprocess_limit, but there's
 * a key distinction: here we need hard constants in OFFSET/LIMIT, whereas
 * in preprocess_limit it's good enough to consider estimated values.
 */
bool
limit_needed(Query *parse)
{










































}

/*
 * remove_useless_groupby_columns
 *		Remove any columns in the GROUP BY clause that are redundant due
 *to being functionally dependent on other GROUP BY columns.
 *
 * Since some other DBMSes do not allow references to ungrouped columns, it's
 * not unusual to find all columns listed in GROUP BY even though listing the
 * primary-key columns would be sufficient.  Deleting such excess columns
 * avoids redundant sorting work, so it's worth doing.  When we do this, we
 * must mark the plan as dependent on the pkey constraint (compare the
 * parser's check_ungrouped_columns() and check_functional_grouping()).
 *
 * In principle, we could treat any NOT-NULL columns appearing in a UNIQUE
 * index as the determining columns.  But as with check_functional_grouping(),
 * there's currently no way to represent dependency on a NOT NULL constraint,
 * so we consider only the pkey for now.
 */
static void
remove_useless_groupby_columns(PlannerInfo *root)
{





















































































































































}

/*
 * preprocess_groupclause - do preparatory work on GROUP BY clause
 *
 * The idea here is to adjust the ordering of the GROUP BY elements
 * (which in itself is semantically insignificant) to match ORDER BY,
 * thereby allowing a single sort operation to both implement the ORDER BY
 * requirement and set up for a Unique step that implements GROUP BY.
 *
 * In principle it might be interesting to consider other orderings of the
 * GROUP BY elements, which could match the sort ordering of other
 * possible plans (eg an indexscan) and thereby reduce cost.  We don't
 * bother with that, though.  Hashed grouping will frequently win anyway.
 *
 * Note: we need no comparable processing of the distinctClause because
 * the parser already enforced that that matches ORDER BY.
 *
 * For grouping sets, the order of items is instead forced to agree with that
 * of the grouping set (and items not in the grouping set are skipped). The
 * work of sorting the order of grouping set elements to match the ORDER BY if
 * possible is done elsewhere.
 */
static List *
preprocess_groupclause(PlannerInfo *root, List *force)
{



























































































}

/*
 * Extract lists of grouping sets that can be implemented using a single
 * rollup-type aggregate pass each. Returns a list of lists of grouping sets.
 *
 * Input must be sorted with smallest sets first. Result has each sublist
 * sorted with smallest sets first.
 *
 * We want to produce the absolute minimum possible number of lists here to
 * avoid excess sorts. Fortunately, there is an algorithm for this; the problem
 * of finding the minimal partition of a partially-ordered set into chains
 * (which is what we need, taking the list of grouping sets as a poset ordered
 * by set inclusion) can be mapped to the problem of finding the maximum
 * cardinality matching on a bipartite graph, which is solvable in polynomial
 * time with a worst case of no worse than O(n^2.5) and usually much
 * better. Since our N is at most 4096, we don't need to consider fallbacks to
 * heuristic or approximate methods.  (Planning time for a 12-d cube is under
 * half a second on my modest system even with optimization off and assertions
 * on.)
 */
static List *
extract_rollup_sets(List *groupingSets)
{
























































































































































































































}

/*
 * Reorder the elements of a list of grouping sets such that they have correct
 * prefix relationships. Also inserts the GroupingSetData annotations.
 *
 * The input must be ordered with smallest sets first; the result is returned
 * with largest sets first.  Note that the result shares no list substructure
 * with the input, so it's safe for the caller to modify it later.
 *
 * If we're passed in a sortclause, we follow its order of columns to the
 * extent possible, to minimize the chance that we add unnecessary sorts.
 * (We're trying here to ensure that GROUPING SETS ((a,b,c),(c)) ORDER BY c,b,a
 * gets implemented in one pass.)
 */
static List *
reorder_grouping_sets(List *groupingsets, List *sortclause)
{









































}

/*
 * Compute query_pathkeys and other pathkeys during plan generation
 */
static void
standard_qp_callback(PlannerInfo *root, void *extra)
{
















































































}

/*
 * Estimate number of groups produced by grouping clauses (1 if not grouping)
 *
 * path_rows: number of output rows from scan/join step
 * gd: grouping sets data including list of grouping sets and their clauses
 * target_list: target list containing group clause references
 *
 * If doing grouping sets, we also annotate the gsets data with the estimates
 * for each set and each individual rollup list, with a view to later
 * determining whether some combination of them could be hashed instead.
 */
static double
get_number_of_groups(PlannerInfo *root, double path_rows, grouping_sets_data *gd, List *target_list)
{





















































































}

/*
 * create_grouping_paths
 *
 * Build a new upperrel containing Paths for grouping and/or aggregation.
 * Along the way, we also build an upperrel for Paths which are partially
 * grouped and/or aggregated.  A partially grouped and/or aggregated path
 * needs a FinalizeAggregate node to complete the aggregation.  Currently,
 * the only partially grouped paths we build are also partial paths; that
 * is, they need a Gather and then a FinalizeAggregate.
 *
 * input_rel: contains the source-data Paths
 * target: the pathtarget for the result Paths to compute
 * agg_costs: cost info about all aggregates in query (in AGGSPLIT_SIMPLE mode)
 * gd: grouping sets data including list of grouping sets and their clauses
 *
 * Note: all Paths in input_rel are expected to return the target computed
 * by make_group_input_target.
 */
static RelOptInfo *
create_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, const AggClauseCosts *agg_costs, grouping_sets_data *gd)
{


































































































}

/*
 * make_grouping_rel
 *
 * Create a new grouping rel and set basic properties.
 *
 * input_rel represents the underlying scan/join relation.
 * target is the output expected from the grouping relation.
 */
static RelOptInfo *
make_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, Node *havingQual)
{







































}

/*
 * is_degenerate_grouping
 *
 * A degenerate grouping is one in which the query has a HAVING qual and/or
 * grouping sets, but no aggregates and no GROUP BY (which implies that the
 * grouping sets are all empty).
 */
static bool
is_degenerate_grouping(PlannerInfo *root)
{



}

/*
 * create_degenerate_grouping_paths
 *
 * When the grouping is degenerate (see is_degenerate_grouping), we are
 * supposed to emit either zero or one row for each grouping set depending on
 * whether HAVING succeeds.  Furthermore, there cannot be any variables in
 * either HAVING or the targetlist, so we actually do not need the FROM table
 * at all! We can just throw away the plan-so-far and generate a Result node.
 * This is a sufficiently unusual corner case that it's not worth contorting
 * the structure of this module to avoid having to generate the earlier paths
 * in the first place.
 */
static void
create_degenerate_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel)
{






























}

/*
 * create_ordinary_grouping_paths
 *
 * Create grouping paths for the ordinary (that is, non-degenerate) case.
 *
 * We need to consider sorted and hashed aggregation in the same function,
 * because otherwise (1) it would be harder to throw an appropriate error
 * message if neither way works, and (2) we should not allow hashtable size
 * considerations to dissuade us from using hashing if sorting is not possible.
 *
 * *partially_grouped_rel_p will be set to the partially grouped rel which this
 * function creates, or to NULL if it doesn't create one.
 */
static void
create_ordinary_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, GroupPathExtraData *extra, RelOptInfo **partially_grouped_rel_p)
{
















































































































}

/*
 * For a given input path, consider the possible ways of doing grouping sets on
 * it, by combinations of hashing and sorting.  This can be called multiple
 * times, so it's important that it not scribble on input.  No result is
 * returned, but any generated paths are added to grouped_rel.
 */
static void
consider_groupingsets_paths(PlannerInfo *root, RelOptInfo *grouped_rel, Path *path, bool is_sorted, bool can_hash, grouping_sets_data *gd, const AggClauseCosts *agg_costs, double dNumGroups)
{

































































































































































































































































































































}

/*
 * create_window_paths
 *
 * Build a new upperrel containing Paths for window-function evaluation.
 *
 * input_rel: contains the source-data Paths
 * input_target: result of make_window_input_target
 * output_target: what the topmost WindowAggPath should return
 * wflists: result of find_window_functions
 * activeWindows: result of select_active_windows
 *
 * Note: all Paths in input_rel are expected to return input_target.
 */
static RelOptInfo *
create_window_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *input_target, PathTarget *output_target, bool output_target_parallel_safe, WindowFuncLists *wflists, List *activeWindows)
{


























































}

/*
 * Stack window-function implementation steps atop the given Path, and
 * add the result to window_rel.
 *
 * window_rel: upperrel to contain result
 * path: input Path to use (must return input_target)
 * input_target: result of make_window_input_target
 * output_target: what the topmost WindowAggPath should return
 * wflists: result of find_window_functions
 * activeWindows: result of select_active_windows
 */
static void
create_one_window_path(PlannerInfo *root, RelOptInfo *window_rel, Path *path, PathTarget *input_target, PathTarget *output_target, WindowFuncLists *wflists, List *activeWindows)
{
































































}

/*
 * create_distinct_paths
 *
 * Build a new upperrel containing Paths for SELECT DISTINCT evaluation.
 *
 * input_rel: contains the source-data Paths
 *
 * Note: input paths should already compute the desired pathtarget, since
 * Sort/Unique won't project anything.
 */
static RelOptInfo *
create_distinct_paths(PlannerInfo *root, RelOptInfo *input_rel)
{













































































































































































}

/*
 * create_ordered_paths
 *
 * Build a new upperrel containing Paths for ORDER BY evaluation.
 *
 * All paths in the result must satisfy the ORDER BY ordering.
 * The only new path we need consider is an explicit sort on the
 * cheapest-total existing path.
 *
 * input_rel: contains the source-data Paths
 * target: the output tlist the result Paths must emit
 * limit_tuples: estimated bound on the number of output tuples,
 *		or -1 if no LIMIT or couldn't estimate
 */
static RelOptInfo *
create_ordered_paths(PlannerInfo *root, RelOptInfo *input_rel, PathTarget *target, bool target_parallel_safe, double limit_tuples)
{















































































































}

/*
 * make_group_input_target
 *	  Generate appropriate PathTarget for initial input to grouping nodes.
 *
 * If there is grouping or aggregation, the scan/join subplan cannot emit
 * the query's final targetlist; for example, it certainly can't emit any
 * aggregate function calls.  This routine generates the correct target
 * for the scan/join subplan.
 *
 * The query target list passed from the parser already contains entries
 * for all ORDER BY and GROUP BY expressions, but it will not have entries
 * for variables used only in HAVING clauses; so we need to add those
 * variables to the subplan target list.  Also, we flatten all expressions
 * except GROUP BY items into their component variables; other expressions
 * will be computed by the upper plan nodes rather than by the subplan.
 * For example, given a query like
 *		SELECT a+b,SUM(c+d) FROM table GROUP BY a+b;
 * we want to pass this targetlist to the subplan:
 *		a+b,c,d
 * where the a+b target will be used by the Sort/Group steps, and the
 * other targets will be used for computing the final results.
 *
 * 'final_target' is the query's final target list (in PathTarget form)
 *
 * The result is the PathTarget to be computed by the Paths returned from
 * query_planner().
 */
static PathTarget *
make_group_input_target(PlannerInfo *root, PathTarget *final_target)
{
































































}

/*
 * make_partial_grouping_target
 *	  Generate appropriate PathTarget for output of partial aggregate
 *	  (or partial grouping, if there are no aggregates) nodes.
 *
 * A partial aggregation node needs to emit all the same aggregates that
 * a regular aggregation node would, plus any aggregates used in HAVING;
 * except that the Aggref nodes should be marked as partial aggregates.
 *
 * In addition, we'd better emit any Vars and PlaceholderVars that are
 * used outside of Aggrefs in the aggregation tlist and HAVING.  (Presumably,
 * these would be Vars that are grouped by or used in grouping expressions.)
 *
 * grouping_target is the tlist to be emitted by the topmost aggregation step.
 * havingQual represents the HAVING clause.
 */
static PathTarget *
make_partial_grouping_target(PlannerInfo *root, PathTarget *grouping_target, Node *havingQual)
{
























































































}

/*
 * mark_partial_aggref
 *	  Adjust an Aggref to make it represent a partial-aggregation step.
 *
 * The Aggref node is modified in-place; caller must do any copying required.
 */
void
mark_partial_aggref(Aggref *agg, AggSplit aggsplit)
{
























}

/*
 * postprocess_setop_tlist
 *	  Fix up targetlist returned by plan_set_operations().
 *
 * We need to transpose sort key info from the orig_tlist into new_tlist.
 * NOTE: this would not be good enough if we supported resjunk sort keys
 * for results of set operations --- then, we'd need to project a whole
 * new tlist to evaluate the resjunk columns.  For now, just ereport if we
 * find any resjunk columns in orig_tlist.
 */
static List *
postprocess_setop_tlist(List *new_tlist, List *orig_tlist)
{





























}

/*
 * select_active_windows
 *		Create a list of the "active" window clauses (ie, those
 *referenced by non-deleted WindowFuncs) in the order they are to be executed.
 */
static List *
select_active_windows(PlannerInfo *root, WindowFuncLists *wflists)
{




































































}

/*
 * common_prefix_cmp
 *	  QSort comparison function for WindowClauseSortData
 *
 * Sort the windows by the required sorting clauses. First, compare the sort
 * clauses themselves. Second, if one window's clauses are a prefix of another
 * one's clauses, put the window with more sort clauses first.
 */
static int
common_prefix_cmp(const void *a, const void *b)
{















































}

/*
 * make_window_input_target
 *	  Generate appropriate PathTarget for initial input to WindowAgg nodes.
 *
 * When the query has window functions, this function computes the desired
 * target to be computed by the node just below the first WindowAgg.
 * This tlist must contain all values needed to evaluate the window functions,
 * compute the final target list, and perform any required final sort step.
 * If multiple WindowAggs are needed, each intermediate one adds its window
 * function results onto this base tlist; only the topmost WindowAgg computes
 * the actual desired target list.
 *
 * This function is much like make_group_input_target, though not quite enough
 * like it to share code.  As in that function, we flatten most expressions
 * into their component variables.  But we do not want to flatten window
 * PARTITION BY/ORDER BY clauses, since that might result in multiple
 * evaluations of them, which would be bad (possibly even resulting in
 * inconsistent answers, if they contain volatile functions).
 * Also, we must not flatten GROUP BY clauses that were left unflattened by
 * make_group_input_target, because we may no longer have access to the
 * individual Vars in them.
 *
 * Another key difference from make_group_input_target is that we don't
 * flatten Aggref expressions, since those are to be computed below the
 * window functions and just referenced like Vars above that.
 *
 * 'final_target' is the query's final target list (in PathTarget form)
 * 'activeWindows' is the list of active windows previously identified by
 *			select_active_windows.
 *
 * The result is the PathTarget to be computed by the plan node immediately
 * below the first WindowAgg node.
 */
static PathTarget *
make_window_input_target(PlannerInfo *root, PathTarget *final_target, List *activeWindows)
{



































































































}

/*
 * make_pathkeys_for_window
 *		Create a pathkeys list describing the required input ordering
 *		for the given WindowClause.
 *
 * The required ordering is first the PARTITION keys, then the ORDER keys.
 * In the future we might try to implement windowing using hashing, in which
 * case the ordering could be relaxed, but for now we always sort.
 */
static List *
make_pathkeys_for_window(PlannerInfo *root, WindowClause *wc, List *tlist)
{


















}

/*
 * make_sort_input_target
 *	  Generate appropriate PathTarget for initial input to Sort step.
 *
 * If the query has ORDER BY, this function chooses the target to be computed
 * by the node just below the Sort (and DISTINCT, if any, since Unique can't
 * project) steps.  This might or might not be identical to the query's final
 * output target.
 *
 * The main argument for keeping the sort-input tlist the same as the final
 * is that we avoid a separate projection node (which will be needed if
 * they're different, because Sort can't project).  However, there are also
 * advantages to postponing tlist evaluation till after the Sort: it ensures
 * a consistent order of evaluation for any volatile functions in the tlist,
 * and if there's also a LIMIT, we can stop the query without ever computing
 * tlist functions for later rows, which is beneficial for both volatile and
 * expensive functions.
 *
 * Our current policy is to postpone volatile expressions till after the sort
 * unconditionally (assuming that that's possible, ie they are in plain tlist
 * columns and not ORDER BY/GROUP BY/DISTINCT columns).  We also prefer to
 * postpone set-returning expressions, because running them beforehand would
 * bloat the sort dataset, and because it might cause unexpected output order
 * if the sort isn't stable.  However there's a constraint on that: all SRFs
 * in the tlist should be evaluated at the same plan step, so that they can
 * run in sync in nodeProjectSet.  So if any SRFs are in sort columns, we
 * mustn't postpone any SRFs.  (Note that in principle that policy should
 * probably get applied to the group/window input targetlists too, but we
 * have not done that historically.)  Lastly, expensive expressions are
 * postponed if there is a LIMIT, or if root->tuple_fraction shows that
 * partial evaluation of the query is possible (if neither is true, we expect
 * to have to evaluate the expressions for every row anyway), or if there are
 * any volatile or set-returning expressions (since once we've put in a
 * projection at all, it won't cost any more to postpone more stuff).
 *
 * Another issue that could potentially be considered here is that
 * evaluating tlist expressions could result in data that's either wider
 * or narrower than the input Vars, thus changing the volume of data that
 * has to go through the Sort.  However, we usually have only a very bad
 * idea of the output width of any expression more complex than a Var,
 * so for now it seems too risky to try to optimize on that basis.
 *
 * Note that if we do produce a modified sort-input target, and then the
 * query ends up not using an explicit Sort, no particular harm is done:
 * we'll initially use the modified target for the preceding path nodes,
 * but then change them to the final target with apply_projection_to_path.
 * Moreover, in such a case the guarantees about evaluation order of
 * volatile functions still hold, since the rows are sorted already.
 *
 * This function has some things in common with make_group_input_target and
 * make_window_input_target, though the detailed rules for what to do are
 * different.  We never flatten/postpone any grouping or ordering columns;
 * those are needed before the sort.  If we do flatten a particular
 * expression, we leave Aggref and WindowFunc nodes alone, since those were
 * computed earlier.
 *
 * 'final_target' is the query's final target list (in PathTarget form)
 * 'have_postponed_srfs' is an output argument, see below
 *
 * The result is the PathTarget to be computed by the plan node immediately
 * below the Sort step (and the Distinct step, if any).  This will be
 * exactly final_target if we decide a projection step wouldn't be helpful.
 *
 * In addition, *have_postponed_srfs is set to true if we choose to postpone
 * any set-returning functions to after the Sort.
 */
static PathTarget *
make_sort_input_target(PlannerInfo *root, PathTarget *final_target, bool *have_postponed_srfs)
{


























































































































































}

/*
 * get_cheapest_fractional_path
 *	  Find the cheapest path for retrieving a specified fraction of all
 *	  the tuples expected to be returned by the given relation.
 *
 * We interpret tuple_fraction the same way as grouping_planner.
 *
 * We assume set_cheapest() has been run on the given rel.
 */
Path *
get_cheapest_fractional_path(RelOptInfo *rel, double tuple_fraction)
{




























}

/*
 * adjust_paths_for_srfs
 *		Fix up the Paths of the given upperrel to handle tSRFs properly.
 *
 * The executor can only handle set-returning functions that appear at the
 * top level of the targetlist of a ProjectSet plan node.  If we have any SRFs
 * that are not at top level, we need to split up the evaluation into multiple
 * plan levels in which each level satisfies this constraint.  This function
 * modifies each Path of an upperrel that (might) compute any SRFs in its
 * output tlist to insert appropriate projection steps.
 *
 * The given targets and targets_contain_srfs lists are from
 * split_pathtarget_at_srfs().  We assume the existing Paths emit the first
 * target in targets.
 */
static void
adjust_paths_for_srfs(PlannerInfo *root, RelOptInfo *rel, List *targets, List *targets_contain_srfs)
{
















































































}

/*
 * expression_planner
 *		Perform planner's transformations on a standalone expression.
 *
 * Various utility commands need to evaluate expressions that are not part
 * of a plannable query.  They can do so using the executor's regular
 * expression-execution machinery, but first the expression has to be fed
 * through here to transform it from parser output to something executable.
 *
 * Currently, we disallow sublinks in standalone expressions, so there's no
 * real "planning" involved here.  (That might not always be true though.)
 * What we must do is run eval_const_expressions to ensure that any function
 * calls are converted to positional notation and function default arguments
 * get inserted.  The fact that constant subexpressions get simplified is a
 * side-effect that is useful when the expression will get evaluated more than
 * once.  Also, we must fix operator function IDs.
 *
 * This does not return any information about dependencies of the expression.
 * Hence callers should use the results only for the duration of the current
 * query.  Callers that would like to cache the results for longer should use
 * expression_planner_with_deps, probably via the plancache.
 *
 * Note: this must not make any damaging changes to the passed-in expression
 * tree.  (It would actually be okay to apply fix_opfuncids to it, but since
 * we first do an expression_tree_mutator-based walk, what is returned will
 * be a new node tree.)  The result is constructed in the current memory
 * context; beware that this can leak a lot of additional stuff there, too.
 */
Expr *
expression_planner(Expr *expr)
{












}

/*
 * expression_planner_with_deps
 *		Perform planner's transformations on a standalone expression,
 *		returning expression dependency information along with the
 *result.
 *
 * This is identical to expression_planner() except that it also returns
 * information about possible dependencies of the expression, ie identities of
 * objects whose definitions affect the result.  As in a PlannedStmt, these
 * are expressed as a list of relation Oids and a list of PlanInvalItems.
 */
Expr *
expression_planner_with_deps(Expr *expr, List **relationOids, List **invalItems)
{


































}

/*
 * plan_cluster_use_sort
 *		Use the planner to decide how CLUSTER should implement sorting
 *
 * tableOid is the OID of a table to be clustered on its index indexOid
 * (which is already known to be a btree index).  Decide whether it's
 * cheaper to do an indexscan or a seqscan-plus-sort to execute the CLUSTER.
 * Return true to use sorting, false to use an indexscan.
 *
 * Note: caller had better already hold some type of lock on the table.
 */
bool
plan_cluster_use_sort(Oid tableOid, Oid indexOid)
{


































































































}

/*
 * plan_create_index_workers
 *		Use the planner to decide how many parallel worker processes
 *		CREATE INDEX should request for use
 *
 * tableOid is the table on which the index is to be built.  indexOid is the
 * OID of an index to be created or reindexed (which must be a btree index).
 *
 * Return value is the number of parallel worker processes to request.  It
 * may be unsafe to proceed if this is 0.  Note that this does not include the
 * leader participating as a worker (value is always a number of parallel
 * worker processes).
 *
 * Note: caller had better already hold some type of lock on the table and
 * index.
 */
int
plan_create_index_workers(Oid tableOid, Oid indexOid)
{
























































































































}

/*
 * add_paths_to_grouping_rel
 *
 * Add non-partial paths to grouping relation.
 */
static void
add_paths_to_grouping_rel(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, double dNumGroups, GroupPathExtraData *extra)
{























































































































































}

/*
 * create_partial_grouping_paths
 *
 * Create a new upper relation representing the result of partial aggregation
 * and populate it with appropriate paths.  Note that we don't finalize the
 * lists of paths here, so the caller can add additional partial or non-partial
 * paths and must afterward call gather_grouping_paths and set_cheapest on
 * the returned upper relation.
 *
 * All paths for this new upper relation -- both partial and non-partial --
 * have been partially aggregated but require a subsequent FinalizeAggregate
 * step.
 *
 * NB: This function is allowed to return NULL if it determines that there is
 * no real need to create a new RelOptInfo.
 */
static RelOptInfo *
create_partial_grouping_paths(PlannerInfo *root, RelOptInfo *grouped_rel, RelOptInfo *input_rel, grouping_sets_data *gd, GroupPathExtraData *extra, bool force_rel_creation)
{














































































































































































































}

/*
 * Generate Gather and Gather Merge paths for a grouping relation or partial
 * grouping relation.
 *
 * generate_gather_paths does most of the work, but we also consider a special
 * case: we could try sorting the data by the group_pathkeys and then applying
 * Gather Merge.
 *
 * NB: This function shouldn't be used for anything other than a grouped or
 * partially grouped relation not only because of the fact that it explicitly
 * references group_pathkeys but we pass "true" as the third argument to
 * generate_gather_paths().
 */
static void
gather_grouping_paths(PlannerInfo *root, RelOptInfo *rel)
{


















}

/*
 * can_partial_agg
 *
 * Determines whether or not partial grouping and/or aggregation is possible.
 * Returns true when possible, false otherwise.
 */
static bool
can_partial_agg(PlannerInfo *root, const AggClauseCosts *agg_costs)
{























}

/*
 * apply_scanjoin_target_to_paths
 *
 * Adjust the final scan/join relation, and recursively all of its children,
 * to generate the final scan/join target.  It would be more correct to model
 * this as a separate planning step with a new RelOptInfo at the toplevel and
 * for each child relation, but doing it this way is noticeably cheaper.
 * Maybe that problem can be solved at some point, but for now we do this.
 *
 * If tlist_same_exprs is true, then the scan/join target to be applied has
 * the same expressions as the existing reltarget, so we need only insert the
 * appropriate sortgroupref information.  By avoiding the creation of
 * projection paths we save effort both immediately and at plan creation time.
 */
static void
apply_scanjoin_target_to_paths(PlannerInfo *root, RelOptInfo *rel, List *scanjoin_targets, List *scanjoin_targets_contain_srfs, bool scanjoin_target_parallel_safe, bool tlist_same_exprs)
{














































































































































































































}

/*
 * create_partitionwise_grouping_paths
 *
 * If the partition keys of input relation are part of the GROUP BY clause, all
 * the rows belonging to a given group come from a single partition.  This
 * allows aggregation/grouping over a partitioned relation to be broken down
 * into aggregation/grouping on each partition.  This should be no worse, and
 * often better, than the normal approach.
 *
 * However, if the GROUP BY clause does not contain all the partition keys,
 * rows from a given group may be spread across multiple partitions. In that
 * case, we perform partial aggregation for each group, append the results,
 * and then finalize aggregation.  This is less certain to win than the
 * previous case.  It may win if the PartialAggregate stage greatly reduces
 * the number of groups, because fewer rows will pass through the Append node.
 * It may lose if we have lots of small groups.
 */
static void
create_partitionwise_grouping_paths(PlannerInfo *root, RelOptInfo *input_rel, RelOptInfo *grouped_rel, RelOptInfo *partially_grouped_rel, const AggClauseCosts *agg_costs, grouping_sets_data *gd, PartitionwiseAggregateType patype, GroupPathExtraData *extra)
{











































































































}

/*
 * group_by_has_partkey
 *
 * Returns true, if all the partition keys of the given relation are part of
 * the GROUP BY clauses, false otherwise.
 */
static bool
group_by_has_partkey(RelOptInfo *input_rel, List *targetList, List *groupClause)
{











































}