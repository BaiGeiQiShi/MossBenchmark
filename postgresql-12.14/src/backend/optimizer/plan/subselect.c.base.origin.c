/*-------------------------------------------------------------------------
 *
 * subselect.c
 *	  Planning routines for subselects.
 *
 * This module deals with SubLinks and CTEs, but not subquery RTEs (i.e.,
 * not sub-SELECT-in-FROM cases).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/subselect.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/paramassign.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteManip.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)

typedef struct convert_testexpr_context
{
  PlannerInfo *root;
  List *subst_nodes; /* Nodes to substitute for Params */
} convert_testexpr_context;

typedef struct process_sublinks_context
{
  PlannerInfo *root;
  bool isTopQual;
} process_sublinks_context;

typedef struct finalize_primnode_context
{
  PlannerInfo *root;
  Bitmapset *paramids; /* Non-local PARAM_EXEC paramids found */
} finalize_primnode_context;

typedef struct inline_cte_walker_context
{
  const char *ctename; /* name and relative level of target CTE */
  int levelsup;
  Query *ctequery; /* query to substitute */
} inline_cte_walker_context;

static Node *
build_subplan(PlannerInfo *root, Plan *plan, PlannerInfo *subroot, List *plan_params, SubLinkType subLinkType, int subLinkId, Node *testexpr, List *testexpr_paramids, bool unknownEqFalse);
static List *
generate_subquery_params(PlannerInfo *root, List *tlist, List **paramIds);
static List *
generate_subquery_vars(PlannerInfo *root, List *tlist, Index varno);
static Node *
convert_testexpr(PlannerInfo *root, Node *testexpr, List *subst_nodes);
static Node *
convert_testexpr_mutator(Node *node, convert_testexpr_context *context);
static bool
subplan_is_hashable(Plan *plan);
static bool
testexpr_is_hashable(Node *testexpr, List *param_ids);
static bool
test_opexpr_is_hashable(OpExpr *testexpr, List *param_ids);
static bool
hash_ok_operator(OpExpr *expr);
static bool
SS_make_multiexprs_unique_walker(Node *node, void *context);
static bool
contain_dml(Node *node);
static bool
contain_dml_walker(Node *node, void *context);
static bool
contain_outer_selfref(Node *node);
static bool
contain_outer_selfref_walker(Node *node, Index *depth);
static void
inline_cte(PlannerInfo *root, CommonTableExpr *cte);
static bool
inline_cte_walker(Node *node, inline_cte_walker_context *context);
static bool
simplify_EXISTS_query(PlannerInfo *root, Query *query);
static Query *
convert_EXISTS_to_ANY(PlannerInfo *root, Query *subselect, Node **testexpr, List **paramIds);
static Node *
replace_correlation_vars_mutator(Node *node, PlannerInfo *root);
static Node *
process_sublinks_mutator(Node *node, process_sublinks_context *context);
static Bitmapset *
finalize_plan(PlannerInfo *root, Plan *plan, int gather_param, Bitmapset *valid_params, Bitmapset *scan_params);
static bool
finalize_primnode(Node *node, finalize_primnode_context *context);
static bool
finalize_agg_primnode(Node *node, finalize_primnode_context *context);

/*
 * Get the datatype/typmod/collation of the first column of the plan's output.
 *
 * This information is stored for ARRAY_SUBLINK execution and for
 * exprType()/exprTypmod()/exprCollation(), which have no way to get at the
 * plan associated with a SubPlan node.  We really only need the info for
 * EXPR_SUBLINK and ARRAY_SUBLINK subplans, but for consistency we save it
 * always.
 */
static void
get_first_col_type(Plan *plan, Oid *coltype, int32 *coltypmod, Oid *colcollation)
{
















}

/*
 * Convert a SubLink (as created by the parser) into a SubPlan.
 *
 * We are given the SubLink's contained query, type, ID, and testexpr.  We are
 * also told if this expression appears at top level of a WHERE/HAVING qual.
 *
 * Note: we assume that the testexpr has been AND/OR flattened (actually,
 * it's been through eval_const_expressions), but not converted to
 * implicit-AND form; and any SubLinks in it should already have been
 * converted to SubPlans.  The subquery is as yet untouched, however.
 *
 * The result is whatever we need to substitute in place of the SubLink node
 * in the executable expression.  If we're going to do the subplan as a
 * regular subplan, this will be the constructed SubPlan node.  If we're going
 * to do the subplan as an InitPlan, the SubPlan node instead goes into
 * root->init_plans, and what we return here is an expression tree
 * representing the InitPlan's result: usually just a Param node representing
 * a single scalar result, but possibly a row comparison tree containing
 * multiple Param nodes, or for a MULTIEXPR subquery a simple NULL constant
 * (since the real output Params are elsewhere in the tree, and the MULTIEXPR
 * subquery itself is in a resjunk tlist entry whose value is uninteresting).
 */
static Node *
make_subplan(PlannerInfo *root, Query *orig_subquery, SubLinkType subLinkType, int subLinkId, Node *testexpr, bool isTopQual)
{










































































































































}

/*
 * Build a SubPlan node given the raw inputs --- subroutine for make_subplan
 *
 * Returns either the SubPlan, or a replacement expression if we decide to
 * make it an InitPlan, as explained in the comments for make_subplan.
 */
static Node *
build_subplan(PlannerInfo *root, Plan *plan, PlannerInfo *subroot, List *plan_params, SubLinkType subLinkType, int subLinkId, Node *testexpr, List *testexpr_paramids, bool unknownEqFalse)
{




















































































































































































































































}

/*
 * generate_subquery_params: build a list of Params representing the output
 * columns of a sublink's sub-select, given the sub-select's targetlist.
 *
 * We also return an integer list of the paramids of the Params.
 */
static List *
generate_subquery_params(PlannerInfo *root, List *tlist, List **paramIds)
{






















}

/*
 * generate_subquery_vars: build a list of Vars representing the output
 * columns of a sublink's sub-select, given the sub-select's targetlist.
 * The Vars have the specified varno (RTE index).
 */
static List *
generate_subquery_vars(PlannerInfo *root, List *tlist, Index varno)
{



















}

/*
 * convert_testexpr: convert the testexpr given by the parser into
 * actually executable form.  This entails replacing PARAM_SUBLINK Params
 * with Params or Vars representing the results of the sub-select.  The
 * nodes to be substituted are passed in as the List result from
 * generate_subquery_params or generate_subquery_vars.
 */
static Node *
convert_testexpr(PlannerInfo *root, Node *testexpr, List *subst_nodes)
{





}

static Node *
convert_testexpr_mutator(Node *node, convert_testexpr_context *context)
{














































}

/*
 * subplan_is_hashable: can we implement an ANY subplan by hashing?
 */
static bool
subplan_is_hashable(Plan *plan)
{















}

/*
 * testexpr_is_hashable: is an ANY SubLink's test expression hashable?
 *
 * To identify LHS vs RHS of the hash expression, we must be given the
 * list of output Param IDs of the SubLink's subquery.
 */
static bool
testexpr_is_hashable(Node *testexpr, List *param_ids)
{
































}

static bool
test_opexpr_is_hashable(OpExpr *testexpr, List *param_ids)
{





































}

/*
 * Check expression is hashable + strict
 *
 * We could use op_hashjoinable() and op_strict(), but do it like this to
 * avoid a redundant cache lookup.
 */
static bool
hash_ok_operator(OpExpr *expr)
{



































}

/*
 * SS_make_multiexprs_unique
 *
 * After cloning an UPDATE targetlist that contains MULTIEXPR_SUBLINK
 * SubPlans, inheritance_planner() must call this to assign new, unique Param
 * IDs to the cloned MULTIEXPR_SUBLINKs' output parameters.  See notes in
 * ExecScanSubPlan.
 *
 * We do not need to renumber Param IDs for MULTIEXPR_SUBLINK plans that are
 * initplans, because those don't have input parameters that could cause
 * confusion.  Such initplans will not appear in the targetlist anyway, but
 * they still complicate matters because the surviving regular subplans might
 * not have consecutive subLinkIds.
 */
void
SS_make_multiexprs_unique(PlannerInfo *root, PlannerInfo *subroot)
{










































































}

/*
 * Locate PARAM_MULTIEXPR Params in an expression tree, and update as needed.
 * (We can update-in-place because the tree was already copied.)
 */
static bool
SS_make_multiexprs_unique_walker(Node *node, void *context)
{





































}

/*
 * SS_process_ctes: process a query's WITH list
 *
 * Consider each CTE in the WITH list and either ignore it (if it's an
 * unreferenced SELECT), "inline" it to create a regular sub-SELECT-in-FROM,
 * or convert it to an initplan.
 *
 * A side effect is to fill in root->cte_plan_ids with a list that
 * parallels root->parse->cteList and provides the subplan ID for
 * each CTE's initplan, or a dummy ID (-1) if we didn't make an initplan.
 */
void
SS_process_ctes(PlannerInfo *root)
{































































































































































}

/*
 * contain_dml: is any subquery not a plain SELECT?
 *
 * We reject SELECT FOR UPDATE/SHARE as well as INSERT etc.
 */
static bool
contain_dml(Node *node)
{

}

static bool
contain_dml_walker(Node *node, void *context)
{
















}

/*
 * contain_outer_selfref: is there an external recursive self-reference?
 */
static bool
contain_outer_selfref(Node *node)
{









}

static bool
contain_outer_selfref_walker(Node *node, Index *depth)
{

































}

/*
 * inline_cte: convert RTE_CTE references to given CTE into RTE_SUBQUERYs
 */
static void
inline_cte(PlannerInfo *root, CommonTableExpr *cte)
{








}

static bool
inline_cte_walker(Node *node, inline_cte_walker_context *context)
{
































































}

/*
 * convert_ANY_sublink_to_join: try to convert an ANY SubLink to a join
 *
 * The caller has found an ANY SubLink at the top level of one of the query's
 * qual clauses, but has not checked the properties of the SubLink further.
 * Decide whether it is appropriate to process this SubLink in join style.
 * If so, form a JoinExpr and return it.  Return NULL if the SubLink cannot
 * be converted to a join.
 *
 * The only non-obvious input parameter is available_rels: this is the set
 * of query rels that can safely be referenced in the sublink expression.
 * (We must restrict this to avoid changing the semantics when a sublink
 * is present in an outer join's ON qual.)  The conversion must fail if
 * the converted qual would reference any but these parent-query relids.
 *
 * On success, the returned JoinExpr has larg = NULL and rarg = the jointree
 * item representing the pulled-up subquery.  The caller must set larg to
 * represent the relation(s) on the lefthand side of the new join, and insert
 * the JoinExpr into the upper query's jointree at an appropriate place
 * (typically, where the lefthand relation(s) had been).  Note that the
 * passed-in SubLink must also be removed from its original position in the
 * query quals, since the quals of the returned JoinExpr replace it.
 * (Notionally, we replace the SubLink with a constant TRUE, then elide the
 * redundant constant from the qual.)
 *
 * On success, the caller is also responsible for recursively applying
 * pull_up_sublinks processing to the rarg and quals of the returned JoinExpr.
 * (On failure, there is no need to do anything, since pull_up_sublinks will
 * be applied when we recursively plan the sub-select.)
 *
 * Side effects of a successful conversion include adding the SubLink's
 * subselect to the query's rangetable, so that it can be referenced in
 * the JoinExpr's rarg.
 */
JoinExpr *
convert_ANY_sublink_to_join(PlannerInfo *root, SubLink *sublink, Relids available_rels)
{






























































































}

/*
 * convert_EXISTS_sublink_to_join: try to convert an EXISTS SubLink to a join
 *
 * The API of this function is identical to convert_ANY_sublink_to_join's,
 * except that we also support the case where the caller has found NOT EXISTS,
 * so we need an additional input parameter "under_not".
 */
JoinExpr *
convert_EXISTS_sublink_to_join(PlannerInfo *root, SubLink *sublink, bool under_not, Relids available_rels)
{































































































































































}

/*
 * simplify_EXISTS_query: remove any useless stuff in an EXISTS's subquery
 *
 * The only thing that matters about an EXISTS query is whether it returns
 * zero or more than zero rows.  Therefore, we can remove certain SQL features
 * that won't affect that.  The only part that is really likely to matter in
 * typical usage is simplifying the targetlist: it's a common habit to write
 * "SELECT * FROM" even though there is no need to evaluate any columns.
 *
 * Note: by suppressing the targetlist we could cause an observable behavioral
 * change, namely that any errors that might occur in evaluating the tlist
 * won't occur, nor will other side-effects of volatile functions.  This seems
 * unlikely to bother anyone in practice.
 *
 * Returns true if was able to discard the targetlist, else false.
 */
static bool
simplify_EXISTS_query(PlannerInfo *root, Query *query)
{


































































}

/*
 * convert_EXISTS_to_ANY: try to convert EXISTS to a hashable ANY sublink
 *
 * The subselect is expected to be a fresh copy that we can munge up,
 * and to have been successfully passed through simplify_EXISTS_query.
 *
 * On success, the modified subselect is returned, and we store a suitable
 * upper-level test expression at *testexpr, plus a list of the subselect's
 * output Params at *paramIds.  (The test expression is already Param-ified
 * and hence need not go through convert_testexpr, which is why we have to
 * deal with the Param IDs specially.)
 *
 * On failure, returns NULL.
 */
static Query *
convert_EXISTS_to_ANY(PlannerInfo *root, Query *subselect, Node **testexpr, List **paramIds)
{








































































































































































































}

/*
 * Replace correlation vars (uplevel vars) with Params.
 *
 * Uplevel PlaceHolderVars and aggregates are replaced, too.
 *
 * Note: it is critical that this runs immediately after SS_process_sublinks.
 * Since we do not recurse into the arguments of uplevel PHVs and aggregates,
 * they will get copied to the appropriate subplan args list in the parent
 * query with uplevel vars not replaced by Params, but only adjusted in level
 * (see replace_outer_placeholdervar and replace_outer_agg).  That's exactly
 * what we want for the vars of the parent level --- but if a PHV's or
 * aggregate's argument contains any further-up variables, they have to be
 * replaced with Params in their turn. That will happen when the parent level
 * runs SS_replace_correlation_vars.  Therefore it must do so after expanding
 * its sublinks to subplans.  And we don't want any steps in between, else
 * those steps would never get applied to the argument expressions, either in
 * the parent or the child level.
 *
 * Another fairly tricky thing going on here is the handling of SubLinks in
 * the arguments of uplevel PHVs/aggregates.  Those are not touched inside the
 * intermediate query level, either.  Instead, SS_process_sublinks recurses on
 * them after copying the PHV or Aggref expression into the parent plan level
 * (this is actually taken care of in build_subplan).
 */
Node *
SS_replace_correlation_vars(PlannerInfo *root, Node *expr)
{


}

static Node *
replace_correlation_vars_mutator(Node *node, PlannerInfo *root)
{

































}

/*
 * Expand SubLinks to SubPlans in the given expression.
 *
 * The isQual argument tells whether or not this expression is a WHERE/HAVING
 * qualifier expression.  If it is, any sublinks appearing at top level need
 * not distinguish FALSE from UNKNOWN return values.
 */
Node *
SS_process_sublinks(PlannerInfo *root, Node *expr, bool isQual)
{





}

static Node *
process_sublinks_mutator(Node *node, process_sublinks_context *context)
{







































































































































}

/*
 * SS_identify_outer_params - identify the Params available from outer levels
 *
 * This must be run after SS_replace_correlation_vars and SS_process_sublinks
 * processing is complete in a given query level as well as all of its
 * descendant levels (which means it's most practical to do it at the end of
 * processing the query level).  We compute the set of paramIds that outer
 * levels will make available to this level+descendants, and record it in
 * root->outer_params for use while computing extParam/allParam sets in final
 * plan cleanup.  (We can't just compute it then, because the upper levels'
 * plan_params lists are transient and will be gone by then.)
 */
void
SS_identify_outer_params(PlannerInfo *root)
{















































}

/*
 * SS_charge_for_initplans - account for initplans in Path costs & parallelism
 *
 * If any initPlans have been created in the current query level, they will
 * get attached to the Plan tree created from whichever Path we select from
 * the given rel.  Increment all that rel's Paths' costs to account for them,
 * and make sure the paths get marked as parallel-unsafe, since we can't
 * currently transmit initPlans to parallel workers.
 *
 * This is separate from SS_attach_initplans because we might conditionally
 * create more initPlans during create_plan(), depending on which Path we
 * select.  However, Paths that would generate such initPlans are expected
 * to have included their cost already.
 */
void
SS_charge_for_initplans(PlannerInfo *root, RelOptInfo *final_rel)
{











































}

/*
 * SS_attach_initplans - attach initplans to topmost plan node
 *
 * Attach any initplans created in the current query level to the specified
 * plan node, which should normally be the topmost node for the query level.
 * (In principle the initPlans could go in any node at or above where they're
 * referenced; but there seems no reason to put them any lower than the
 * topmost node, so we don't bother to track exactly where they came from.)
 * We do not touch the plan node's cost; the initplans should have been
 * accounted for in path costing.
 */
void
SS_attach_initplans(PlannerInfo *root, Plan *plan)
{

}

/*
 * SS_finalize_plan - do final parameter processing for a completed Plan.
 *
 * This recursively computes the extParam and allParam sets for every Plan
 * node in the given plan tree.  (Oh, and RangeTblFunction.funcparams too.)
 *
 * We assume that SS_finalize_plan has already been run on any initplans or
 * subplans the plan tree could reference.
 */
void
SS_finalize_plan(PlannerInfo *root, Plan *plan)
{


}

/*
 * Recursive processing of all nodes in the plan tree
 *
 * gather_param is the rescan_param of an ancestral Gather/GatherMerge,
 * or -1 if there is none.
 *
 * valid_params is the set of param IDs supplied by outer plan levels
 * that are valid to reference in this plan node or its children.
 *
 * scan_params is a set of param IDs to force scan plan nodes to reference.
 * This is for EvalPlanQual support, and is always NULL at the top of the
 * recursion.
 *
 * The return value is the computed allParam set for the given Plan node.
 * This is just an internal notational convenience: we can add a child
 * plan's allParams to the set of param IDs of interest to this level
 * in the same statement that recurses to that child.
 *
 * Do not scribble on caller's values of valid_params or scan_params!
 *
 * Note: although we attempt to deal with initPlans anywhere in the tree, the
 * logic is not really right.  The problem is that a plan node might return an
 * output Param of its initPlan as a targetlist item, in which case it's valid
 * for the parent plan level to reference that same Param; the parent's usage
 * will be converted into a Var referencing the child plan node by setrefs.c.
 * But this function would see the parent's reference as out of scope and
 * complain about it.  For now, this does not matter because the planner only
 * attaches initPlans to the topmost plan node in a query level, so the case
 * doesn't arise.  If we ever merge this processing into setrefs.c, maybe it
 * can be handled more cleanly.
 */
static Bitmapset *
finalize_plan(PlannerInfo *root, Plan *plan, int gather_param, Bitmapset *valid_params, Bitmapset *scan_params)
{

































































































































































































































































































































































































































































































































}

/*
 * finalize_primnode: add IDs of all PARAM_EXEC params appearing in the given
 * expression tree to the result set.
 */
static bool
finalize_primnode(Node *node, finalize_primnode_context *context)
{























































}

/*
 * finalize_agg_primnode: find all Aggref nodes in the given expression tree,
 * and add IDs of all PARAM_EXEC params appearing within their aggregated
 * arguments to the result set.
 */
static bool
finalize_agg_primnode(Node *node, finalize_primnode_context *context)
{














}

/*
 * SS_make_initplan_output_param - make a Param for an initPlan's output
 *
 * The plan is expected to return a scalar value of the given type/collation.
 *
 * Note that in some cases the initplan may not ever appear in the finished
 * plan tree.  If that happens, we'll have wasted a PARAM_EXEC slot, which
 * is no big deal.
 */
Param *
SS_make_initplan_output_param(PlannerInfo *root, Oid resulttype, int32 resulttypmod, Oid resultcollation)
{

}

/*
 * SS_make_initplan_from_plan - given a plan tree, make it an InitPlan
 *
 * We build an EXPR_SUBLINK SubPlan node and put it into the initplan
 * list for the outer query level.  A Param that represents the initplan's
 * output has already been assigned using SS_make_initplan_output_param.
 */
void
SS_make_initplan_from_plan(PlannerInfo *root, PlannerInfo *subroot, Plan *plan, Param *prm)
{






























}