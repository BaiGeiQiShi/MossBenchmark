/*-------------------------------------------------------------------------
 *
 * prepjointree.c
 *	  Planner preprocessing for subqueries and join tree manipulation.
 *
 * NOTE: the intended sequence for invoking these operations is
 *		replace_empty_jointree
 *		pull_up_sublinks
 *		inline_set_returning_functions
 *		pull_up_subqueries
 *		flatten_simple_union_all
 *		do expression preprocessing (including flattening JOIN alias
 *vars) reduce_outer_joins remove_useless_result_rtes
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/prep/prepjointree.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/placeholder.h"
#include "optimizer/prep.h"
#include "optimizer/subselect.h"
#include "optimizer/tlist.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define pull_varnos_of_level(a, b, c) pull_varnos_of_level_new(a, b, c)

typedef struct pullup_replace_vars_context
{
  PlannerInfo *root;
  List *targetlist;          /* tlist of subquery being pulled up */
  RangeTblEntry *target_rte; /* RTE of subquery */
  Relids relids;             /* relids within subquery, as numbered after
                              * pullup (set only if target_rte->lateral) */
  bool *outer_hasSubLinks;   /* -> outer query's hasSubLinks */
  int varno;                 /* varno of subquery */
  bool need_phvs;            /* do we need PlaceHolderVars? */
  bool wrap_non_vars;        /* do we need 'em on *all* non-Vars? */
  Node **rv_cache;           /* cache for results with PHVs */
} pullup_replace_vars_context;

typedef struct reduce_outer_joins_state
{
  Relids relids;       /* base relids within this subtree */
  bool contains_outer; /* does subtree contain outer join(s)? */
  List *sub_states;    /* List of states for subtree components */
} reduce_outer_joins_state;

static Node *
pull_up_sublinks_jointree_recurse(PlannerInfo *root, Node *jtnode, Relids *relids);
static Node *
pull_up_sublinks_qual_recurse(PlannerInfo *root, Node *node, Node **jtlink1, Relids available_rels1, Node **jtlink2, Relids available_rels2);
static Node *
pull_up_subqueries_recurse(PlannerInfo *root, Node *jtnode, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel);
static Node *
pull_up_simple_subquery(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel);
static Node *
pull_up_simple_union_all(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte);
static void
pull_up_union_leaf_queries(Node *setOp, PlannerInfo *root, int parentRTindex, Query *setOpQuery, int childRToffset);
static void
make_setop_translation_list(Query *query, Index newvarno, List **translated_vars);
static bool
is_simple_subquery(PlannerInfo *root, Query *subquery, RangeTblEntry *rte, JoinExpr *lowest_outer_join);
static Node *
pull_up_simple_values(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte);
static bool
is_simple_values(PlannerInfo *root, RangeTblEntry *rte);
static bool
is_simple_union_all(Query *subquery);
static bool
is_simple_union_all_recurse(Node *setOp, Query *setOpQuery, List *colTypes);
static bool
is_safe_append_member(Query *subquery);
static bool
jointree_contains_lateral_outer_refs(PlannerInfo *root, Node *jtnode, bool restricted, Relids safe_upper_varnos);
static void
replace_vars_in_jointree(Node *jtnode, pullup_replace_vars_context *context, JoinExpr *lowest_nulling_outer_join);
static Node *
pullup_replace_vars(Node *expr, pullup_replace_vars_context *context);
static Node *
pullup_replace_vars_callback(Var *var, replace_rte_variables_context *context);
static Query *
pullup_replace_vars_subquery(Query *query, pullup_replace_vars_context *context);
static reduce_outer_joins_state *
reduce_outer_joins_pass1(Node *jtnode);
static void
reduce_outer_joins_pass2(Node *jtnode, reduce_outer_joins_state *state, PlannerInfo *root, Relids nonnullable_rels, List *nonnullable_vars, List *forced_null_vars);
static Node *
remove_useless_results_recurse(PlannerInfo *root, Node *jtnode);
static int
get_result_relid(PlannerInfo *root, Node *jtnode);
static void
remove_result_refs(PlannerInfo *root, int varno, Node *newjtloc);
static bool
find_dependent_phvs(PlannerInfo *root, int varno);
static bool
find_dependent_phvs_in_jointree(PlannerInfo *root, Node *node, int varno);
static void
substitute_phv_relids(Node *node, int varno, Relids subrelids);
static void
fix_append_rel_relids(List *append_rel_list, int varno, Relids subrelids);
static Node *
find_jointree_node_for_rel(Node *jtnode, int relid);

/*
 * replace_empty_jointree
 *		If the Query's jointree is empty, replace it with a dummy
 *RTE_RESULT relation.
 *
 * By doing this, we can avoid a bunch of corner cases that formerly existed
 * for SELECTs with omitted FROM clauses.  An example is that a subquery
 * with empty jointree previously could not be pulled up, because that would
 * have resulted in an empty relid set, making the subquery not uniquely
 * identifiable for join or PlaceHolderVar processing.
 *
 * Unlike most other functions in this file, this function doesn't recurse;
 * we rely on other processing to invoke it on sub-queries at suitable times.
 */
void
replace_empty_jointree(Query *parse)
{





























}

/*
 * pull_up_sublinks
 *		Attempt to pull up ANY and EXISTS SubLinks to be treated as
 *		semijoins or anti-semijoins.
 *
 * A clause "foo op ANY (sub-SELECT)" can be processed by pulling the
 * sub-SELECT up to become a rangetable entry and treating the implied
 * comparisons as quals of a semijoin.  However, this optimization *only*
 * works at the top level of WHERE or a JOIN/ON clause, because we cannot
 * distinguish whether the ANY ought to return FALSE or NULL in cases
 * involving NULL inputs.  Also, in an outer join's ON clause we can only
 * do this if the sublink is degenerate (ie, references only the nullable
 * side of the join).  In that case it is legal to push the semijoin
 * down into the nullable side of the join.  If the sublink references any
 * nonnullable-side variables then it would have to be evaluated as part
 * of the outer join, which makes things way too complicated.
 *
 * Under similar conditions, EXISTS and NOT EXISTS clauses can be handled
 * by pulling up the sub-SELECT and creating a semijoin or anti-semijoin.
 *
 * This routine searches for such clauses and does the necessary parsetree
 * transformations if any are found.
 *
 * This routine has to run before preprocess_expression(), so the quals
 * clauses are not yet reduced to implicit-AND format, and are not guaranteed
 * to be AND/OR-flat either.  That means we need to recursively search through
 * explicit AND clauses.  We stop as soon as we hit a non-AND item.
 */
void
pull_up_sublinks(PlannerInfo *root)
{


















}

/*
 * Recurse through jointree nodes for pull_up_sublinks()
 *
 * In addition to returning the possibly-modified jointree node, we return
 * a relids set of the contained rels into *relids.
 */
static Node *
pull_up_sublinks_jointree_recurse(PlannerInfo *root, Node *jtnode, Relids *relids)
{


























































































































}

/*
 * Recurse through top-level qual nodes for pull_up_sublinks()
 *
 * jtlink1 points to the link in the jointree where any new JoinExprs should
 * be inserted if they reference available_rels1 (i.e., available_rels1
 * denotes the relations present underneath jtlink1).  Optionally, jtlink2 can
 * point to a second link where new JoinExprs should be inserted if they
 * reference available_rels2 (pass NULL for both those arguments if not used).
 * Note that SubLinks referencing both sets of variables cannot be optimized.
 * If we find multiple pull-up-able SubLinks, they'll get stacked onto jtlink1
 * and/or jtlink2 in the order we encounter them.  We rely on subsequent
 * optimization to rearrange the stack if appropriate.
 *
 * Returns the replacement qual node, or NULL if the qual should be removed.
 */
static Node *
pull_up_sublinks_qual_recurse(PlannerInfo *root, Node *node, Node **jtlink1, Relids available_rels1, Node **jtlink2, Relids available_rels2)
{













































































































































































}

/*
 * inline_set_returning_functions
 *		Attempt to "inline" set-returning functions in the FROM clause.
 *
 * If an RTE_FUNCTION rtable entry invokes a set-returning function that
 * contains just a simple SELECT, we can convert the rtable entry to an
 * RTE_SUBQUERY entry exposing the SELECT directly.  This is especially
 * useful if the subquery can then be "pulled up" for further optimization,
 * but we do it even if not, to reduce executor overhead.
 *
 * This has to be done before we have started to do any optimization of
 * subqueries, else any such steps wouldn't get applied to subqueries
 * obtained via inlining.  However, we do it after pull_up_sublinks
 * so that we can inline any functions used in SubLink subselects.
 *
 * Like most of the planner, this feels free to scribble on its input data
 * structure.
 */
void
inline_set_returning_functions(PlannerInfo *root)
{
























}

/*
 * pull_up_subqueries
 *		Look for subqueries in the rangetable that can be pulled up into
 *		the parent query.  If the subquery has no special features like
 *		grouping/aggregation then we can merge it into the parent's
 *jointree. Also, subqueries that are simple UNION ALL structures can be
 *		converted into "append relations".
 */
void
pull_up_subqueries(PlannerInfo *root)
{






}

/*
 * pull_up_subqueries_recurse
 *		Recursive guts of pull_up_subqueries.
 *
 * This recursively processes the jointree and returns a modified jointree.
 *
 * If this jointree node is within either side of an outer join, then
 * lowest_outer_join references the lowest such JoinExpr node; otherwise
 * it is NULL.  We use this to constrain the effects of LATERAL subqueries.
 *
 * If this jointree node is within the nullable side of an outer join, then
 * lowest_nulling_outer_join references the lowest such JoinExpr node;
 * otherwise it is NULL.  This forces use of the PlaceHolderVar mechanism for
 * references to non-nullable targetlist items, but only for references above
 * that join.
 *
 * If we are looking at a member subquery of an append relation,
 * containing_appendrel describes that relation; else it is NULL.
 * This forces use of the PlaceHolderVar mechanism for all non-Var targetlist
 * items, and puts some additional restrictions on what can be pulled up.
 *
 * A tricky aspect of this code is that if we pull up a subquery we have
 * to replace Vars that reference the subquery's outputs throughout the
 * parent query, including quals attached to jointree nodes above the one
 * we are currently processing!  We handle this by being careful to maintain
 * validity of the jointree structure while recursing, in the following sense:
 * whenever we recurse, all qual expressions in the tree must be reachable
 * from the top level, in case the recursive call needs to modify them.
 *
 * Notice also that we can't turn pullup_replace_vars loose on the whole
 * jointree, because it'd return a mutated copy of the tree; we have to
 * invoke it just on the quals, instead.  This behavior is what makes it
 * reasonable to pass lowest_outer_join and lowest_nulling_outer_join as
 * pointers rather than some more-indirect way of identifying the lowest
 * OJs.  Likewise, we don't replace append_rel_list members but only their
 * substructure, so the containing_appendrel reference is safe to use.
 */
static Node *
pull_up_subqueries_recurse(PlannerInfo *root, Node *jtnode, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel)
{


































































































}

/*
 * pull_up_simple_subquery
 *		Attempt to pull up a single simple subquery.
 *
 * jtnode is a RangeTblRef that has been tentatively identified as a simple
 * subquery by pull_up_subqueries.  We return the replacement jointree node,
 * or jtnode itself if we determine that the subquery can't be pulled up
 * after all.
 *
 * rte is the RangeTblEntry referenced by jtnode.  Remaining parameters are
 * as for pull_up_subqueries_recurse.
 */
static Node *
pull_up_simple_subquery(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte, JoinExpr *lowest_outer_join, JoinExpr *lowest_nulling_outer_join, AppendRelInfo *containing_appendrel)
{
























































































































































































































































































































































































}

/*
 * pull_up_simple_union_all
 *		Pull up a single simple UNION ALL subquery.
 *
 * jtnode is a RangeTblRef that has been identified as a simple UNION ALL
 * subquery by pull_up_subqueries.  We pull up the leaf subqueries and
 * build an "append relation" for the union set.  The result value is just
 * jtnode, since we don't actually need to change the query jointree.
 */
static Node *
pull_up_simple_union_all(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte)
{

























































}

/*
 * pull_up_union_leaf_queries -- recursive guts of pull_up_simple_union_all
 *
 * Build an AppendRelInfo for each leaf query in the setop tree, and then
 * apply pull_up_subqueries to the leaf query.
 *
 * Note that setOpQuery is the Query containing the setOp node, whose tlist
 * contains references to all the setop output columns.  When called from
 * pull_up_simple_union_all, this is *not* the same as root->parse, which is
 * the parent Query we are pulling up into.
 *
 * parentRTindex is the appendrel parent's index in root->parse->rtable.
 *
 * The child RTEs have already been copied to the parent.  childRToffset
 * tells us where in the parent's range table they were copied.  When called
 * from flatten_simple_union_all, childRToffset is 0 since the child RTEs
 * were already in root->parse->rtable and no RT index adjustment is needed.
 */
static void
pull_up_union_leaf_queries(Node *setOp, PlannerInfo *root, int parentRTindex, Query *setOpQuery, int childRToffset)
{

















































}

/*
 * make_setop_translation_list
 *	  Build the list of translations from parent Vars to child Vars for
 *	  a UNION ALL member.  (At this point it's just a simple list of
 *	  referencing Vars, but if we succeed in pulling up the member
 *	  subquery, the Vars will get replaced by pulled-up expressions.)
 */
static void
make_setop_translation_list(Query *query, Index newvarno, List **translated_vars)
{
















}

/*
 * is_simple_subquery
 *	  Check a subquery in the range table to see if it's simple enough
 *	  to pull up into the parent query.
 *
 * rte is the RTE_SUBQUERY RangeTblEntry that contained the subquery.
 * (Note subquery is not necessarily equal to rte->subquery; it could be a
 * processed copy of that.)
 * lowest_outer_join is the lowest outer join above the subquery, or NULL.
 */
static bool
is_simple_subquery(PlannerInfo *root, Query *subquery, RangeTblEntry *rte, JoinExpr *lowest_outer_join)
{


















































































































}

/*
 * pull_up_simple_values
 *		Pull up a single simple VALUES RTE.
 *
 * jtnode is a RangeTblRef that has been identified as a simple VALUES RTE
 * by pull_up_subqueries.  We always return a RangeTblRef representing a
 * RESULT RTE to replace it (all failure cases should have been detected by
 * is_simple_values()).  Actually, what we return is just jtnode, because
 * we replace the VALUES RTE in the rangetable with the RESULT RTE.
 *
 * rte is the RangeTblEntry referenced by jtnode.  Because of the limited
 * possible usage of VALUES RTEs, we do not need the remaining parameters
 * of pull_up_subqueries_recurse.
 */
static Node *
pull_up_simple_values(PlannerInfo *root, Node *jtnode, RangeTblEntry *rte)
{
































































































}

/*
 * is_simple_values
 *	  Check a VALUES RTE in the range table to see if it's simple enough
 *	  to pull up into the parent query.
 *
 * rte is the RTE_VALUES RangeTblEntry to check.
 */
static bool
is_simple_values(PlannerInfo *root, RangeTblEntry *rte)
{








































}

/*
 * is_simple_union_all
 *	  Check a subquery to see if it's a simple UNION ALL.
 *
 * We require all the setops to be UNION ALL (no mixing) and there can't be
 * any datatype coercions involved, ie, all the leaf queries must emit the
 * same datatypes.
 */
static bool
is_simple_union_all(Query *subquery)
{























}

static bool
is_simple_union_all_recurse(Node *setOp, Query *setOpQuery, List *colTypes)
{

































}

/*
 * is_safe_append_member
 *	  Check a subquery that is a leaf of a UNION ALL appendrel to see if
 *it's safe to pull up.
 */
static bool
is_safe_append_member(Query *subquery)
{









































}

/*
 * jointree_contains_lateral_outer_refs
 *		Check for disallowed lateral references in a jointree's quals
 *
 * If restricted is false, all level-1 Vars are allowed (but we still must
 * search the jointree, since it might contain outer joins below which there
 * will be restrictions).  If restricted is true, return true when any qual
 * in the jointree contains level-1 Vars coming from outside the rels listed
 * in safe_upper_varnos.
 */
static bool
jointree_contains_lateral_outer_refs(PlannerInfo *root, Node *jtnode, bool restricted, Relids safe_upper_varnos)
{































































}

/*
 * Helper routine for pull_up_subqueries: do pullup_replace_vars on every
 * expression in the jointree, without changing the jointree structure itself.
 * Ugly, but there's no other way...
 *
 * If we are at or below lowest_nulling_outer_join, we can suppress use of
 * PlaceHolderVars wrapped around the replacement expressions.
 */
static void
replace_vars_in_jointree(Node *jtnode, pullup_replace_vars_context *context, JoinExpr *lowest_nulling_outer_join)
{








































































































}

/*
 * Apply pullup variable replacement throughout an expression tree
 *
 * Returns a modified copy of the tree, so this can't be used where we
 * need to do in-place replacement.
 */
static Node *
pullup_replace_vars(Node *expr, pullup_replace_vars_context *context)
{

}

static Node *
pullup_replace_vars_callback(Var *var, replace_rte_variables_context *context)
{











































































































































































}

/*
 * Apply pullup variable replacement to a subquery
 *
 * This needs to be different from pullup_replace_vars() because
 * replace_rte_variables will think that it shouldn't increment sublevels_up
 * before entering the Query; so we need to call it with sublevels_up == 1.
 */
static Query *
pullup_replace_vars_subquery(Query *query, pullup_replace_vars_context *context)
{


}

/*
 * flatten_simple_union_all
 *		Try to optimize top-level UNION ALL structure into an appendrel
 *
 * If a query's setOperations tree consists entirely of simple UNION ALL
 * operations, flatten it into an append relation, which we can process more
 * intelligently than the general setops case.  Otherwise, do nothing.
 *
 * In most cases, this can succeed only for a top-level query, because for a
 * subquery in FROM, the parent query's invocation of pull_up_subqueries would
 * already have flattened the UNION via pull_up_simple_union_all.  But there
 * are a few cases we can support here but not in that code path, for example
 * when the subquery also contains ORDER BY.
 */
void
flatten_simple_union_all(PlannerInfo *root)
{

















































































}

/*
 * reduce_outer_joins
 *		Attempt to reduce outer joins to plain inner joins.
 *
 * The idea here is that given a query like
 *		SELECT ... FROM a LEFT JOIN b ON (...) WHERE b.y = 42;
 * we can reduce the LEFT JOIN to a plain JOIN if the "=" operator in WHERE
 * is strict.  The strict operator will always return NULL, causing the outer
 * WHERE to fail, on any row where the LEFT JOIN filled in NULLs for b's
 * columns.  Therefore, there's no need for the join to produce null-extended
 * rows in the first place --- which makes it a plain join not an outer join.
 * (This scenario may not be very likely in a query written out by hand, but
 * it's reasonably likely when pushing quals down into complex views.)
 *
 * More generally, an outer join can be reduced in strength if there is a
 * strict qual above it in the qual tree that constrains a Var from the
 * nullable side of the join to be non-null.  (For FULL joins this applies
 * to each side separately.)
 *
 * Another transformation we apply here is to recognize cases like
 *		SELECT ... FROM a LEFT JOIN b ON (a.x = b.y) WHERE b.y IS NULL;
 * If the join clause is strict for b.y, then only null-extended rows could
 * pass the upper WHERE, and we can conclude that what the query is really
 * specifying is an anti-semijoin.  We change the join type from JOIN_LEFT
 * to JOIN_ANTI.  The IS NULL clause then becomes redundant, and must be
 * removed to prevent bogus selectivity calculations, but we leave it to
 * distribute_qual_to_rels to get rid of such clauses.
 *
 * Also, we get rid of JOIN_RIGHT cases by flipping them around to become
 * JOIN_LEFT.  This saves some code here and in some later planner routines,
 * but the main reason to do it is to not need to invent a JOIN_REVERSE_ANTI
 * join type.
 *
 * To ease recognition of strict qual clauses, we require this routine to be
 * run after expression preprocessing (i.e., qual canonicalization and JOIN
 * alias-var expansion).
 */
void
reduce_outer_joins(PlannerInfo *root)
{




















}

/*
 * reduce_outer_joins_pass1 - phase 1 data collection
 *
 * Returns a state node describing the given jointree node.
 */
static reduce_outer_joins_state *
reduce_outer_joins_pass1(Node *jtnode)
{


























































}

/*
 * reduce_outer_joins_pass2 - phase 2 processing
 *
 *	jtnode: current jointree node
 *	state: state data collected by phase 1 for this node
 *	root: toplevel planner state
 *	nonnullable_rels: set of base relids forced non-null by upper quals
 *	nonnullable_vars: list of Vars forced non-null by upper quals
 *	forced_null_vars: list of Vars forced null by upper quals
 */
static void
reduce_outer_joins_pass2(Node *jtnode, reduce_outer_joins_state *state, PlannerInfo *root, Relids nonnullable_rels, List *nonnullable_vars, List *forced_null_vars)
{











































































































































































































































































}

/*
 * remove_useless_result_rtes
 *		Attempt to remove RTE_RESULT RTEs from the join tree.
 *
 * We can remove RTE_RESULT entries from the join tree using the knowledge
 * that RTE_RESULT returns exactly one row and has no output columns.  Hence,
 * if one is inner-joined to anything else, we can delete it.  Optimizations
 * are also possible for some outer-join cases, as detailed below.
 *
 * Some of these optimizations depend on recognizing empty (constant-true)
 * quals for FromExprs and JoinExprs.  That makes it useful to apply this
 * optimization pass after expression preprocessing, since that will have
 * eliminated constant-true quals, allowing more cases to be recognized as
 * optimizable.  What's more, the usual reason for an RTE_RESULT to be present
 * is that we pulled up a subquery or VALUES clause, thus very possibly
 * replacing Vars with constants, making it more likely that a qual can be
 * reduced to constant true.  Also, because some optimizations depend on
 * the outer-join type, it's best to have done reduce_outer_joins() first.
 *
 * A PlaceHolderVar referencing an RTE_RESULT RTE poses an obstacle to this
 * process: we must remove the RTE_RESULT's relid from the PHV's phrels, but
 * we must not reduce the phrels set to empty.  If that would happen, and
 * the RTE_RESULT is an immediate child of an outer join, we have to give up
 * and not remove the RTE_RESULT: there is noplace else to evaluate the
 * PlaceHolderVar.  (That is, in such cases the RTE_RESULT *does* have output
 * columns.)  But if the RTE_RESULT is an immediate child of an inner join,
 * we can usually change the PlaceHolderVar's phrels so as to evaluate it at
 * the inner join instead.  This is OK because we really only care that PHVs
 * are evaluated above or below the correct outer joins.  We can't, however,
 * postpone the evaluation of a PHV to above where it is used; so there are
 * some checks below on whether output PHVs are laterally referenced in the
 * other join input rel(s).
 *
 * We used to try to do this work as part of pull_up_subqueries() where the
 * potentially-optimizable cases get introduced; but it's way simpler, and
 * more effective, to do it separately.
 */
void
remove_useless_result_rtes(PlannerInfo *root)
{





































}

/*
 * remove_useless_results_recurse
 *		Recursive guts of remove_useless_result_rtes.
 *
 * This recursively processes the jointree and returns a modified jointree.
 */
static Node *
remove_useless_results_recurse(PlannerInfo *root, Node *jtnode)
{











































































































































































































}

/*
 * get_result_relid
 *		If jtnode is a RangeTblRef for an RTE_RESULT RTE, return its
 *relid; otherwise return 0.
 */
static int
get_result_relid(PlannerInfo *root, Node *jtnode)
{












}

/*
 * remove_result_refs
 *		Helper routine for dropping an unneeded RTE_RESULT RTE.
 *
 * This doesn't physically remove the RTE from the jointree, because that's
 * more easily handled in remove_useless_results_recurse.  What it does do
 * is the necessary cleanup in the rest of the tree: we must adjust any PHVs
 * that may reference the RTE.  Be sure to call this at a point where the
 * jointree is valid (no disconnected nodes).
 *
 * Note that we don't need to process the append_rel_list, since RTEs
 * referenced directly in the jointree won't be appendrel members.
 *
 * varno is the RTE_RESULT's relid.
 * newjtloc is the jointree location at which any PHVs referencing the
 * RTE_RESULT should be evaluated instead.
 */
static void
remove_result_refs(PlannerInfo *root, int varno, Node *newjtloc)
{
















}

/*
 * find_dependent_phvs - are there any PlaceHolderVars whose relids are
 * exactly the given varno?
 *
 * find_dependent_phvs should be used when we want to see if there are
 * any such PHVs anywhere in the Query.  Another use-case is to see if
 * a subtree of the join tree contains such PHVs; but for that, we have
 * to look not only at the join tree nodes themselves but at the
 * referenced RTEs.  For that, use find_dependent_phvs_in_jointree.
 */

typedef struct
{
  Relids relids;
  int sublevels_up;
} find_dependent_phvs_context;

static bool
find_dependent_phvs_walker(Node *node, find_dependent_phvs_context *context)
{































}

static bool
find_dependent_phvs(PlannerInfo *root, int varno)
{












}

static bool
find_dependent_phvs_in_jointree(PlannerInfo *root, Node *node, int varno)
{









































}

/*
 * substitute_phv_relids - adjust PlaceHolderVar relid sets after pulling up
 * a subquery or removing an RTE_RESULT jointree item
 *
 * Find any PlaceHolderVar nodes in the given tree that reference the
 * pulled-up relid, and change them to reference the replacement relid(s).
 *
 * NOTE: although this has the form of a walker, we cheat and modify the
 * nodes in-place.  This should be OK since the tree was copied by
 * pullup_replace_vars earlier.  Avoid scribbling on the original values of
 * the bitmapsets, though, because expression_tree_mutator doesn't copy those.
 */

typedef struct
{
  int varno;
  int sublevels_up;
  Relids subrelids;
} substitute_phv_relids_context;

static bool
substitute_phv_relids_walker(Node *node, substitute_phv_relids_context *context)
{


































}

static void
substitute_phv_relids(Node *node, int varno, Relids subrelids)
{










}

/*
 * fix_append_rel_relids: update RT-index fields of AppendRelInfo nodes
 *
 * When we pull up a subquery, any AppendRelInfo references to the subquery's
 * RT index have to be replaced by the substituted relid (and there had better
 * be only one).  We also need to apply substitute_phv_relids to their
 * translated_vars lists, since those might contain PlaceHolderVars.
 *
 * We assume we may modify the AppendRelInfo nodes in-place.
 */
static void
fix_append_rel_relids(List *append_rel_list, int varno, Relids subrelids)
{




























}

/*
 * get_relids_in_jointree: get set of RT indexes present in a jointree
 *
 * If include_joins is true, join RT indexes are included; if false,
 * only base rels are included.
 */
Relids
get_relids_in_jointree(Node *jtnode, bool include_joins)
{






































}

/*
 * get_relids_for_join: get set of base RT indexes making up a join
 */
Relids
get_relids_for_join(Query *query, int joinrelid)
{








}

/*
 * find_jointree_node_for_rel: locate jointree node for a base or join RT index
 *
 * Returns NULL if not found
 */
static Node *
find_jointree_node_for_rel(Node *jtnode, int relid)
{



















































}