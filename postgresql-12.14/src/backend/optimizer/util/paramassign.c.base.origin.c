/*-------------------------------------------------------------------------
 *
 * paramassign.c
 *		Functions for assigning PARAM_EXEC slots during planning.
 *
 * This module is responsible for managing three planner data structures:
 *
 * root->glob->paramExecTypes: records actual assignments of PARAM_EXEC slots.
 * The i'th list element holds the data type OID of the i'th parameter slot.
 * (Elements can be InvalidOid if they represent slots that are needed for
 * chgParam signaling, but will never hold a value at runtime.)  This list is
 * global to the whole plan since the executor has only one PARAM_EXEC array.
 * Assignments are permanent for the plan: we never remove entries once added.
 *
 * root->plan_params: a list of PlannerParamItem nodes, recording Vars and
 * PlaceHolderVars that the root's query level needs to supply to lower-level
 * subqueries, along with the PARAM_EXEC number to use for each such value.
 * Elements are added to this list while planning a subquery, and the list
 * is reset to empty after completion of each subquery.
 *
 * root->curOuterParams: a list of NestLoopParam nodes, recording Vars and
 * PlaceHolderVars that some outer level of nestloop needs to pass down to
 * a lower-level plan node in its righthand side.  Elements are added to this
 * list as createplan.c creates lower Plan nodes that need such Params, and
 * are removed when it creates a NestLoop Plan node that will supply those
 * values.
 *
 * The latter two data structures are used to prevent creating multiple
 * PARAM_EXEC slots (each requiring work to fill) when the same upper
 * SubPlan or NestLoop supplies a value that is referenced in more than
 * one place in its child plan nodes.  However, when the same Var has to
 * be supplied to different subplan trees by different SubPlan or NestLoop
 * parent nodes, we don't recognize any commonality; a fresh plan_params or
 * curOuterParams entry will be made (since the old one has been removed
 * when we finished processing the earlier SubPlan or NestLoop) and a fresh
 * PARAM_EXEC number will be assigned.  At one time we tried to avoid
 * allocating duplicate PARAM_EXEC numbers in such cases, but it's harder
 * than it seems to avoid bugs due to overlapping Param lifetimes, so we
 * don't risk that anymore.  Minimizing the number of PARAM_EXEC slots
 * doesn't really save much executor work anyway.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/paramassign.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/nodeFuncs.h"
#include "nodes/plannodes.h"
#include "optimizer/paramassign.h"
#include "optimizer/placeholder.h"
#include "rewrite/rewriteManip.h"

/*
 * Select a PARAM_EXEC number to identify the given Var as a parameter for
 * the current subquery.  (It might already have one.)
 * Record the need for the Var in the proper upper-level root->plan_params.
 */
static int
assign_param_for_var(PlannerInfo *root, Var *var)
{









































}

/*
 * Generate a Param node to replace the given Var,
 * which is expected to have varlevelsup > 0 (ie, it is not local).
 * Record the need for the Var in the proper upper-level root->plan_params.
 */
Param *
replace_outer_var(PlannerInfo *root, Var *var)
{

















}

/*
 * Select a PARAM_EXEC number to identify the given PlaceHolderVar as a
 * parameter for the current subquery.  (It might already have one.)
 * Record the need for the PHV in the proper upper-level root->plan_params.
 *
 * This is just like assign_param_for_var, except for PlaceHolderVars.
 */
static int
assign_param_for_placeholdervar(PlannerInfo *root, PlaceHolderVar *phv)
{







































}

/*
 * Generate a Param node to replace the given PlaceHolderVar,
 * which is expected to have phlevelsup > 0 (ie, it is not local).
 * Record the need for the PHV in the proper upper-level root->plan_params.
 *
 * This is just like replace_outer_var, except for PlaceHolderVars.
 */
Param *
replace_outer_placeholdervar(PlannerInfo *root, PlaceHolderVar *phv)
{

















}

/*
 * Generate a Param node to replace the given Aggref
 * which is expected to have agglevelsup > 0 (ie, it is not local).
 * Record the need for the Aggref in the proper upper-level root->plan_params.
 */
Param *
replace_outer_agg(PlannerInfo *root, Aggref *agg)
{




































}

/*
 * Generate a Param node to replace the given GroupingFunc expression which is
 * expected to have agglevelsup > 0 (ie, it is not local).
 * Record the need for the GroupingFunc in the proper upper-level
 * root->plan_params.
 */
Param *
replace_outer_grouping(PlannerInfo *root, GroupingFunc *grp)
{





































}

/*
 * Generate a Param node to replace the given Var,
 * which is expected to come from some upper NestLoop plan node.
 * Record the need for the Var in root->curOuterParams.
 */
Param *
replace_nestloop_param_var(PlannerInfo *root, Var *var)
{


































}

/*
 * Generate a Param node to replace the given PlaceHolderVar,
 * which is expected to come from some upper NestLoop plan node.
 * Record the need for the PHV in root->curOuterParams.
 *
 * This is just like replace_nestloop_param_var, except for PlaceHolderVars.
 */
Param *
replace_nestloop_param_placeholdervar(PlannerInfo *root, PlaceHolderVar *phv)
{

































}

/*
 * process_subquery_nestloop_params
 *	  Handle params of a parameterized subquery that need to be fed
 *	  from an outer nestloop.
 *
 * Currently, that would be *all* params that a subquery in FROM has demanded
 * from the current query level, since they must be LATERAL references.
 *
 * subplan_params is a list of PlannerParamItems that we intend to pass to
 * a subquery-in-FROM.  (This was constructed in root->plan_params while
 * planning the subquery, but isn't there anymore when this is called.)
 *
 * The subplan's references to the outer variables are already represented
 * as PARAM_EXEC Params, since that conversion was done by the routines above
 * while planning the subquery.  So we need not modify the subplan or the
 * PlannerParamItems here.  What we do need to do is add entries to
 * root->curOuterParams to signal the parent nestloop plan node that it must
 * provide these values.  This differs from replace_nestloop_param_var in
 * that the PARAM_EXEC slots to use have already been determined.
 *
 * Note that we also use root->curOuterRels as an implicit parameter for
 * sanity checks.
 */
void
process_subquery_nestloop_params(PlannerInfo *root, List *subplan_params)
{











































































}

/*
 * Identify any NestLoopParams that should be supplied by a NestLoop plan
 * node with the specified lefthand rels.  Remove them from the active
 * root->curOuterParams list and return them as the result list.
 */
List *
identify_current_nestloop_params(PlannerInfo *root, Relids leftrelids)
{


































}

/*
 * Generate a new Param node that will not conflict with any other.
 *
 * This is used to create Params representing subplan outputs or
 * NestLoop parameters.
 *
 * We don't need to build a PlannerParamItem for such a Param, but we do
 * need to make sure we record the type in paramExecTypes (otherwise,
 * there won't be a slot allocated for it).
 */
Param *
generate_new_exec_param(PlannerInfo *root, Oid paramtype, int32 paramtypmod, Oid paramcollation)
{












}

/*
 * Assign a (nonnegative) PARAM_EXEC ID for a special parameter (one that
 * is not actually used to carry a value at runtime).  Such parameters are
 * used for special runtime signaling purposes, such as connecting a
 * recursive union node to its worktable scan node or forcing plan
 * re-evaluation within the EvalPlanQual mechanism.  No actual Param node
 * exists with this ID, however.
 */
int
assign_special_exec_param(PlannerInfo *root)
{




}