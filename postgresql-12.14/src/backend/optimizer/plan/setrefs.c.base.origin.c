/*-------------------------------------------------------------------------
 *
 * setrefs.c
 *	  Post-processing of a completed plan tree: fix references to subplan
 *	  vars, compute regproc values for operators, etc
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/setrefs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/tlist.h"
#include "tcop/utility.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

typedef struct
{
  Index varno;         /* RT index of Var */
  AttrNumber varattno; /* attr number of Var */
  AttrNumber resno;    /* TLE position of Var */
} tlist_vinfo;

typedef struct
{
  List *tlist;                             /* underlying target list */
  int num_vars;                            /* number of plain Var tlist entries */
  bool has_ph_vars;                        /* are there PlaceHolderVar entries? */
  bool has_non_vars;                       /* are there other entries? */
  tlist_vinfo vars[FLEXIBLE_ARRAY_MEMBER]; /* has num_vars entries */
} indexed_tlist;

typedef struct
{
  PlannerInfo *root;
  int rtoffset;
} fix_scan_expr_context;

typedef struct
{
  PlannerInfo *root;
  indexed_tlist *outer_itlist;
  indexed_tlist *inner_itlist;
  Index acceptable_rel;
  int rtoffset;
} fix_join_expr_context;

typedef struct
{
  PlannerInfo *root;
  indexed_tlist *subplan_itlist;
  Index newvarno;
  int rtoffset;
} fix_upper_expr_context;

/*
 * Check if a Const node is a regclass value.  We accept plain OID too,
 * since a regclass Const will get folded to that type if it's an argument
 * to oideq or similar operators.  (This might result in some extraneous
 * values in a plan's list of relation dependencies, but the worst result
 * would be occasional useless replans.)
 */
#define ISREGCLASSCONST(con) (((con)->consttype == REGCLASSOID || (con)->consttype == OIDOID) && !(con)->constisnull)

#define fix_scan_list(root, lst, rtoffset) ((List *)fix_scan_expr(root, (Node *)(lst), rtoffset))

static void
add_rtes_to_flat_rtable(PlannerInfo *root, bool recursing);
static void
flatten_unplanned_rtes(PlannerGlobal *glob, RangeTblEntry *rte);
static bool
flatten_rtes_walker(Node *node, PlannerGlobal *glob);
static void
add_rte_to_flat_rtable(PlannerGlobal *glob, RangeTblEntry *rte);
static Plan *
set_plan_refs(PlannerInfo *root, Plan *plan, int rtoffset);
static Plan *
set_indexonlyscan_references(PlannerInfo *root, IndexOnlyScan *plan, int rtoffset);
static Plan *
set_subqueryscan_references(PlannerInfo *root, SubqueryScan *plan, int rtoffset);
static bool
trivial_subqueryscan(SubqueryScan *plan);
static Plan *
clean_up_removed_plan_level(Plan *parent, Plan *child);
static void
set_foreignscan_references(PlannerInfo *root, ForeignScan *fscan, int rtoffset);
static void
set_customscan_references(PlannerInfo *root, CustomScan *cscan, int rtoffset);
static Plan *
set_append_references(PlannerInfo *root, Append *aplan, int rtoffset);
static Plan *
set_mergeappend_references(PlannerInfo *root, MergeAppend *mplan, int rtoffset);
static void
set_hash_references(PlannerInfo *root, Plan *plan, int rtoffset);
static Node *
fix_scan_expr(PlannerInfo *root, Node *node, int rtoffset);
static Node *
fix_scan_expr_mutator(Node *node, fix_scan_expr_context *context);
static bool
fix_scan_expr_walker(Node *node, fix_scan_expr_context *context);
static void
set_join_references(PlannerInfo *root, Join *join, int rtoffset);
static void
set_upper_references(PlannerInfo *root, Plan *plan, int rtoffset);
static void
set_param_references(PlannerInfo *root, Plan *plan);
static Node *
convert_combining_aggrefs(Node *node, void *context);
static void
set_dummy_tlist_references(Plan *plan, int rtoffset);
static indexed_tlist *
build_tlist_index(List *tlist);
static Var *
search_indexed_tlist_for_var(Var *var, indexed_tlist *itlist, Index newvarno, int rtoffset);
static Var *
search_indexed_tlist_for_non_var(Expr *node, indexed_tlist *itlist, Index newvarno);
static Var *
search_indexed_tlist_for_sortgroupref(Expr *node, Index sortgroupref, indexed_tlist *itlist, Index newvarno);
static List *
fix_join_expr(PlannerInfo *root, List *clauses, indexed_tlist *outer_itlist, indexed_tlist *inner_itlist, Index acceptable_rel, int rtoffset);
static Node *
fix_join_expr_mutator(Node *node, fix_join_expr_context *context);
static Node *
fix_upper_expr(PlannerInfo *root, Node *node, indexed_tlist *subplan_itlist, Index newvarno, int rtoffset);
static Node *
fix_upper_expr_mutator(Node *node, fix_upper_expr_context *context);
static List *
set_returning_clause_references(PlannerInfo *root, List *rlist, Plan *topplan, Index resultRelation, int rtoffset);

/*****************************************************************************
 *
 *		SUBPLAN REFERENCES
 *
 *****************************************************************************/

/*
 * set_plan_references
 *
 * This is the final processing pass of the planner/optimizer.  The plan
 * tree is complete; we just have to adjust some representational details
 * for the convenience of the executor:
 *
 * 1. We flatten the various subquery rangetables into a single list, and
 * zero out RangeTblEntry fields that are not useful to the executor.
 *
 * 2. We adjust Vars in scan nodes to be consistent with the flat rangetable.
 *
 * 3. We adjust Vars in upper plan nodes to refer to the outputs of their
 * subplans.
 *
 * 4. Aggrefs in Agg plan nodes need to be adjusted in some cases involving
 * partial aggregation or minmax aggregate optimization.
 *
 * 5. PARAM_MULTIEXPR Params are replaced by regular PARAM_EXEC Params,
 * now that we have finished planning all MULTIEXPR subplans.
 *
 * 6. We compute regproc OIDs for operators (ie, we look up the function
 * that implements each op).
 *
 * 7. We create lists of specific objects that the plan depends on.
 * This will be used by plancache.c to drive invalidation of cached plans.
 * Relation dependencies are represented by OIDs, and everything else by
 * PlanInvalItems (this distinction is motivated by the shared-inval APIs).
 * Currently, relations, user-defined functions, and domains are the only
 * types of objects that are explicitly tracked this way.
 *
 * 8. We assign every plan node in the tree a unique ID.
 *
 * We also perform one final optimization step, which is to delete
 * SubqueryScan, Append, and MergeAppend plan nodes that aren't doing
 * anything useful.  The reason for doing this last is that
 * it can't readily be done before set_plan_references, because it would
 * break set_upper_references: the Vars in the child plan's top tlist
 * wouldn't match up with the Vars in the outer plan tree.  A SubqueryScan
 * serves a necessary function as a buffer between outer query and subquery
 * variable numbering ... but after we've flattened the rangetable this is
 * no longer a problem, since then there's only one rtindex namespace.
 * Likewise, Append and MergeAppend buffer between the parent and child vars
 * of an appendrel, but we don't need to worry about that once we've done
 * set_plan_references.
 *
 * set_plan_references recursively traverses the whole plan tree.
 *
 * The return value is normally the same Plan node passed in, but can be
 * different when the passed-in Plan is a node we decide isn't needed.
 *
 * The flattened rangetable entries are appended to root->glob->finalrtable.
 * Also, rowmarks entries are appended to root->glob->finalrowmarks, and the
 * RT indexes of ModifyTable result relations to root->glob->resultRelations.
 * Plan dependencies are appended to root->glob->relationOids (for relations)
 * and root->glob->invalItems (for everything else).
 *
 * Notice that we modify Plan nodes in-place, but use expression_tree_mutator
 * to process targetlist and qual expressions.  We can assume that the Plan
 * nodes were just built by the planner and are not multiply referenced, but
 * it's not so safe to assume that for expression tree nodes.
 */
Plan *
set_plan_references(PlannerInfo *root, Plan *plan)
{
































}

/*
 * Extract RangeTblEntries from the plan's rangetable, and add to flat rtable
 *
 * This can recurse into subquery plans; "recursing" is true if so.
 */
static void
add_rtes_to_flat_rtable(PlannerInfo *root, bool recursing)
{

















































































}

/*
 * Extract RangeTblEntries from a subquery that was never planned at all
 */
static void
flatten_unplanned_rtes(PlannerGlobal *glob, RangeTblEntry *rte)
{


}

static bool
flatten_rtes_walker(Node *node, PlannerGlobal *glob)
{





















}

/*
 * Add (a copy of) the given RTE to the final rangetable
 *
 * In the flat rangetable, we zero out substructure pointers that are not
 * needed by the executor; this reduces the storage space and copying cost
 * for cached plans.  We keep only the ctename, alias and eref Alias fields,
 * which are needed by EXPLAIN, and the selectedCols, insertedCols,
 * updatedCols, and extraUpdatedCols bitmaps, which are needed for
 * executor-startup permissions checking and for trigger event checking.
 */
static void
add_rte_to_flat_rtable(PlannerGlobal *glob, RangeTblEntry *rte)
{













































}

/*
 * set_plan_refs: recurse through the Plan nodes of a single subquery level
 */
static Plan *
set_plan_refs(PlannerInfo *root, Plan *plan, int rtoffset)
{





































































































































































































































































































































































































































































}

/*
 * set_indexonlyscan_references
 *		Do set_plan_references processing on an IndexOnlyScan
 *
 * This is unlike the handling of a plain IndexScan because we have to
 * convert Vars referencing the heap into Vars referencing the index.
 * We can use the fix_upper_expr machinery for that, by working from a
 * targetlist describing the index columns.
 */
static Plan *
set_indexonlyscan_references(PlannerInfo *root, IndexOnlyScan *plan, int rtoffset)
{






































}

/*
 * set_subqueryscan_references
 *		Do set_plan_references processing on a SubqueryScan
 *
 * We try to strip out the SubqueryScan entirely; if we can't, we have
 * to do the normal processing on it.
 */
static Plan *
set_subqueryscan_references(PlannerInfo *root, SubqueryScan *plan, int rtoffset)
{

































}

/*
 * trivial_subqueryscan
 *		Detect whether a SubqueryScan can be deleted from the plan tree.
 *
 * We can delete it if it has no qual to check and the targetlist just
 * regurgitates the output of the child plan.
 */
static bool
trivial_subqueryscan(SubqueryScan *plan)
{
























































}

/*
 * clean_up_removed_plan_level
 *		Do necessary cleanup when we strip out a SubqueryScan, Append,
 *etc
 *
 * We are dropping the "parent" plan in favor of returning just its "child".
 * A few small tweaks are needed.
 */
static Plan *
clean_up_removed_plan_level(Plan *parent, Plan *child)
{











}

/*
 * set_foreignscan_references
 *	   Do set_plan_references processing on a ForeignScan
 */
static void
set_foreignscan_references(PlannerInfo *root, ForeignScan *fscan, int rtoffset)
{














































}

/*
 * set_customscan_references
 *	   Do set_plan_references processing on a CustomScan
 */
static void
set_customscan_references(PlannerInfo *root, CustomScan *cscan, int rtoffset)
{














































}

/*
 * set_append_references
 *		Do set_plan_references processing on an Append
 *
 * We try to strip out the Append entirely; if we can't, we have
 * to do the normal processing on it.
 */
static Plan *
set_append_references(PlannerInfo *root, Append *aplan, int rtoffset)
{
























































}

/*
 * set_mergeappend_references
 *		Do set_plan_references processing on a MergeAppend
 *
 * We try to strip out the MergeAppend entirely; if we can't, we have
 * to do the normal processing on it.
 */
static Plan *
set_mergeappend_references(PlannerInfo *root, MergeAppend *mplan, int rtoffset)
{
























































}

/*
 * set_hash_references
 *	   Do set_plan_references processing on a Hash node
 */
static void
set_hash_references(PlannerInfo *root, Plan *plan, int rtoffset)
{

















}

/*
 * copyVar
 *		Copy a Var node.
 *
 * fix_scan_expr and friends do this enough times that it's worth having
 * a bespoke routine instead of using the generic copyObject() function.
 */
static inline Var *
copyVar(Var *var)
{




}

/*
 * fix_expr_common
 *		Do generic set_plan_references processing on an expression node
 *
 * This is code that is common to all variants of expression-fixing.
 * We must look up operator opcode info for OpExpr and related nodes,
 * add OIDs from regclass Const nodes into root->glob->relationOids, and
 * add PlanInvalItems for user-defined functions into root->glob->invalItems.
 * We also fill in column index lists for GROUPING() expressions.
 *
 * We assume it's okay to update opcode info in-place.  So this could possibly
 * scribble on the planner's input data structures, but it's OK.
 */
static void
fix_expr_common(PlannerInfo *root, Node *node)
{






































































}

/*
 * fix_param_node
 *		Do set_plan_references processing on a Param
 *
 * If it's a PARAM_MULTIEXPR, replace it with the appropriate Param from
 * root->multiexpr_params; otherwise no change is needed.
 * Just for paranoia's sake, we make a copy of the node in either case.
 */
static Node *
fix_param_node(PlannerInfo *root, Param *p)
{


















}

/*
 * fix_scan_expr
 *		Do set_plan_references processing on a scan-level expression
 *
 * This consists of incrementing all Vars' varnos by rtoffset,
 * replacing PARAM_MULTIEXPR Params, expanding PlaceHolderVars,
 * replacing Aggref nodes that should be replaced by initplan output Params,
 * looking up operator opcode info for OpExpr and related nodes,
 * and adding OIDs from regclass Const nodes into root->glob->relationOids.
 */
static Node *
fix_scan_expr(PlannerInfo *root, Node *node, int rtoffset)
{
























}

static Node *
fix_scan_expr_mutator(Node *node, fix_scan_expr_context *context)
{









































































}

static bool
fix_scan_expr_walker(Node *node, fix_scan_expr_context *context)
{







}

/*
 * set_join_references
 *	  Modify the target list and quals of a join node to reference its
 *	  subplans, by setting the varnos to OUTER_VAR or INNER_VAR and setting
 *	  attno values to the result domain number of either the corresponding
 *	  outer or inner join tuple item.  Also perform opcode lookup for these
 *	  expressions, and add regclass OIDs to root->glob->relationOids.
 */
static void
set_join_references(PlannerInfo *root, Join *join, int rtoffset)
{

























































































}

/*
 * set_upper_references
 *	  Update the targetlist and quals of an upper-level plan node
 *	  to refer to the tuples returned by its lefttree subplan.
 *	  Also perform opcode lookup for these expressions, and
 *	  add regclass OIDs to root->glob->relationOids.
 *
 * This is used for single-input plan types like Agg, Group, Result.
 *
 * In most cases, we have to match up individual Vars in the tlist and
 * qual expressions with elements of the subplan's tlist (which was
 * generated by flattening these selfsame expressions, so it should have all
 * the required variables).  There is an important exception, however:
 * depending on where we are in the plan tree, sort/group columns may have
 * been pushed into the subplan tlist unflattened.  If these values are also
 * needed in the output then we want to reference the subplan tlist element
 * rather than recomputing the expression.
 */
static void
set_upper_references(PlannerInfo *root, Plan *plan, int rtoffset)
{



































}

/*
 * set_param_references
 *	  Initialize the initParam list in Gather or Gather merge node such that
 *	  it contains reference of all the params that needs to be evaluated
 *	  before execution of the node.  It contains the initplan params that
 *are being passed to the plan nodes below it.
 */
static void
set_param_references(PlannerInfo *root, Plan *plan)
{



































}

/*
 * Recursively scan an expression tree and convert Aggrefs to the proper
 * intermediate form for combining aggregates.  This means (1) replacing each
 * one's argument list with a single argument that is the original Aggref
 * modified to show partial aggregation and (2) changing the upper Aggref to
 * show combining aggregation.
 *
 * After this step, set_upper_references will replace the partial Aggrefs
 * with Vars referencing the lower Agg plan node's outputs, so that the final
 * form seen by the executor is a combining Aggref with a Var as input.
 *
 * It's rather messy to postpone this step until setrefs.c; ideally it'd be
 * done in createplan.c.  The difficulty is that once we modify the Aggref
 * expressions, they will no longer be equal() to their original form and
 * so cross-plan-node-level matches will fail.  So this has to happen after
 * the plan node above the Agg has resolved its subplan references.
 */
static Node *
convert_combining_aggrefs(Node *node, void *context)
{



















































}

/*
 * set_dummy_tlist_references
 *	  Replace the targetlist of an upper-level plan node with a simple
 *	  list of OUTER_VAR references to its child.
 *
 * This is used for plan types like Sort and Append that don't evaluate
 * their targetlists.  Although the executor doesn't care at all what's in
 * the tlist, EXPLAIN needs it to be realistic.
 *
 * Note: we could almost use set_upper_references() here, but it fails for
 * Append for lack of a lefttree subplan.  Single-purpose code is faster
 * anyway.
 */
static void
set_dummy_tlist_references(Plan *plan, int rtoffset)
{










































}

/*
 * build_tlist_index --- build an index data structure for a child tlist
 *
 * In most cases, subplan tlists will be "flat" tlists with only Vars,
 * so we try to optimize that case by extracting information about Vars
 * in advance.  Matching a parent tlist to a child is still an O(N^2)
 * operation, but at least with a much smaller constant factor than plain
 * tlist_member() searches.
 *
 * The result of this function is an indexed_tlist struct to pass to
 * search_indexed_tlist_for_var() or search_indexed_tlist_for_non_var().
 * When done, the indexed_tlist may be freed with a single pfree().
 */
static indexed_tlist *
build_tlist_index(List *tlist)
{







































}

/*
 * build_tlist_index_other_vars --- build a restricted tlist index
 *
 * This is like build_tlist_index, but we only index tlist entries that
 * are Vars belonging to some rel other than the one specified.  We will set
 * has_ph_vars (allowing PlaceHolderVars to be matched), but not has_non_vars
 * (so nothing other than Vars and PlaceHolderVars can be matched).
 */
static indexed_tlist *
build_tlist_index_other_vars(List *tlist, Index ignore_rel)
{






































}

/*
 * search_indexed_tlist_for_var --- find a Var in an indexed tlist
 *
 * If a match is found, return a copy of the given Var with suitably
 * modified varno/varattno (to wit, newvarno and the resno of the TLE entry).
 * Also ensure that varnoold is incremented by rtoffset.
 * If no match, return NULL.
 */
static Var *
search_indexed_tlist_for_var(Var *var, indexed_tlist *itlist, Index newvarno, int rtoffset)
{

























}

/*
 * search_indexed_tlist_for_non_var --- find a non-Var in an indexed tlist
 *
 * If a match is found, return a Var constructed to reference the tlist item.
 * If no match, return NULL.
 *
 * NOTE: it is a waste of time to call this unless itlist->has_ph_vars or
 * itlist->has_non_vars.  Furthermore, set_join_references() relies on being
 * able to prevent matching of non-Vars by clearing itlist->has_non_vars,
 * so there's a correctness reason not to call it unless that's set.
 */
static Var *
search_indexed_tlist_for_non_var(Expr *node, indexed_tlist *itlist, Index newvarno)
{


























}

/*
 * search_indexed_tlist_for_sortgroupref --- find a sort/group expression
 *
 * If a match is found, return a Var constructed to reference the tlist item.
 * If no match, return NULL.
 *
 * This is needed to ensure that we select the right subplan TLE in cases
 * where there are multiple textually-equal()-but-volatile sort expressions.
 * And it's also faster than search_indexed_tlist_for_non_var.
 */
static Var *
search_indexed_tlist_for_sortgroupref(Expr *node, Index sortgroupref, indexed_tlist *itlist, Index newvarno)
{



















}

/*
 * fix_join_expr
 *	   Create a new set of targetlist entries or join qual clauses by
 *	   changing the varno/varattno values of variables in the clauses
 *	   to reference target list values from the outer and inner join
 *	   relation target lists.  Also perform opcode lookup and add
 *	   regclass OIDs to root->glob->relationOids.
 *
 * This is used in three different scenarios:
 * 1) a normal join clause, where all the Vars in the clause *must* be
 *	  replaced by OUTER_VAR or INNER_VAR references.  In this case
 *	  acceptable_rel should be zero so that any failure to match a Var will
 *be reported as an error. 2) RETURNING clauses, which may contain both Vars of
 *the target relation and Vars of other relations. In this case we want to
 *replace the other-relation Vars by OUTER_VAR references, while leaving target
 *Vars alone. Thus inner_itlist = NULL and acceptable_rel = the ID of the target
 *relation should be passed. 3) ON CONFLICT UPDATE SET/WHERE clauses.  Here
 *references to EXCLUDED are to be replaced with INNER_VAR references, while
 *leaving target Vars (the to-be-updated relation) alone. Correspondingly
 *inner_itlist is to be EXCLUDED elements, outer_itlist = NULL and
 *acceptable_rel the target relation.
 *
 * 'clauses' is the targetlist or list of join clauses
 * 'outer_itlist' is the indexed target list of the outer join relation,
 *		or NULL
 * 'inner_itlist' is the indexed target list of the inner join relation,
 *		or NULL
 * 'acceptable_rel' is either zero or the rangetable index of a relation
 *		whose Vars may appear in the clause without provoking an error
 * 'rtoffset': how much to increment varnoold by
 *
 * Returns the new expression tree.  The original clause structure is
 * not modified.
 */
static List *
fix_join_expr(PlannerInfo *root, List *clauses, indexed_tlist *outer_itlist, indexed_tlist *inner_itlist, Index acceptable_rel, int rtoffset)
{








}

static Node *
fix_join_expr_mutator(Node *node, fix_join_expr_context *context)
{






























































































}

/*
 * fix_upper_expr
 *		Modifies an expression tree so that all Var nodes reference
 *outputs of a subplan.  Also looks for Aggref nodes that should be replaced by
 *initplan output Params.  Also performs opcode lookup, and adds regclass OIDs
 *to root->glob->relationOids.
 *
 * This is used to fix up target and qual expressions of non-join upper-level
 * plan nodes, as well as index-only scan nodes.
 *
 * An error is raised if no matching var can be found in the subplan tlist
 * --- so this routine should only be applied to nodes whose subplans'
 * targetlists were generated by flattening the expressions used in the
 * parent node.
 *
 * If itlist->has_non_vars is true, then we try to match whole subexpressions
 * against elements of the subplan tlist, so that we can avoid recomputing
 * expressions that were already computed by the subplan.  (This is relatively
 * expensive, so we don't want to try it in the common case where the
 * subplan tlist is just a flattened list of Vars.)
 *
 * 'node': the tree to be fixed (a target item or qual)
 * 'subplan_itlist': indexed target list for subplan (or index)
 * 'newvarno': varno to use for Vars referencing tlist elements
 * 'rtoffset': how much to increment varnoold by
 *
 * The resulting tree is a copy of the original in which all Var nodes have
 * varno = newvarno, varattno = resno of corresponding targetlist element.
 * The original tree is not modified.
 */
static Node *
fix_upper_expr(PlannerInfo *root, Node *node, indexed_tlist *subplan_itlist, Index newvarno, int rtoffset)
{







}

static Node *
fix_upper_expr_mutator(Node *node, fix_upper_expr_context *context)
{







































































}

/*
 * set_returning_clause_references
 *		Perform setrefs.c's work on a RETURNING targetlist
 *
 * If the query involves more than just the result table, we have to
 * adjust any Vars that refer to other tables to reference junk tlist
 * entries in the top subplan's targetlist.  Vars referencing the result
 * table should be left alone, however (the executor will evaluate them
 * using the actual heap tuple, after firing triggers if any).  In the
 * adjusted RETURNING list, result-table Vars will have their original
 * varno (plus rtoffset), but Vars for other rels will have varno OUTER_VAR.
 *
 * We also must perform opcode lookup and add regclass OIDs to
 * root->glob->relationOids.
 *
 * 'rlist': the RETURNING targetlist to be fixed
 * 'topplan': the top subplan node that will be just below the ModifyTable
 *		node (note it's not yet passed through set_plan_refs)
 * 'resultRelation': RT index of the associated result relation
 * 'rtoffset': how much to increment varnos by
 *
 * Note: the given 'root' is for the parent query level, not the 'topplan'.
 * This does not matter currently since we only access the dependency-item
 * lists in root->glob, but it would need some hacking if we wanted a root
 * that actually matches the subplan.
 *
 * Note: resultRelation is not yet adjusted by rtoffset.
 */
static List *
set_returning_clause_references(PlannerInfo *root, List *rlist, Plan *topplan, Index resultRelation, int rtoffset)
{























}

/*****************************************************************************
 *					QUERY DEPENDENCY MANAGEMENT
 *****************************************************************************/

/*
 * record_plan_function_dependency
 *		Mark the current plan as depending on a particular function.
 *
 * This is exported so that the function-inlining code can record a
 * dependency on a function that it's removed from the plan tree.
 */
void
record_plan_function_dependency(PlannerInfo *root, Oid funcid)
{






















}

/*
 * record_plan_type_dependency
 *		Mark the current plan as depending on a particular type.
 *
 * This is exported so that eval_const_expressions can record a
 * dependency on a domain that it's removed a CoerceToDomain node for.
 *
 * We don't currently need to record dependencies on domains that the
 * plan contains CoerceToDomain nodes for, though that might change in
 * future.  Hence, this isn't actually called in this module, though
 * someday fix_expr_common might call it.
 */
void
record_plan_type_dependency(PlannerInfo *root, Oid typid)
{


















}

/*
 * extract_query_dependencies
 *		Given a rewritten, but not yet planned, query or queries
 *		(i.e. a Query node or list of Query nodes), extract dependencies
 *		just as set_plan_references would do.  Also detect whether any
 *		rewrite steps were affected by RLS.
 *
 * This is needed by plancache.c to handle invalidation of cached unplanned
 * queries.
 *
 * Note: this does not go through eval_const_expressions, and hence doesn't
 * reflect its additions of inlined functions and elided CoerceToDomain nodes
 * to the invalItems list.  This is obviously OK for functions, since we'll
 * see them in the original query tree anyway.  For domains, it's OK because
 * we don't care about domains unless they get elided.  That is, a plan might
 * have domain dependencies that the query tree doesn't.
 */
void
extract_query_dependencies(Node *query, List **relationOids, List **invalItems, bool *hasRowSecurity)
{




















}

/*
 * Tree walker for extract_query_dependencies.
 *
 * This is exported so that expression_planner_with_deps can call it on
 * simple expressions (post-planning, not before planning, in that case).
 * In that usage, glob.dependsOnRole isn't meaningful, but the relationOids
 * and invalItems lists are added to as needed.
 */
bool
extract_query_dependencies_walker(Node *node, PlannerInfo *context)
{


















































}