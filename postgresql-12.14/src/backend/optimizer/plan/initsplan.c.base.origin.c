/*-------------------------------------------------------------------------
 *
 * initsplan.c
 *	  Target list, qualification, joininfo initialization routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/plan/initsplan.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "catalog/pg_class.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/inherit.h"
#include "optimizer/joininfo.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"
#include "optimizer/planner.h"
#include "optimizer/prep.h"
#include "optimizer/restrictinfo.h"
#include "parser/analyze.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

/* These parameters are set by GUC */
int from_collapse_limit;
int join_collapse_limit;

/* Elements of the postponed_qual_list used during deconstruct_recurse */
typedef struct PostponedQual
{
  Node *qual;    /* a qual clause waiting to be processed */
  Relids relids; /* the set of baserels it references */
} PostponedQual;

static void
extract_lateral_references(PlannerInfo *root, RelOptInfo *brel, Index rtindex);
static List *
deconstruct_recurse(PlannerInfo *root, Node *jtnode, bool below_outer_join, Relids *qualscope, Relids *inner_join_rels, List **postponed_qual_list);
static void
process_security_barrier_quals(PlannerInfo *root, int rti, Relids qualscope, bool below_outer_join);
static SpecialJoinInfo *
make_outerjoininfo(PlannerInfo *root, Relids left_rels, Relids right_rels, Relids inner_join_rels, JoinType jointype, List *clause);
static void
compute_semijoin_info(PlannerInfo *root, SpecialJoinInfo *sjinfo, List *clause);
static void
distribute_qual_to_rels(PlannerInfo *root, Node *clause, bool is_deduced, bool below_outer_join, JoinType jointype, Index security_level, Relids qualscope, Relids ojscope, Relids outerjoin_nonnullable, Relids deduced_nullable_relids, List **postponed_qual_list);
static bool
check_outerjoin_delay(PlannerInfo *root, Relids *relids_p, Relids *nullable_relids_p, bool is_pushed_down);
static bool
check_equivalence_delay(PlannerInfo *root, RestrictInfo *restrictinfo);
static bool
check_redundant_nullability_qual(PlannerInfo *root, Node *clause);
static void
check_mergejoinable(RestrictInfo *restrictinfo);
static void
check_hashjoinable(RestrictInfo *restrictinfo);

/*****************************************************************************
 *
 *	 JOIN TREES
 *
 *****************************************************************************/

/*
 * add_base_rels_to_query
 *
 *	  Scan the query's jointree and create baserel RelOptInfos for all
 *	  the base relations (e.g., table, subquery, and function RTEs)
 *	  appearing in the jointree.
 *
 * The initial invocation must pass root->parse->jointree as the value of
 * jtnode.  Internally, the function recurses through the jointree.
 *
 * At the end of this process, there should be one baserel RelOptInfo for
 * every non-join RTE that is used in the query.  Some of the baserels
 * may be appendrel parents, which will require additional "otherrel"
 * RelOptInfos for their member rels, but those are added later.
 */
void
add_base_rels_to_query(PlannerInfo *root, Node *jtnode)
{































}

/*
 * add_other_rels_to_query
 *	  create "otherrel" RelOptInfos for the children of appendrel baserels
 *
 * At the end of this process, there should be RelOptInfos for all relations
 * that will be scanned by the query.
 */
void
add_other_rels_to_query(PlannerInfo *root)
{

























}

/*****************************************************************************
 *
 *	 TARGET LISTS
 *
 *****************************************************************************/

/*
 * build_base_rel_tlists
 *	  Add targetlist entries for each var needed in the query's final tlist
 *	  (and HAVING clause, if any) to the appropriate base relations.
 *
 * We mark such vars as needed by "relation 0" to ensure that they will
 * propagate up through all join plan steps.
 */
void
build_base_rel_tlists(PlannerInfo *root, List *final_tlist)
{






















}

/*
 * add_vars_to_targetlist
 *	  For each variable appearing in the list, add it to the owning
 *	  relation's targetlist if not already present, and mark the variable
 *	  as being needed for the indicated join (or for final output if
 *	  where_needed includes "relation 0").
 *
 *	  The list may also contain PlaceHolderVars.  These don't necessarily
 *	  have a single owning relation; we keep their attr_needed info in
 *	  root->placeholder_list instead.  If create_new_ph is true, it's OK
 *	  to create new PlaceHolderInfos; otherwise, the PlaceHolderInfos must
 *	  already exist, and we should only update their ph_needed.  (This
 *should be true before deconstruct_jointree begins, and false after that.)
 */
void
add_vars_to_targetlist(PlannerInfo *root, List *vars, Relids where_needed, bool create_new_ph)
{









































}

/*****************************************************************************
 *
 *	  LATERAL REFERENCES
 *
 *****************************************************************************/

/*
 * find_lateral_references
 *	  For each LATERAL subquery, extract all its references to Vars and
 *	  PlaceHolderVars of the current query level, and make sure those values
 *	  will be available for evaluation of the subquery.
 *
 * While later planning steps ensure that the Var/PHV source rels are on the
 * outside of nestloops relative to the LATERAL subquery, we also need to
 * ensure that the Vars/PHVs propagate up to the nestloop join level; this
 * means setting suitable where_needed values for them.
 *
 * Note that this only deals with lateral references in unflattened LATERAL
 * subqueries.  When we flatten a LATERAL subquery, its lateral references
 * become plain Vars in the parent query, but they may have to be wrapped in
 * PlaceHolderVars if they need to be forced NULL by outer joins that don't
 * also null the LATERAL subquery.  That's all handled elsewhere.
 *
 * This has to run before deconstruct_jointree, since it might result in
 * creation of PlaceHolderInfos.
 */
void
find_lateral_references(PlannerInfo *root)
{

















































}

static void
extract_lateral_references(PlannerInfo *root, RelOptInfo *brel, Index rtindex)
{









































































































}

/*
 * create_lateral_join_info
 *	  Fill in the per-base-relation direct_lateral_relids, lateral_relids
 *	  and lateral_referencers sets.
 *
 * This has to run after deconstruct_jointree, because we need to know the
 * final ph_eval_at values for PlaceHolderVars.
 */
void
create_lateral_join_info(PlannerInfo *root)
{



















































































































































































































}

/*****************************************************************************
 *
 *	  JOIN TREE PROCESSING
 *
 *****************************************************************************/

/*
 * deconstruct_jointree
 *	  Recursively scan the query's join tree for WHERE and JOIN/ON qual
 *	  clauses, and add these to the appropriate restrictinfo and joininfo
 *	  lists belonging to base RelOptInfos.  Also, add SpecialJoinInfo nodes
 *	  to root->join_info_list for any outer joins appearing in the query
 *tree. Return a "joinlist" data structure showing the join order decisions that
 *need to be made by make_one_rel().
 *
 * The "joinlist" result is a list of items that are either RangeTblRef
 * jointree nodes or sub-joinlists.  All the items at the same level of
 * joinlist must be joined in an order to be determined by make_one_rel()
 * (note that legal orders may be constrained by SpecialJoinInfo nodes).
 * A sub-joinlist represents a subproblem to be planned separately. Currently
 * sub-joinlists arise only from FULL OUTER JOIN or when collapsing of
 * subproblems is stopped by join_collapse_limit or from_collapse_limit.
 *
 * NOTE: when dealing with inner joins, it is appropriate to let a qual clause
 * be evaluated at the lowest level where all the variables it mentions are
 * available.  However, we cannot push a qual down into the nullable side(s)
 * of an outer join since the qual might eliminate matching rows and cause a
 * NULL row to be incorrectly emitted by the join.  Therefore, we artificially
 * OR the minimum-relids of such an outer join into the required_relids of
 * clauses appearing above it.  This forces those clauses to be delayed until
 * application of the outer join (or maybe even higher in the join tree).
 */
List *
deconstruct_jointree(PlannerInfo *root)
{

















}

/*
 * deconstruct_recurse
 *	  One recursion level of deconstruct_jointree processing.
 *
 * Inputs:
 *	jtnode is the jointree node to examine
 *	below_outer_join is true if this node is within the nullable side of a
 *		higher-level outer join
 * Outputs:
 *	*qualscope gets the set of base Relids syntactically included in this
 *		jointree node (do not modify or free this, as it may also be
 *pointed to by RestrictInfo and SpecialJoinInfo nodes) *inner_join_rels gets
 *the set of base Relids syntactically included in inner joins appearing at or
 *below this jointree node (do not modify or free this, either)
 *	*postponed_qual_list is a list of PostponedQual structs, which we can
 *		add quals to if they turn out to belong to a higher join level
 *	Return value is the appropriate joinlist for this jointree node
 *
 * In addition, entries will be added to root->join_info_list for outer joins.
 */
static List *
deconstruct_recurse(PlannerInfo *root, Node *jtnode, bool below_outer_join, Relids *qualscope, Relids *inner_join_rels, List **postponed_qual_list)
{








































































































































































































































































































}

/*
 * process_security_barrier_quals
 *	  Transfer security-barrier quals into relation's baserestrictinfo list.
 *
 * The rewriter put any relevant security-barrier conditions into the RTE's
 * securityQuals field, but it's now time to copy them into the rel's
 * baserestrictinfo.
 *
 * In inheritance cases, we only consider quals attached to the parent rel
 * here; they will be valid for all children too, so it's okay to consider
 * them for purposes like equivalence class creation.  Quals attached to
 * individual child rels will be dealt with during path creation.
 */
static void
process_security_barrier_quals(PlannerInfo *root, int rti, Relids qualscope, bool below_outer_join)
{
































}

/*
 * make_outerjoininfo
 *	  Build a SpecialJoinInfo for the current outer join
 *
 * Inputs:
 *	left_rels: the base Relids syntactically on outer side of join
 *	right_rels: the base Relids syntactically on inner side of join
 *	inner_join_rels: base Relids participating in inner joins below this one
 *	jointype: what it says (must always be LEFT, FULL, SEMI, or ANTI)
 *	clause: the outer join's join condition (in implicit-AND format)
 *
 * The node should eventually be appended to root->join_info_list, but we
 * do not do that here.
 *
 * Note: we assume that this function is invoked bottom-up, so that
 * root->join_info_list already contains entries for all outer joins that are
 * syntactically below this one.
 */
static SpecialJoinInfo *
make_outerjoininfo(PlannerInfo *root, Relids left_rels, Relids right_rels, Relids inner_join_rels, JoinType jointype, List *clause)
{
























































































































































































































}

/*
 * compute_semijoin_info
 *	  Fill semijoin-related fields of a new SpecialJoinInfo
 *
 * Note: this relies on only the jointype and syn_righthand fields of the
 * SpecialJoinInfo; the rest may not be set yet.
 */
static void
compute_semijoin_info(PlannerInfo *root, SpecialJoinInfo *sjinfo, List *clause)
{














































































































































































}

/*****************************************************************************
 *
 *	  QUALIFICATIONS
 *
 *****************************************************************************/

/*
 * distribute_qual_to_rels
 *	  Add clause information to either the baserestrictinfo or joininfo list
 *	  (depending on whether the clause is a join) of each base relation
 *	  mentioned in the clause.  A RestrictInfo node is created and added to
 *	  the appropriate list for each rel.  Alternatively, if the clause uses
 *a mergejoinable operator and is not delayed by outer-join rules, enter the
 *left- and right-side expressions into the query's list of EquivalenceClasses.
 *Alternatively, if the clause needs to be treated as belonging to a higher join
 *level, just add it to postponed_qual_list.
 *
 * 'clause': the qual clause to be distributed
 * 'is_deduced': true if the qual came from implied-equality deduction
 * 'below_outer_join': true if the qual is from a JOIN/ON that is below the
 *		nullable side of a higher-level outer join
 * 'jointype': type of join the qual is from (JOIN_INNER for a WHERE clause)
 * 'security_level': security_level to assign to the qual
 * 'qualscope': set of baserels the qual's syntactic scope covers
 * 'ojscope': NULL if not an outer-join qual, else the minimum set of baserels
 *		needed to form this join
 * 'outerjoin_nonnullable': NULL if not an outer-join qual, else the set of
 *		baserels appearing on the outer (nonnullable) side of the join
 *		(for FULL JOIN this includes both sides of the join, and must in
 *fact equal qualscope) 'deduced_nullable_relids': if is_deduced is true, the
 *nullable relids to impute to the clause; otherwise NULL 'postponed_qual_list':
 *list of PostponedQual structs, which we can add this qual to if it turns out
 *to belong to a higher join level. Can be NULL if caller knows postponement is
 *impossible.
 *
 * 'qualscope' identifies what level of JOIN the qual came from syntactically.
 * 'ojscope' is needed if we decide to force the qual up to the outer-join
 * level, which will be ojscope not necessarily qualscope.
 *
 * In normal use (when is_deduced is false), at the time this is called,
 * root->join_info_list must contain entries for all and only those special
 * joins that are syntactically below this qual.  But when is_deduced is true,
 * we are adding new deduced clauses after completion of deconstruct_jointree,
 * so it cannot be assumed that root->join_info_list has anything to do with
 * qual placement.
 */
static void
distribute_qual_to_rels(PlannerInfo *root, Node *clause, bool is_deduced, bool below_outer_join, JoinType jointype, Index security_level, Relids qualscope, Relids ojscope, Relids outerjoin_nonnullable, Relids deduced_nullable_relids, List **postponed_qual_list)
{































































































































































































































































































































































}

/*
 * check_outerjoin_delay
 *		Detect whether a qual referencing the given relids must be
 *delayed in application due to the presence of a lower outer join, and/or may
 *force extra delay of higher-level outer joins.
 *
 * If the qual must be delayed, add relids to *relids_p to reflect the lowest
 * safe level for evaluating the qual, and return true.  Any extra delay for
 * higher-level joins is reflected by setting delay_upper_joins to true in
 * SpecialJoinInfo structs.  We also compute nullable_relids, the set of
 * referenced relids that are nullable by lower outer joins (note that this
 * can be nonempty even for a non-delayed qual).
 *
 * For an is_pushed_down qual, we can evaluate the qual as soon as (1) we have
 * all the rels it mentions, and (2) we are at or above any outer joins that
 * can null any of these rels and are below the syntactic location of the
 * given qual.  We must enforce (2) because pushing down such a clause below
 * the OJ might cause the OJ to emit null-extended rows that should not have
 * been formed, or that should have been rejected by the clause.  (This is
 * only an issue for non-strict quals, since if we can prove a qual mentioning
 * only nullable rels is strict, we'd have reduced the outer join to an inner
 * join in reduce_outer_joins().)
 *
 * To enforce (2), scan the join_info_list and merge the required-relid sets of
 * any such OJs into the clause's own reference list.  At the time we are
 * called, the join_info_list contains only outer joins below this qual.  We
 * have to repeat the scan until no new relids get added; this ensures that
 * the qual is suitably delayed regardless of the order in which OJs get
 * executed.  As an example, if we have one OJ with LHS=A, RHS=B, and one with
 * LHS=B, RHS=C, it is implied that these can be done in either order; if the
 * B/C join is done first then the join to A can null C, so a qual actually
 * mentioning only C cannot be applied below the join to A.
 *
 * For a non-pushed-down qual, this isn't going to determine where we place the
 * qual, but we need to determine outerjoin_delayed and nullable_relids anyway
 * for use later in the planning process.
 *
 * Lastly, a pushed-down qual that references the nullable side of any current
 * join_info_list member and has to be evaluated above that OJ (because its
 * required relids overlap the LHS too) causes that OJ's delay_upper_joins
 * flag to be set true.  This will prevent any higher-level OJs from
 * being interchanged with that OJ, which would result in not having any
 * correct place to evaluate the qual.  (The case we care about here is a
 * sub-select WHERE clause within the RHS of some outer join.  The WHERE
 * clause must effectively be treated as a degenerate clause of that outer
 * join's condition.  Rather than trying to match such clauses with joins
 * directly, we set delay_upper_joins here, and when the upper outer join
 * is processed by make_outerjoininfo, it will refrain from allowing the
 * two OJs to commute.)
 */
static bool
check_outerjoin_delay(PlannerInfo *root, Relids *relids_p, /* in/out parameter */
    Relids *nullable_relids_p,                             /* output parameter */
    bool is_pushed_down)
{





























































}

/*
 * check_equivalence_delay
 *		Detect whether a potential equivalence clause is rendered unsafe
 *		by outer-join-delay considerations.  Return true if it's safe.
 *
 * The initial tests in distribute_qual_to_rels will consider a mergejoinable
 * clause to be a potential equivalence clause if it is not outerjoin_delayed.
 * But since the point of equivalence processing is that we will recombine the
 * two sides of the clause with others, we have to check that each side
 * satisfies the not-outerjoin_delayed condition on its own; otherwise it might
 * not be safe to evaluate everywhere we could place a derived equivalence
 * condition.
 */
static bool
check_equivalence_delay(PlannerInfo *root, RestrictInfo *restrictinfo)
{

























}

/*
 * check_redundant_nullability_qual
 *	  Check to see if the qual is an IS NULL qual that is redundant with
 *	  a lower JOIN_ANTI join.
 *
 * We want to suppress redundant IS NULL quals, not so much to save cycles
 * as to avoid generating bogus selectivity estimates for them.  So if
 * redundancy is detected here, distribute_qual_to_rels() just throws away
 * the qual.
 */
static bool
check_redundant_nullability_qual(PlannerInfo *root, Node *clause)
{



























}

/*
 * distribute_restrictinfo_to_rels
 *	  Push a completed RestrictInfo into the proper restriction or join
 *	  clause list(s).
 *
 * This is the last step of distribute_qual_to_rels() for ordinary qual
 * clauses.  Clauses that are interesting for equivalence-class processing
 * are diverted to the EC machinery, but may ultimately get fed back here.
 */
void
distribute_restrictinfo_to_rels(PlannerInfo *root, RestrictInfo *restrictinfo)
{













































}

/*
 * process_implied_equality
 *	  Create a restrictinfo item that says "item1 op item2", and push it
 *	  into the appropriate lists.  (In practice opno is always a btree
 *	  equality operator.)
 *
 * "qualscope" is the nominal syntactic level to impute to the restrictinfo.
 * This must contain at least all the rels used in the expressions, but it
 * is used only to set the qual application level when both exprs are
 * variable-free.  Otherwise the qual is applied at the lowest join level
 * that provides all its variables.
 *
 * "nullable_relids" is the set of relids used in the expressions that are
 * potentially nullable below the expressions.  (This has to be supplied by
 * caller because this function is used after deconstruct_jointree, so we
 * don't have knowledge of where the clause items came from.)
 *
 * "security_level" is the security level to assign to the new restrictinfo.
 *
 * "both_const" indicates whether both items are known pseudo-constant;
 * in this case it is worth applying eval_const_expressions() in case we
 * can produce constant TRUE or constant FALSE.  (Otherwise it's not,
 * because the expressions went through eval_const_expressions already.)
 *
 * Note: this function will copy item1 and item2, but it is caller's
 * responsibility to make sure that the Relids parameters are fresh copies
 * not shared with other uses.
 *
 * This is currently used only when an EquivalenceClass is found to
 * contain pseudoconstants.  See path/pathkeys.c for more details.
 */
void
process_implied_equality(PlannerInfo *root, Oid opno, Oid collation, Expr *item1, Expr *item2, Relids qualscope, Relids nullable_relids, Index security_level, bool below_outer_join, bool both_const)
{
































}

/*
 * build_implied_join_equality --- build a RestrictInfo for a derived equality
 *
 * This overlaps the functionality of process_implied_equality(), but we
 * must return the RestrictInfo, not push it into the joininfo tree.
 *
 * Note: this function will copy item1 and item2, but it is caller's
 * responsibility to make sure that the Relids parameters are fresh copies
 * not shared with other uses.
 *
 * Note: we do not do initialize_mergeclause_eclasses() here.  It is
 * caller's responsibility that left_ec/right_ec be set as necessary.
 */
RestrictInfo *
build_implied_join_equality(PlannerInfo *root, Oid opno, Oid collation, Expr *item1, Expr *item2, Relids qualscope, Relids nullable_relids, Index security_level)
{



























}

/*
 * match_foreign_keys_to_quals
 *		Match foreign-key constraints to equivalence classes and join
 *quals
 *
 * The idea here is to see which query join conditions match equality
 * constraints of a foreign-key relationship.  For such join conditions,
 * we can use the FK semantics to make selectivity estimates that are more
 * reliable than estimating from statistics, especially for multiple-column
 * FKs, where the normal assumption of independent conditions tends to fail.
 *
 * In this function we annotate the ForeignKeyOptInfos in root->fkey_list
 * with info about which eclasses and join qual clauses they match, and
 * discard any ForeignKeyOptInfos that are irrelevant for the query.
 */
void
match_foreign_keys_to_quals(PlannerInfo *root)
{
































































































































































}

/*****************************************************************************
 *
 *	 CHECKS FOR MERGEJOINABLE AND HASHJOINABLE CLAUSES
 *
 *****************************************************************************/

/*
 * check_mergejoinable
 *	  If the restrictinfo's clause is mergejoinable, set the mergejoin
 *	  info fields in the restrictinfo.
 *
 *	  Currently, we support mergejoin for binary opclauses where
 *	  the operator is a mergejoinable operator.  The arguments can be
 *	  anything --- as long as there are no volatile functions in them.
 */
static void
check_mergejoinable(RestrictInfo *restrictinfo)
{






























}

/*
 * check_hashjoinable
 *	  If the restrictinfo's clause is hashjoinable, set the hashjoin
 *	  info fields in the restrictinfo.
 *
 *	  Currently, we support hashjoin for binary opclauses where
 *	  the operator is a hashjoinable operator.  The arguments can be
 *	  anything --- as long as there are no volatile functions in them.
 */
static void
check_hashjoinable(RestrictInfo *restrictinfo)
{
























}