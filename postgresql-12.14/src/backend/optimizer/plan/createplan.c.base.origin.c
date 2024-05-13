/*-------------------------------------------------------------------------
 *
 * createplan.c
 *	  Routines to create the desired plan for processing a query.
 *	  Planning is complete, we just need to convert the selected
 *	  Path into a Plan.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/createplan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>
#include <math.h>

#include "access/sysattr.h"
#include "catalog/pg_class.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/extensible.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/paramassign.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "partitioning/partprune.h"
#include "utils/lsyscache.h"

/*
 * Flag bits that can appear in the flags argument of create_plan_recurse().
 * These can be OR-ed together.
 *
 * CP_EXACT_TLIST specifies that the generated plan node must return exactly
 * the tlist specified by the path's pathtarget (this overrides both
 * CP_SMALL_TLIST and CP_LABEL_TLIST, if those are set).  Otherwise, the
 * plan node is allowed to return just the Vars and PlaceHolderVars needed
 * to evaluate the pathtarget.
 *
 * CP_SMALL_TLIST specifies that a narrower tlist is preferred.  This is
 * passed down by parent nodes such as Sort and Hash, which will have to
 * store the returned tuples.
 *
 * CP_LABEL_TLIST specifies that the plan node must return columns matching
 * any sortgrouprefs specified in its pathtarget, with appropriate
 * ressortgroupref labels.  This is passed down by parent nodes such as Sort
 * and Group, which need these values to be available in their inputs.
 *
 * CP_IGNORE_TLIST specifies that the caller plans to replace the targetlist,
 * and therefore it doesn't matter a bit what target list gets generated.
 */
#define CP_EXACT_TLIST 0x0001  /* Plan must return specified tlist */
#define CP_SMALL_TLIST 0x0002  /* Prefer narrower tlists */
#define CP_LABEL_TLIST 0x0004  /* tlist must contain sortgrouprefs */
#define CP_IGNORE_TLIST 0x0008 /* caller will replace tlist */

static Plan *
create_plan_recurse(PlannerInfo *root, Path *best_path, int flags);
static Plan *
create_scan_plan(PlannerInfo *root, Path *best_path, int flags);
static List *
build_path_tlist(PlannerInfo *root, Path *path);
static bool
use_physical_tlist(PlannerInfo *root, Path *path, int flags);
static List *
get_gating_quals(PlannerInfo *root, List *quals);
static Plan *
create_gating_plan(PlannerInfo *root, Path *path, Plan *plan, List *gating_quals);
static Plan *
create_join_plan(PlannerInfo *root, JoinPath *best_path);
static Plan *
create_append_plan(PlannerInfo *root, AppendPath *best_path, int flags);
static Plan *
create_merge_append_plan(PlannerInfo *root, MergeAppendPath *best_path, int flags);
static Result *
create_group_result_plan(PlannerInfo *root, GroupResultPath *best_path);
static ProjectSet *
create_project_set_plan(PlannerInfo *root, ProjectSetPath *best_path);
static Material *
create_material_plan(PlannerInfo *root, MaterialPath *best_path, int flags);
static Plan *
create_unique_plan(PlannerInfo *root, UniquePath *best_path, int flags);
static Gather *
create_gather_plan(PlannerInfo *root, GatherPath *best_path);
static Plan *
create_projection_plan(PlannerInfo *root, ProjectionPath *best_path, int flags);
static Plan *
inject_projection_plan(Plan *subplan, List *tlist, bool parallel_safe);
static Sort *
create_sort_plan(PlannerInfo *root, SortPath *best_path, int flags);
static Group *
create_group_plan(PlannerInfo *root, GroupPath *best_path);
static Unique *
create_upper_unique_plan(PlannerInfo *root, UpperUniquePath *best_path, int flags);
static Agg *
create_agg_plan(PlannerInfo *root, AggPath *best_path);
static Plan *
create_groupingsets_plan(PlannerInfo *root, GroupingSetsPath *best_path);
static Result *
create_minmaxagg_plan(PlannerInfo *root, MinMaxAggPath *best_path);
static WindowAgg *
create_windowagg_plan(PlannerInfo *root, WindowAggPath *best_path);
static SetOp *
create_setop_plan(PlannerInfo *root, SetOpPath *best_path, int flags);
static RecursiveUnion *
create_recursiveunion_plan(PlannerInfo *root, RecursiveUnionPath *best_path);
static LockRows *
create_lockrows_plan(PlannerInfo *root, LockRowsPath *best_path, int flags);
static ModifyTable *
create_modifytable_plan(PlannerInfo *root, ModifyTablePath *best_path);
static Limit *
create_limit_plan(PlannerInfo *root, LimitPath *best_path, int flags);
static SeqScan *
create_seqscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static SampleScan *
create_samplescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static Scan *
create_indexscan_plan(PlannerInfo *root, IndexPath *best_path, List *tlist, List *scan_clauses, bool indexonly);
static BitmapHeapScan *
create_bitmap_scan_plan(PlannerInfo *root, BitmapHeapPath *best_path, List *tlist, List *scan_clauses);
static Plan *
create_bitmap_subplan(PlannerInfo *root, Path *bitmapqual, List **qual, List **indexqual, List **indexECs);
static void
bitmap_subplan_mark_shared(Plan *plan);
static TidScan *
create_tidscan_plan(PlannerInfo *root, TidPath *best_path, List *tlist, List *scan_clauses);
static SubqueryScan *
create_subqueryscan_plan(PlannerInfo *root, SubqueryScanPath *best_path, List *tlist, List *scan_clauses);
static FunctionScan *
create_functionscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static ValuesScan *
create_valuesscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static TableFuncScan *
create_tablefuncscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static CteScan *
create_ctescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static NamedTuplestoreScan *
create_namedtuplestorescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static Result *
create_resultscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static WorkTableScan *
create_worktablescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses);
static ForeignScan *
create_foreignscan_plan(PlannerInfo *root, ForeignPath *best_path, List *tlist, List *scan_clauses);
static CustomScan *
create_customscan_plan(PlannerInfo *root, CustomPath *best_path, List *tlist, List *scan_clauses);
static NestLoop *
create_nestloop_plan(PlannerInfo *root, NestPath *best_path);
static MergeJoin *
create_mergejoin_plan(PlannerInfo *root, MergePath *best_path);
static HashJoin *
create_hashjoin_plan(PlannerInfo *root, HashPath *best_path);
static Node *
replace_nestloop_params(PlannerInfo *root, Node *expr);
static Node *
replace_nestloop_params_mutator(Node *node, PlannerInfo *root);
static void
fix_indexqual_references(PlannerInfo *root, IndexPath *index_path, List **stripped_indexquals_p, List **fixed_indexquals_p);
static List *
fix_indexorderby_references(PlannerInfo *root, IndexPath *index_path);
static Node *
fix_indexqual_clause(PlannerInfo *root, IndexOptInfo *index, int indexcol, Node *clause, List *indexcolnos);
static Node *
fix_indexqual_operand(Node *node, IndexOptInfo *index, int indexcol);
static List *
get_switched_clauses(List *clauses, Relids outerrelids);
static List *
order_qual_clauses(PlannerInfo *root, List *clauses);
static void
copy_generic_path_info(Plan *dest, Path *src);
static void
copy_plan_costsize(Plan *dest, Plan *src);
static void
label_sort_with_costsize(PlannerInfo *root, Sort *plan, double limit_tuples);
static SeqScan *
make_seqscan(List *qptlist, List *qpqual, Index scanrelid);
static SampleScan *
make_samplescan(List *qptlist, List *qpqual, Index scanrelid, TableSampleClause *tsc);
static IndexScan *
make_indexscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig, List *indexorderby, List *indexorderbyorig, List *indexorderbyops, ScanDirection indexscandir);
static IndexOnlyScan *
make_indexonlyscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *recheckqual, List *indexorderby, List *indextlist, ScanDirection indexscandir);
static BitmapIndexScan *
make_bitmap_indexscan(Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig);
static BitmapHeapScan *
make_bitmap_heapscan(List *qptlist, List *qpqual, Plan *lefttree, List *bitmapqualorig, Index scanrelid);
static TidScan *
make_tidscan(List *qptlist, List *qpqual, Index scanrelid, List *tidquals);
static SubqueryScan *
make_subqueryscan(List *qptlist, List *qpqual, Index scanrelid, Plan *subplan);
static FunctionScan *
make_functionscan(List *qptlist, List *qpqual, Index scanrelid, List *functions, bool funcordinality);
static ValuesScan *
make_valuesscan(List *qptlist, List *qpqual, Index scanrelid, List *values_lists);
static TableFuncScan *
make_tablefuncscan(List *qptlist, List *qpqual, Index scanrelid, TableFunc *tablefunc);
static CteScan *
make_ctescan(List *qptlist, List *qpqual, Index scanrelid, int ctePlanId, int cteParam);
static NamedTuplestoreScan *
make_namedtuplestorescan(List *qptlist, List *qpqual, Index scanrelid, char *enrname);
static WorkTableScan *
make_worktablescan(List *qptlist, List *qpqual, Index scanrelid, int wtParam);
static RecursiveUnion *
make_recursive_union(List *tlist, Plan *lefttree, Plan *righttree, int wtParam, List *distinctList, long numGroups);
static BitmapAnd *
make_bitmap_and(List *bitmapplans);
static BitmapOr *
make_bitmap_or(List *bitmapplans);
static NestLoop *
make_nestloop(List *tlist, List *joinclauses, List *otherclauses, List *nestParams, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique);
static HashJoin *
make_hashjoin(List *tlist, List *joinclauses, List *otherclauses, List *hashclauses, List *hashoperators, List *hashcollations, List *hashkeys, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique);
static Hash *
make_hash(Plan *lefttree, List *hashkeys, Oid skewTable, AttrNumber skewColumn, bool skewInherit);
static MergeJoin *
make_mergejoin(List *tlist, List *joinclauses, List *otherclauses, List *mergeclauses, Oid *mergefamilies, Oid *mergecollations, int *mergestrategies, bool *mergenullsfirst, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique, bool skip_mark_restore);
static Sort *
make_sort(Plan *lefttree, int numCols, AttrNumber *sortColIdx, Oid *sortOperators, Oid *collations, bool *nullsFirst);
static Plan *
prepare_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids, const AttrNumber *reqColIdx, bool adjust_tlist_in_place, int *p_numsortkeys, AttrNumber **p_sortColIdx, Oid **p_sortOperators, Oid **p_collations, bool **p_nullsFirst);
static EquivalenceMember *
find_ec_member_for_tle(EquivalenceClass *ec, TargetEntry *tle, Relids relids);
static Sort *
make_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids);
static Sort *
make_sort_from_groupcols(List *groupcls, AttrNumber *grpColIdx, Plan *lefttree);
static Material *
make_material(Plan *lefttree);
static WindowAgg *
make_windowagg(List *tlist, Index winref, int partNumCols, AttrNumber *partColIdx, Oid *partOperators, Oid *partCollations, int ordNumCols, AttrNumber *ordColIdx, Oid *ordOperators, Oid *ordCollations, int frameOptions, Node *startOffset, Node *endOffset, Oid startInRangeFunc, Oid endInRangeFunc, Oid inRangeColl, bool inRangeAsc, bool inRangeNullsFirst, Plan *lefttree);
static Group *
make_group(List *tlist, List *qual, int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators, Oid *grpCollations, Plan *lefttree);
static Unique *
make_unique_from_sortclauses(Plan *lefttree, List *distinctList);
static Unique *
make_unique_from_pathkeys(Plan *lefttree, List *pathkeys, int numCols);
static Gather *
make_gather(List *qptlist, List *qpqual, int nworkers, int rescan_param, bool single_copy, Plan *subplan);
static SetOp *
make_setop(SetOpCmd cmd, SetOpStrategy strategy, Plan *lefttree, List *distinctList, AttrNumber flagColIdx, int firstFlag, long numGroups);
static LockRows *
make_lockrows(Plan *lefttree, List *rowMarks, int epqParam);
static Result *
make_result(List *tlist, Node *resconstantqual, Plan *subplan);
static ProjectSet *
make_project_set(List *tlist, Plan *subplan);
static ModifyTable *
make_modifytable(PlannerInfo *root, CmdType operation, bool canSetTag, Index nominalRelation, Index rootRelation, bool partColsUpdated, List *resultRelations, List *subplans, List *subroots, List *withCheckOptionLists, List *returningLists, List *rowMarks, OnConflictExpr *onconflict, int epqParam);
static GatherMerge *
create_gather_merge_plan(PlannerInfo *root, GatherMergePath *best_path);

/*
 * create_plan
 *	  Creates the access plan for a query by recursively processing the
 *	  desired tree of pathnodes, starting at the node 'best_path'.  For
 *	  every pathnode found, we create a corresponding plan node containing
 *	  appropriate id, target list, and qualification information.
 *
 *	  The tlists and quals in the plan tree are still in planner format,
 *	  ie, Vars still correspond to the parser's numbering.  This will be
 *	  fixed later by setrefs.c.
 *
 *	  best_path is the best access path
 *
 *	  Returns a Plan tree.
 */
Plan *
create_plan(PlannerInfo *root, Path *best_path)
{














































}

/*
 * create_plan_recurse
 *	  Recursive guts of create_plan().
 */
static Plan *
create_plan_recurse(PlannerInfo *root, Path *best_path, int flags)
{
























































































































}

/*
 * create_scan_plan
 *	 Create a scan plan for the parent relation of 'best_path'.
 */
static Plan *
create_scan_plan(PlannerInfo *root, Path *best_path, int flags)
{



























































































































































































}

/*
 * Build a target list (ie, a list of TargetEntry) for the Path's output.
 *
 * This is almost just make_tlist_from_pathtarget(), but we also have to
 * deal with replacing nestloop params.
 */
static List *
build_path_tlist(PlannerInfo *root, Path *path)
{































}

/*
 * use_physical_tlist
 *		Decide whether to use a tlist matching relation structure,
 *		rather than only those Vars actually referenced.
 */
static bool
use_physical_tlist(PlannerInfo *root, Path *path, int flags)
{










































































































































}

/*
 * get_gating_quals
 *	  See if there are pseudoconstant quals in a node's quals list
 *
 * If the node's quals list includes any pseudoconstant quals,
 * return just those quals.
 */
static List *
get_gating_quals(PlannerInfo *root, List *quals)
{











}

/*
 * create_gating_plan
 *	  Deal with pseudoconstant qual clauses
 *
 * Add a gating Result node atop the already-built plan.
 */
static Plan *
create_gating_plan(PlannerInfo *root, Path *path, Plan *plan, List *gating_quals)
{















































}

/*
 * create_join_plan
 *	  Create a join plan for 'best_path' and (recursively) plans for its
 *	  inner and outer paths.
 */
static Plan *
create_join_plan(PlannerInfo *root, JoinPath *best_path)
{













































}

/*
 * create_append_plan
 *	  Create an Append plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Plan *
create_append_plan(PlannerInfo *root, AppendPath *best_path, int flags)
{









































































































































































}

/*
 * create_merge_append_plan
 *	  Create a MergeAppend plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Plan *
create_merge_append_plan(PlannerInfo *root, MergeAppendPath *best_path, int flags)
{





























































































































}

/*
 * create_group_result_plan
 *	  Create a Result plan for 'best_path'.
 *	  This is only used for degenerate grouping cases.
 *
 *	  Returns a Plan node.
 */
static Result *
create_group_result_plan(PlannerInfo *root, GroupResultPath *best_path)
{














}

/*
 * create_project_set_plan
 *	  Create a ProjectSet plan for 'best_path'.
 *
 *	  Returns a Plan node.
 */
static ProjectSet *
create_project_set_plan(PlannerInfo *root, ProjectSetPath *best_path)
{














}

/*
 * create_material_plan
 *	  Create a Material plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Material *
create_material_plan(PlannerInfo *root, MaterialPath *best_path, int flags)
{















}

/*
 * create_unique_plan
 *	  Create a Unique plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  Returns a Plan node.
 */
static Plan *
create_unique_plan(PlannerInfo *root, UniquePath *best_path, int flags)
{



















































































































































































}

/*
 * create_gather_plan
 *
 *	  Create a Gather plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static Gather *
create_gather_plan(PlannerInfo *root, GatherPath *best_path)
{




















}

/*
 * create_gather_merge_plan
 *
 *	  Create a Gather Merge plan for 'best_path' and (recursively)
 *	  plans for its subpaths.
 */
static GatherMerge *
create_gather_merge_plan(PlannerInfo *root, GatherMergePath *best_path)
{




































}

/*
 * create_projection_plan
 *
 *	  Create a plan tree to do a projection step and (recursively) plans
 *	  for its subpaths.  We may need a Result node for the projection,
 *	  but sometimes we can just let the subplan do the work.
 */
static Plan *
create_projection_plan(PlannerInfo *root, ProjectionPath *best_path, int flags)
{






















































































}

/*
 * inject_projection_plan
 *	  Insert a Result node to do a projection step.
 *
 * This is used in a few places where we decide on-the-fly that we need a
 * projection step as part of the tree generated for some Path node.
 * We should try to get rid of this in favor of doing it more honestly.
 *
 * One reason it's ugly is we have to be told the right parallel_safe marking
 * to apply (since the tlist might be unsafe even if the child plan is safe).
 */
static Plan *
inject_projection_plan(Plan *subplan, List *tlist, bool parallel_safe)
{















}

/*
 * change_plan_targetlist
 *	  Externally available wrapper for inject_projection_plan.
 *
 * This is meant for use by FDW plan-generation functions, which might
 * want to adjust the tlist computed by some subplan tree.  In general,
 * a Result node is needed to compute the new tlist, but we can optimize
 * some cases.
 *
 * In most cases, tlist_parallel_safe can just be passed as the parallel_safe
 * flag of the FDW's own Path node.
 */
Plan *
change_plan_targetlist(Plan *subplan, List *tlist, bool tlist_parallel_safe)
{
















}

/*
 * create_sort_plan
 *
 *	  Create a Sort plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static Sort *
create_sort_plan(PlannerInfo *root, SortPath *best_path, int flags)
{





















}

/*
 * create_group_plan
 *
 *	  Create a Group plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static Group *
create_group_plan(PlannerInfo *root, GroupPath *best_path)
{




















}

/*
 * create_upper_unique_plan
 *
 *	  Create a Unique plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static Unique *
create_upper_unique_plan(PlannerInfo *root, UpperUniquePath *best_path, int flags)
{














}

/*
 * create_agg_plan
 *
 *	  Create an Agg plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static Agg *
create_agg_plan(PlannerInfo *root, AggPath *best_path)
{




















}

/*
 * Given a groupclause for a collection of grouping sets, produce the
 * corresponding groupColIdx.
 *
 * root->grouping_map maps the tleSortGroupRef to the actual column position in
 * the input tuple. So we get the ref from the entries in the groupclause and
 * look them up there.
 */
static AttrNumber *
remap_groupColIdx(PlannerInfo *root, List *groupClause)
{


















}

/*
 * create_groupingsets_plan
 *	  Create a plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 *
 *	  What we emit is an Agg plan with some vestigial Agg and Sort nodes
 *	  hanging off the side.  The top Agg implements the last grouping set
 *	  specified in the GroupingSetsPath, and any additional grouping sets
 *	  each give rise to a subsidiary Agg and Sort node in the top Agg's
 *	  "chain" list.  These nodes don't participate in the plan directly,
 *	  but they are a convenient way to represent the required data for
 *	  the extra steps.
 *
 *	  Returns a Plan node.
 */
static Plan *
create_groupingsets_plan(PlannerInfo *root, GroupingSetsPath *best_path)
{








































































































































}

/*
 * create_minmaxagg_plan
 *
 *	  Create a Result plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static Result *
create_minmaxagg_plan(PlannerInfo *root, MinMaxAggPath *best_path)
{
























































}

/*
 * create_windowagg_plan
 *
 *	  Create a WindowAgg plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static WindowAgg *
create_windowagg_plan(PlannerInfo *root, WindowAggPath *best_path)
{











































































}

/*
 * create_setop_plan
 *
 *	  Create a SetOp plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static SetOp *
create_setop_plan(PlannerInfo *root, SetOpPath *best_path, int flags)
{


















}

/*
 * create_recursiveunion_plan
 *
 *	  Create a RecursiveUnion plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static RecursiveUnion *
create_recursiveunion_plan(PlannerInfo *root, RecursiveUnionPath *best_path)
{




















}

/*
 * create_lockrows_plan
 *
 *	  Create a LockRows plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static LockRows *
create_lockrows_plan(PlannerInfo *root, LockRowsPath *best_path, int flags)
{











}

/*
 * create_modifytable_plan
 *	  Create a ModifyTable plan for 'best_path'.
 *
 *	  Returns a Plan node.
 */
static ModifyTable *
create_modifytable_plan(PlannerInfo *root, ModifyTablePath *best_path)
{



































}

/*
 * create_limit_plan
 *
 *	  Create a Limit plan for 'best_path' and (recursively) plans
 *	  for its subpaths.
 */
static Limit *
create_limit_plan(PlannerInfo *root, LimitPath *best_path, int flags)
{











}

/*****************************************************************************
 *
 *	BASE-RELATION SCAN METHODS
 *
 *****************************************************************************/

/*
 * create_seqscan_plan
 *	 Returns a seqscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static SeqScan *
create_seqscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{
























}

/*
 * create_samplescan_plan
 *	 Returns a samplescan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static SampleScan *
create_samplescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{






























}

/*
 * create_indexscan_plan
 *	  Returns an indexscan plan for the base relation scanned by 'best_path'
 *	  with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 *
 * We use this for both plain IndexScans and IndexOnlyScans, because the
 * qual preprocessing work is the same for both.  Note that the caller tells
 * us which to build --- we don't look at best_path->path.pathtype, because
 * create_bitmap_subplan needs to be able to override the prior decision.
 */
static Scan *
create_indexscan_plan(PlannerInfo *root, IndexPath *best_path, List *tlist, List *scan_clauses, bool indexonly)
{


































































































































































}

/*
 * create_bitmap_scan_plan
 *	  Returns a bitmap scan plan for the base relation scanned by
 *'best_path' with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static BitmapHeapScan *
create_bitmap_scan_plan(PlannerInfo *root, BitmapHeapPath *best_path, List *tlist, List *scan_clauses)
{







































































































}

/*
 * Given a bitmapqual tree, generate the Plan tree that implements it
 *
 * As byproducts, we also return in *qual and *indexqual the qual lists
 * (in implicit-AND form, without RestrictInfos) describing the original index
 * conditions and the generated indexqual conditions.  (These are the same in
 * simple cases, but when special index operators are involved, the former
 * list includes the special conditions while the latter includes the actual
 * indexable conditions derived from them.)  Both lists include partial-index
 * predicates, because we have to recheck predicates as well as index
 * conditions if the bitmap scan becomes lossy.
 *
 * In addition, we return a list of EquivalenceClass pointers for all the
 * top-level indexquals that were possibly-redundantly derived from ECs.
 * This allows removal of scan_clauses that are redundant with such quals.
 * (We do not attempt to detect such redundancies for quals that are within
 * OR subtrees.  This could be done in a less hacky way if we returned the
 * indexquals in RestrictInfo form, but that would be slower and still pretty
 * messy, since we'd have to build new RestrictInfos in many cases.)
 */
static Plan *
create_bitmap_subplan(PlannerInfo *root, Path *bitmapqual, List **qual, List **indexqual, List **indexECs)
{












































































































































































































}

/*
 * create_tidscan_plan
 *	 Returns a tidscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static TidScan *
create_tidscan_plan(PlannerInfo *root, TidPath *best_path, List *tlist, List *scan_clauses)
{
























































































}

/*
 * create_subqueryscan_plan
 *	 Returns a subqueryscan plan for the base relation scanned by
 *'best_path' with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static SubqueryScan *
create_subqueryscan_plan(PlannerInfo *root, SubqueryScanPath *best_path, List *tlist, List *scan_clauses)
{


































}

/*
 * create_functionscan_plan
 *	 Returns a functionscan plan for the base relation scanned by
 *'best_path' with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static FunctionScan *
create_functionscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{






























}

/*
 * create_tablefuncscan_plan
 *	 Returns a tablefuncscan plan for the base relation scanned by
 *'best_path' with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static TableFuncScan *
create_tablefuncscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{






























}

/*
 * create_valuesscan_plan
 *	 Returns a valuesscan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static ValuesScan *
create_valuesscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{






























}

/*
 * create_ctescan_plan
 *	 Returns a ctescan plan for the base relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static CteScan *
create_ctescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{































































































}

/*
 * create_namedtuplestorescan_plan
 *	 Returns a tuplestorescan plan for the base relation scanned by
 *	'best_path' with restriction clauses 'scan_clauses' and targetlist
 *	'tlist'.
 */
static NamedTuplestoreScan *
create_namedtuplestorescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{

























}

/*
 * create_resultscan_plan
 *	 Returns a Result plan for the RTE_RESULT base relation scanned by
 *	'best_path' with restriction clauses 'scan_clauses' and targetlist
 *	'tlist'.
 */
static Result *
create_resultscan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{

























}

/*
 * create_worktablescan_plan
 *	 Returns a worktablescan plan for the base relation scanned by
 *'best_path' with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static WorkTableScan *
create_worktablescan_plan(PlannerInfo *root, Path *best_path, List *tlist, List *scan_clauses)
{





















































}

/*
 * create_foreignscan_plan
 *	 Returns a foreignscan plan for the relation scanned by 'best_path'
 *	 with restriction clauses 'scan_clauses' and targetlist 'tlist'.
 */
static ForeignScan *
create_foreignscan_plan(PlannerInfo *root, ForeignPath *best_path, List *tlist, List *scan_clauses)
{





































































































































}

/*
 * create_customscan_plan
 *
 * Transform a CustomPath into a Plan.
 */
static CustomScan *
create_customscan_plan(PlannerInfo *root, CustomPath *best_path, List *tlist, List *scan_clauses)
{

















































}

/*****************************************************************************
 *
 *	JOIN METHODS
 *
 *****************************************************************************/

static NestLoop *
create_nestloop_plan(PlannerInfo *root, NestPath *best_path)
{


























































}

static MergeJoin *
create_mergejoin_plan(PlannerInfo *root, MergePath *best_path)
{





































































































































































































































































































}

static HashJoin *
create_hashjoin_plan(PlannerInfo *root, HashPath *best_path)
{




















































































































































}

/*****************************************************************************
 *
 *	SUPPORTING ROUTINES
 *
 *****************************************************************************/

/*
 * replace_nestloop_params
 *	  Replace outer-relation Vars and PlaceHolderVars in the given
 *expression with nestloop Params
 *
 * All Vars and PlaceHolderVars belonging to the relation(s) identified by
 * root->curOuterRels are replaced by Params, and entries are added to
 * root->curOuterParams if not already present.
 */
static Node *
replace_nestloop_params(PlannerInfo *root, Node *expr)
{


}

static Node *
replace_nestloop_params_mutator(Node *node, PlannerInfo *root)
{

























































}

/*
 * fix_indexqual_references
 *	  Adjust indexqual clauses to the form the executor's indexqual
 *	  machinery needs.
 *
 * We have three tasks here:
 *	* Select the actual qual clauses out of the input IndexClause list,
 *	  and remove RestrictInfo nodes from the qual clauses.
 *	* Replace any outer-relation Var or PHV nodes with nestloop Params.
 *	  (XXX eventually, that responsibility should go elsewhere?)
 *	* Index keys must be represented by Var nodes with varattno set to the
 *	  index's attribute number, not the attribute number in the original
 *rel.
 *
 * *stripped_indexquals_p receives a list of the actual qual clauses.
 *
 * *fixed_indexquals_p receives a list of the adjusted quals.  This is a copy
 * that shares no substructure with the original; this is needed in case there
 * are subplans in it (we need two separate copies of the subplan tree, or
 * things will go awry).
 */
static void
fix_indexqual_references(PlannerInfo *root, IndexPath *index_path, List **stripped_indexquals_p, List **fixed_indexquals_p)
{


























}

/*
 * fix_indexorderby_references
 *	  Adjust indexorderby clauses to the form the executor's index
 *	  machinery needs.
 *
 * This is a simplified version of fix_indexqual_references.  The input is
 * bare clauses and a separate indexcol list, instead of IndexClauses.
 */
static List *
fix_indexorderby_references(PlannerInfo *root, IndexPath *index_path)
{
















}

/*
 * fix_indexqual_clause
 *	  Convert a single indexqual clause to the form needed by the executor.
 *
 * We replace nestloop params here, and replace the index key variables
 * or expressions by index Var nodes.
 */
static Node *
fix_indexqual_clause(PlannerInfo *root, IndexOptInfo *index, int indexcol, Node *clause, List *indexcolnos)
{












































}

/*
 * fix_indexqual_operand
 *	  Convert an indexqual expression to a Var referencing the index column.
 *
 * We represent index keys by Var nodes having varno == INDEX_VAR and varattno
 * equal to the index's attribute number (index column position).
 *
 * Most of the code here is just for sanity cross-checking that the given
 * expression actually matches the index column it's claimed to.
 */
static Node *
fix_indexqual_operand(Node *node, IndexOptInfo *index, int indexcol)
{


































































}

/*
 * get_switched_clauses
 *	  Given a list of merge or hash joinclauses (as RestrictInfo nodes),
 *	  extract the bare clauses, and rearrange the elements within the
 *	  clauses, if needed, so the outer join variable is on the left and
 *	  the inner is on the right.  The original clause data structure is not
 *	  touched; a modified list is returned.  We do, however, set the
 *transient outer_is_left field in each RestrictInfo to show which side was
 *which.
 */
static List *
get_switched_clauses(List *clauses, Relids outerrelids)
{







































}

/*
 * order_qual_clauses
 *		Given a list of qual clauses that will all be evaluated at the
 *same plan node, sort the list into the order we want to check the quals in at
 *runtime.
 *
 * When security barrier quals are used in the query, we may have quals with
 * different security levels in the list.  Quals of lower security_level
 * must go before quals of higher security_level, except that we can grant
 * exceptions to move up quals that are leakproof.  When security level
 * doesn't force the decision, we prefer to order clauses by estimated
 * execution cost, cheapest first.
 *
 * Ideally the order should be driven by a combination of execution cost and
 * selectivity, but it's not immediately clear how to account for both,
 * and given the uncertainty of the estimates the reliability of the decisions
 * would be doubtful anyway.  So we just order by security level then
 * estimated per-tuple cost, being careful not to change the order when
 * (as is often the case) the estimates are identical.
 *
 * Although this will work on either bare clauses or RestrictInfos, it's
 * much faster to apply it to RestrictInfos, since it can re-use cost
 * information that is cached in RestrictInfos.  XXX in the bare-clause
 * case, we are also not able to apply security considerations.  That is
 * all right for the moment, because the bare-clause case doesn't occur
 * anywhere that barrier quals could be present, but it would be better to
 * get rid of it.
 *
 * Note: some callers pass lists that contain entries that will later be
 * removed; this is the easiest way to let this routine see RestrictInfos
 * instead of bare clauses.  This is another reason why trying to consider
 * selectivity in the ordering would likely do the wrong thing.
 */
static List *
order_qual_clauses(PlannerInfo *root, List *clauses)
{






























































































}

/*
 * Copy cost and size info from a Path node to the Plan node created from it.
 * The executor usually won't use this info, but it's needed by EXPLAIN.
 * Also copy the parallel-related flags, which the executor *will* use.
 */
static void
copy_generic_path_info(Plan *dest, Path *src)
{






}

/*
 * Copy cost and size info from a lower plan node to an inserted node.
 * (Most callers alter the info after copying it.)
 */
static void
copy_plan_costsize(Plan *dest, Plan *src)
{








}

/*
 * Some places in this file build Sort nodes that don't have a directly
 * corresponding Path node.  The cost of the sort is, or should have been,
 * included in the cost of the Path node we're working from, but since it's
 * not split out, we have to re-figure it using cost_sort().  This is just
 * to label the Sort node nicely for EXPLAIN.
 *
 * limit_tuples is as for cost_sort (in particular, pass -1 if no limit)
 */
static void
label_sort_with_costsize(PlannerInfo *root, Sort *plan, double limit_tuples)
{










}

/*
 * bitmap_subplan_mark_shared
 *	 Set isshared flag in bitmap subplan so that it will be created in
 *	 shared memory.
 */
static void
bitmap_subplan_mark_shared(Plan *plan)
{

















}

/*****************************************************************************
 *
 *	PLAN NODE BUILDING ROUTINES
 *
 * In general, these functions are not passed the original Path and therefore
 * leave it to the caller to fill in the cost/width fields from the Path,
 * typically by calling copy_generic_path_info().  This convention is
 * somewhat historical, but it does support a few places above where we build
 * a plan node without having an exactly corresponding Path node.  Under no
 * circumstances should one of these functions do its own cost calculations,
 * as that would be redundant with calculations done while building Paths.
 *
 *****************************************************************************/

static SeqScan *
make_seqscan(List *qptlist, List *qpqual, Index scanrelid)
{










}

static SampleScan *
make_samplescan(List *qptlist, List *qpqual, Index scanrelid, TableSampleClause *tsc)
{











}

static IndexScan *
make_indexscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig, List *indexorderby, List *indexorderbyorig, List *indexorderbyops, ScanDirection indexscandir)
{

















}

static IndexOnlyScan *
make_indexonlyscan(List *qptlist, List *qpqual, Index scanrelid, Oid indexid, List *indexqual, List *recheckqual, List *indexorderby, List *indextlist, ScanDirection indexscandir)
{
















}

static BitmapIndexScan *
make_bitmap_indexscan(Index scanrelid, Oid indexid, List *indexqual, List *indexqualorig)
{













}

static BitmapHeapScan *
make_bitmap_heapscan(List *qptlist, List *qpqual, Plan *lefttree, List *bitmapqualorig, Index scanrelid)
{











}

static TidScan *
make_tidscan(List *qptlist, List *qpqual, Index scanrelid, List *tidquals)
{











}

static SubqueryScan *
make_subqueryscan(List *qptlist, List *qpqual, Index scanrelid, Plan *subplan)
{











}

static FunctionScan *
make_functionscan(List *qptlist, List *qpqual, Index scanrelid, List *functions, bool funcordinality)
{












}

static TableFuncScan *
make_tablefuncscan(List *qptlist, List *qpqual, Index scanrelid, TableFunc *tablefunc)
{











}

static ValuesScan *
make_valuesscan(List *qptlist, List *qpqual, Index scanrelid, List *values_lists)
{











}

static CteScan *
make_ctescan(List *qptlist, List *qpqual, Index scanrelid, int ctePlanId, int cteParam)
{












}

static NamedTuplestoreScan *
make_namedtuplestorescan(List *qptlist, List *qpqual, Index scanrelid, char *enrname)
{












}

static WorkTableScan *
make_worktablescan(List *qptlist, List *qpqual, Index scanrelid, int wtParam)
{











}

ForeignScan *
make_foreignscan(List *qptlist, List *qpqual, Index scanrelid, List *fdw_exprs, List *fdw_private, List *fdw_scan_tlist, List *fdw_recheck_quals, Plan *outer_plan)
{






















}

static RecursiveUnion *
make_recursive_union(List *tlist, Plan *lefttree, Plan *righttree, int wtParam, List *distinctList, long numGroups)
{













































}

static BitmapAnd *
make_bitmap_and(List *bitmapplans)
{










}

static BitmapOr *
make_bitmap_or(List *bitmapplans)
{










}

static NestLoop *
make_nestloop(List *tlist, List *joinclauses, List *otherclauses, List *nestParams, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique)
{













}

static HashJoin *
make_hashjoin(List *tlist, List *joinclauses, List *otherclauses, List *hashclauses, List *hashoperators, List *hashcollations, List *hashkeys, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique)
{
















}

static Hash *
make_hash(Plan *lefttree, List *hashkeys, Oid skewTable, AttrNumber skewColumn, bool skewInherit)
{














}

static MergeJoin *
make_mergejoin(List *tlist, List *joinclauses, List *otherclauses, List *mergeclauses, Oid *mergefamilies, Oid *mergecollations, int *mergestrategies, bool *mergenullsfirst, Plan *lefttree, Plan *righttree, JoinType jointype, bool inner_unique, bool skip_mark_restore)
{


















}

/*
 * make_sort --- basic routine to build a Sort plan node
 *
 * Caller must have built the sortColIdx, sortOperators, collations, and
 * nullsFirst arrays already.
 */
static Sort *
make_sort(Plan *lefttree, int numCols, AttrNumber *sortColIdx, Oid *sortOperators, Oid *collations, bool *nullsFirst)
{














}

/*
 * prepare_sort_from_pathkeys
 *	  Prepare to sort according to given pathkeys
 *
 * This is used to set up for Sort, MergeAppend, and Gather Merge nodes.  It
 * calculates the executor's representation of the sort key information, and
 * adjusts the plan targetlist if needed to add resjunk sort columns.
 *
 * Input parameters:
 *	  'lefttree' is the plan node which yields input tuples
 *	  'pathkeys' is the list of pathkeys by which the result is to be sorted
 *	  'relids' identifies the child relation being sorted, if any
 *	  'reqColIdx' is NULL or an array of required sort key column numbers
 *	  'adjust_tlist_in_place' is true if lefttree must be modified in-place
 *
 * We must convert the pathkey information into arrays of sort key column
 * numbers, sort operator OIDs, collation OIDs, and nulls-first flags,
 * which is the representation the executor wants.  These are returned into
 * the output parameters *p_numsortkeys etc.
 *
 * When looking for matches to an EquivalenceClass's members, we will only
 * consider child EC members if they belong to given 'relids'.  This protects
 * against possible incorrect matches to child expressions that contain no
 * Vars.
 *
 * If reqColIdx isn't NULL then it contains sort key column numbers that
 * we should match.  This is used when making child plans for a MergeAppend;
 * it's an error if we can't match the columns.
 *
 * If the pathkeys include expressions that aren't simple Vars, we will
 * usually need to add resjunk items to the input plan's targetlist to
 * compute these expressions, since a Sort or MergeAppend node itself won't
 * do any such calculations.  If the input plan type isn't one that can do
 * projections, this means adding a Result node just to do the projection.
 * However, the caller can pass adjust_tlist_in_place = true to force the
 * lefttree tlist to be modified in-place regardless of whether the node type
 * can project --- we use this for fixing the tlist of MergeAppend itself.
 *
 * Returns the node which is to be the input to the Sort (either lefttree,
 * or a Result stacked atop lefttree).
 */
static Plan *
prepare_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids, const AttrNumber *reqColIdx, bool adjust_tlist_in_place, int *p_numsortkeys, AttrNumber **p_sortColIdx, Oid **p_sortOperators, Oid **p_collations, bool **p_nullsFirst)
{
















































































































































































































}

/*
 * find_ec_member_for_tle
 *		Locate an EquivalenceClass member matching the given TLE, if any
 *
 * Child EC members are ignored unless they belong to given 'relids'.
 */
static EquivalenceMember *
find_ec_member_for_tle(EquivalenceClass *ec, TargetEntry *tle, Relids relids)
{














































}

/*
 * make_sort_from_pathkeys
 *	  Create sort plan to sort according to given pathkeys
 *
 *	  'lefttree' is the node which yields input tuples
 *	  'pathkeys' is the list of pathkeys by which the result is to be sorted
 *	  'relids' is the set of relations required by
 *prepare_sort_from_pathkeys()
 */
static Sort *
make_sort_from_pathkeys(Plan *lefttree, List *pathkeys, Relids relids)
{











}

/*
 * make_sort_from_sortclauses
 *	  Create sort plan to sort according to given sortclauses
 *
 *	  'sortcls' is a list of SortGroupClauses
 *	  'lefttree' is the node which yields input tuples
 */
Sort *
make_sort_from_sortclauses(List *sortcls, Plan *lefttree)
{





























}

/*
 * make_sort_from_groupcols
 *	  Create sort plan to sort based on grouping columns
 *
 * 'groupcls' is the list of SortGroupClauses
 * 'grpColIdx' gives the column numbers to use
 *
 * This might look like it could be merged with make_sort_from_sortclauses,
 * but presently we *must* use the grpColIdx[] array to locate sort columns,
 * because the child plan's tlist is not marked with ressortgroupref info
 * appropriate to the grouping node.  So, only the sort ordering info
 * is used from the SortGroupClause entries.
 */
static Sort *
make_sort_from_groupcols(List *groupcls, AttrNumber *grpColIdx, Plan *lefttree)
{


































}

static Material *
make_material(Plan *lefttree)
{









}

/*
 * materialize_finished_plan: stick a Material node atop a completed plan
 *
 * There are a couple of places where we want to attach a Material node
 * after completion of create_plan(), without any MaterialPath path.
 * Those places should probably be refactored someday to do this on the
 * Path representation, but it's not worth the trouble yet.
 */
Plan *
materialize_finished_plan(Plan *subplan)
{

























}

Agg *
make_agg(List *tlist, List *qual, AggStrategy aggstrategy, AggSplit aggsplit, int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators, Oid *grpCollations, List *groupingSets, List *chain, double dNumGroups, Plan *lefttree)
{
























}

static WindowAgg *
make_windowagg(List *tlist, Index winref, int partNumCols, AttrNumber *partColIdx, Oid *partOperators, Oid *partCollations, int ordNumCols, AttrNumber *ordColIdx, Oid *ordOperators, Oid *ordCollations, int frameOptions, Node *startOffset, Node *endOffset, Oid startInRangeFunc, Oid endInRangeFunc, Oid inRangeColl, bool inRangeAsc, bool inRangeNullsFirst, Plan *lefttree)
{




























}

static Group *
make_group(List *tlist, List *qual, int numGroupCols, AttrNumber *grpColIdx, Oid *grpOperators, Oid *grpCollations, Plan *lefttree)
{














}

/*
 * distinctList is a list of SortGroupClauses, identifying the targetlist items
 * that should be considered by the Unique filter.  The input path must
 * already be sorted accordingly.
 */
static Unique *
make_unique_from_sortclauses(Plan *lefttree, List *distinctList)
{









































}

/*
 * as above, but use pathkeys to identify the sort columns and semantics
 */
static Unique *
make_unique_from_pathkeys(Plan *lefttree, List *pathkeys, int numCols)
{








































































































}

static Gather *
make_gather(List *qptlist, List *qpqual, int nworkers, int rescan_param, bool single_copy, Plan *subplan)
{














}

/*
 * distinctList is a list of SortGroupClauses, identifying the targetlist
 * items that should be considered by the SetOp filter.  The input path must
 * already be sorted accordingly.
 */
static SetOp *
make_setop(SetOpCmd cmd, SetOpStrategy strategy, Plan *lefttree, List *distinctList, AttrNumber flagColIdx, int firstFlag, long numGroups)
{













































}

/*
 * make_lockrows
 *	  Build a LockRows plan node
 */
static LockRows *
make_lockrows(Plan *lefttree, List *rowMarks, int epqParam)
{












}

/*
 * make_limit
 *	  Build a Limit plan node
 */
Limit *
make_limit(Plan *lefttree, Node *limitOffset, Node *limitCount)
{












}

/*
 * make_result
 *	  Build a Result plan node
 */
static Result *
make_result(List *tlist, Node *resconstantqual, Plan *subplan)
{










}

/*
 * make_project_set
 *	  Build a ProjectSet plan node
 */
static ProjectSet *
make_project_set(List *tlist, Plan *subplan)
{









}

/*
 * make_modifytable
 *	  Build a ModifyTable plan node
 */
static ModifyTable *
make_modifytable(PlannerInfo *root, CmdType operation, bool canSetTag, Index nominalRelation, Index rootRelation, bool partColsUpdated, List *resultRelations, List *subplans, List *subroots, List *withCheckOptionLists, List *returningLists, List *rowMarks, OnConflictExpr *onconflict, int epqParam)
{





































































































































}

/*
 * is_projection_capable_path
 *		Check whether a given Path node is able to do projection.
 */
bool
is_projection_capable_path(Path *path)
{



































}

/*
 * is_projection_capable_plan
 *		Check whether a given Plan node is able to do projection.
 */
bool
is_projection_capable_plan(Plan *plan)
{




























}