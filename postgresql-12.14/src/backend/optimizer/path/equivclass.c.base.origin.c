/*-------------------------------------------------------------------------
 *
 * equivclass.c
 *	  Routines for managing EquivalenceClasses
 *
 * See src/backend/optimizer/README for discussion of EquivalenceClasses.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/equivclass.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/stratnum.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "utils/lsyscache.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

static EquivalenceMember *
add_eq_member(EquivalenceClass *ec, Expr *expr, Relids relids, Relids nullable_relids, bool is_child, Oid datatype);
static void
generate_base_implied_equalities_const(PlannerInfo *root, EquivalenceClass *ec);
static void
generate_base_implied_equalities_no_const(PlannerInfo *root, EquivalenceClass *ec);
static void
generate_base_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec);
static List *
generate_join_implied_equalities_normal(PlannerInfo *root, EquivalenceClass *ec, Relids join_relids, Relids outer_relids, Relids inner_relids);
static List *
generate_join_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec, Relids nominal_join_relids, Relids outer_relids, Relids nominal_inner_relids, RelOptInfo *inner_rel);
static Oid
select_equality_operator(EquivalenceClass *ec, Oid lefttype, Oid righttype);
static RestrictInfo *
create_join_clause(PlannerInfo *root, EquivalenceClass *ec, Oid opno, EquivalenceMember *leftem, EquivalenceMember *rightem, EquivalenceClass *parent_ec);
static bool
reconsider_outer_join_clause(PlannerInfo *root, RestrictInfo *rinfo, bool outer_on_left);
static bool
reconsider_full_join_clause(PlannerInfo *root, RestrictInfo *rinfo);

/*
 * process_equivalence
 *	  The given clause has a mergejoinable operator and can be applied
 *without any delay by an outer join, so its two sides can be considered equal
 *	  anywhere they are both computable; moreover that equality can be
 *	  extended transitively.  Record this knowledge in the EquivalenceClass
 *	  data structure, if applicable.  Returns true if successful, false if
 *not (in which case caller should treat the clause as ordinary, not an
 *	  equivalence).
 *
 * In some cases, although we cannot convert a clause into EquivalenceClass
 * knowledge, we can still modify it to a more useful form than the original.
 * Then, *p_restrictinfo will be replaced by a new RestrictInfo, which is what
 * the caller should use for further processing.
 *
 * If below_outer_join is true, then the clause was found below the nullable
 * side of an outer join, so its sides might validly be both NULL rather than
 * strictly equal.  We can still deduce equalities in such cases, but we take
 * care to mark an EquivalenceClass if it came from any such clauses.  Also,
 * we have to check that both sides are either pseudo-constants or strict
 * functions of Vars, else they might not both go to NULL above the outer
 * join.  (This is the main reason why we need a failure return.  It's more
 * convenient to check this case here than at the call sites...)
 *
 * We also reject proposed equivalence clauses if they contain leaky functions
 * and have security_level above zero.  The EC evaluation rules require us to
 * apply certain tests at certain joining levels, and we can't tolerate
 * delaying any test on security_level grounds.  By rejecting candidate clauses
 * that might require security delays, we ensure it's safe to apply an EC
 * clause as soon as it's supposed to be applied.
 *
 * On success return, we have also initialized the clause's left_ec/right_ec
 * fields to point to the EquivalenceClass representing it.  This saves lookup
 * effort later.
 *
 * Note: constructing merged EquivalenceClasses is a standard UNION-FIND
 * problem, for which there exist better data structures than simple lists.
 * If this code ever proves to be a bottleneck then it could be sped up ---
 * but for now, simple is beautiful.
 *
 * Note: this is only called during planner startup, not during GEQO
 * exploration, so we need not worry about whether we're in the right
 * memory context.
 */
bool
process_equivalence(PlannerInfo *root, RestrictInfo **p_restrictinfo, bool below_outer_join)
{
































































































































































































































































































































}

/*
 * canonicalize_ec_expression
 *
 * This function ensures that the expression exposes the expected type and
 * collation, so that it will be equal() to other equivalence-class expressions
 * that it ought to be equal() to.
 *
 * The rule for datatypes is that the exposed type should match what it would
 * be for an input to an operator of the EC's opfamilies; which is usually
 * the declared input type of the operator, but in the case of polymorphic
 * operators no relabeling is wanted (compare the behavior of parse_coerce.c).
 * Expressions coming in from quals will generally have the right type
 * already, but expressions coming from indexkeys may not (because they are
 * represented without any explicit relabel in pg_index), and the same problem
 * occurs for sort expressions (because the parser is likewise cavalier about
 * putting relabels on them).  Such cases will be binary-compatible with the
 * real operators, so adding a RelabelType is sufficient.
 *
 * Also, the expression's exposed collation must match the EC's collation.
 * This is important because in comparisons like "foo < bar COLLATE baz",
 * only one of the expressions has the correct exposed collation as we receive
 * it from the parser.  Forcing both of them to have it ensures that all
 * variant spellings of such a construct behave the same.  Again, we can
 * stick on a RelabelType to force the right exposed collation.  (It might
 * work to not label the collation at all in EC members, but this is risky
 * since some parts of the system expect exprCollation() to deliver the
 * right answer for a sort key.)
 */
Expr *
canonicalize_ec_expression(Expr *expr, Oid req_type, Oid req_collation)
{








































}

/*
 * add_eq_member - build a new EquivalenceMember and add it to an EC
 */
static EquivalenceMember *
add_eq_member(EquivalenceClass *ec, Expr *expr, Relids relids, Relids nullable_relids, bool is_child, Oid datatype)
{































}

/*
 * get_eclass_for_sort_expr
 *	  Given an expression and opfamily/collation info, find an existing
 *	  equivalence class it is a member of; if none, optionally build a new
 *	  single-member EquivalenceClass for it.
 *
 * expr is the expression, and nullable_relids is the set of base relids
 * that are potentially nullable below it.  We actually only care about
 * the set of such relids that are used in the expression; but for caller
 * convenience, we perform that intersection step here.  The caller need
 * only be sure that nullable_relids doesn't omit any nullable rels that
 * might appear in the expr.
 *
 * sortref is the SortGroupRef of the originating SortGroupClause, if any,
 * or zero if not.  (It should never be zero if the expression is volatile!)
 *
 * If rel is not NULL, it identifies a specific relation we're considering
 * a path for, and indicates that child EC members for that relation can be
 * considered.  Otherwise child members are ignored.  (Note: since child EC
 * members aren't guaranteed unique, a non-NULL value means that there could
 * be more than one EC that matches the expression; if so it's order-dependent
 * which one you get.  This is annoying but it only happens in corner cases,
 * so for now we live with just reporting the first match.  See also
 * generate_implied_equalities_for_column and match_pathkeys_to_index.)
 *
 * If create_it is true, we'll build a new EquivalenceClass when there is no
 * match.  If create_it is false, we just return NULL when no match.
 *
 * This can be used safely both before and after EquivalenceClass merging;
 * since it never causes merging it does not invalidate any existing ECs
 * or PathKeys.  However, ECs added after path generation has begun are
 * of limited usefulness, so usually it's best to create them beforehand.
 *
 * Note: opfamilies must be chosen consistently with the way
 * process_equivalence() would do; that is, generated from a mergejoinable
 * equality operator.  Else we might fail to detect valid equivalences,
 * generating poor (but not incorrect) plans.
 */
EquivalenceClass *
get_eclass_for_sort_expr(PlannerInfo *root, Expr *expr, Relids nullable_relids, List *opfamilies, Oid opcintype, Oid collation, Index sortref, Relids rel, bool create_it)
{































































































































}

/*
 * generate_base_implied_equalities
 *	  Generate any restriction clauses that we can deduce from equivalence
 *	  classes.
 *
 * When an EC contains pseudoconstants, our strategy is to generate
 * "member = const1" clauses where const1 is the first constant member, for
 * every other member (including other constants).  If we are able to do this
 * then we don't need any "var = var" comparisons because we've successfully
 * constrained all the vars at their points of creation.  If we fail to
 * generate any of these clauses due to lack of cross-type operators, we fall
 * back to the "ec_broken" strategy described below.  (XXX if there are
 * multiple constants of different types, it's possible that we might succeed
 * in forming all the required clauses if we started from a different const
 * member; but this seems a sufficiently hokey corner case to not be worth
 * spending lots of cycles on.)
 *
 * For ECs that contain no pseudoconstants, we generate derived clauses
 * "member1 = member2" for each pair of members belonging to the same base
 * relation (actually, if there are more than two for the same base relation,
 * we only need enough clauses to link each to each other).  This provides
 * the base case for the recursion: each row emitted by a base relation scan
 * will constrain all computable members of the EC to be equal.  As each
 * join path is formed, we'll add additional derived clauses on-the-fly
 * to maintain this invariant (see generate_join_implied_equalities).
 *
 * If the opfamilies used by the EC do not provide complete sets of cross-type
 * equality operators, it is possible that we will fail to generate a clause
 * that must be generated to maintain the invariant.  (An example: given
 * "WHERE a.x = b.y AND b.y = a.z", the scheme breaks down if we cannot
 * generate "a.x = a.z" as a restriction clause for A.)  In this case we mark
 * the EC "ec_broken" and fall back to regurgitating its original source
 * RestrictInfos at appropriate times.  We do not try to retract any derived
 * clauses already generated from the broken EC, so the resulting plan could
 * be poor due to bad selectivity estimates caused by redundant clauses.  But
 * the correct solution to that is to fix the opfamilies ...
 *
 * Equality clauses derived by this function are passed off to
 * process_implied_equality (in plan/initsplan.c) to be inserted into the
 * restrictinfo datastructures.  Note that this must be called after initial
 * scanning of the quals and before Path construction begins.
 *
 * We make no attempt to avoid generating duplicate RestrictInfos here: we
 * don't search ec_sources for matches, nor put the created RestrictInfos
 * into ec_derives.  Doing so would require some slightly ugly changes in
 * initsplan.c's API, and there's no real advantage, because the clauses
 * generated here can't duplicate anything we will generate for joins anyway.
 */
void
generate_base_implied_equalities(PlannerInfo *root)
{















































}

/*
 * generate_base_implied_equalities when EC contains pseudoconstant(s)
 */
static void
generate_base_implied_equalities_const(PlannerInfo *root, EquivalenceClass *ec)
{






























































}

/*
 * generate_base_implied_equalities when EC contains no pseudoconstants
 */
static void
generate_base_implied_equalities_no_const(PlannerInfo *root, EquivalenceClass *ec)
{




























































}

/*
 * generate_base_implied_equalities cleanup after failure
 *
 * What we must do here is push any zero- or one-relation source RestrictInfos
 * of the EC back into the main restrictinfo datastructures.  Multi-relation
 * clauses will be regurgitated later by generate_join_implied_equalities().
 * (We do it this way to maintain continuity with the case that ec_broken
 * becomes set only after we've gone up a join level or two.)  However, for
 * an EC that contains constants, we can adopt a simpler strategy and just
 * throw back all the source RestrictInfos immediately; that works because
 * we know that such an EC can't become broken later.  (This rule justifies
 * ignoring ec_has_const ECs in generate_join_implied_equalities, even when
 * they are broken.)
 */
static void
generate_base_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec)
{











}

/*
 * generate_join_implied_equalities
 *	  Generate any join clauses that we can deduce from equivalence classes.
 *
 * At a join node, we must enforce restriction clauses sufficient to ensure
 * that all equivalence-class members computable at that node are equal.
 * Since the set of clauses to enforce can vary depending on which subset
 * relations are the inputs, we have to compute this afresh for each join
 * relation pair.  Hence a fresh List of RestrictInfo nodes is built and
 * passed back on each call.
 *
 * In addition to its use at join nodes, this can be applied to generate
 * eclass-based join clauses for use in a parameterized scan of a base rel.
 * The reason for the asymmetry of specifying the inner rel as a RelOptInfo
 * and the outer rel by Relids is that this usage occurs before we have
 * built any join RelOptInfos.
 *
 * An annoying special case for parameterized scans is that the inner rel can
 * be an appendrel child (an "other rel").  In this case we must generate
 * appropriate clauses using child EC members.  add_child_rel_equivalences
 * must already have been done for the child rel.
 *
 * The results are sufficient for use in merge, hash, and plain nestloop join
 * methods.  We do not worry here about selecting clauses that are optimal
 * for use in a parameterized indexscan.  indxpath.c makes its own selections
 * of clauses to use, and if the ones we pick here are redundant with those,
 * the extras will be eliminated at createplan time, using the parent_ec
 * markers that we provide (see is_redundant_derived_clause()).
 *
 * Because the same join clauses are likely to be needed multiple times as
 * we consider different join paths, we avoid generating multiple copies:
 * whenever we select a particular pair of EquivalenceMembers to join,
 * we check to see if the pair matches any original clause (in ec_sources)
 * or previously-built clause (in ec_derives).  This saves memory and allows
 * re-use of information cached in RestrictInfos.
 *
 * join_relids should always equal bms_union(outer_relids, inner_rel->relids).
 * We could simplify this function's API by computing it internally, but in
 * most current uses, the caller has the value at hand anyway.
 */
List *
generate_join_implied_equalities(PlannerInfo *root, Relids join_relids, Relids outer_relids, RelOptInfo *inner_rel)
{

}

/*
 * generate_join_implied_equalities_for_ecs
 *	  As above, but consider only the listed ECs.
 */
List *
generate_join_implied_equalities_for_ecs(PlannerInfo *root, List *eclasses, Relids join_relids, Relids outer_relids, RelOptInfo *inner_rel)
{




























































}

/*
 * generate_join_implied_equalities for a still-valid EC
 */
static List *
generate_join_implied_equalities_normal(PlannerInfo *root, EquivalenceClass *ec, Relids join_relids, Relids outer_relids, Relids inner_relids)
{








































































































































































}

/*
 * generate_join_implied_equalities cleanup after failure
 *
 * Return any original RestrictInfos that are enforceable at this join.
 *
 * In the case of a child inner relation, we have to translate the
 * original RestrictInfos from parent to child Vars.
 */
static List *
generate_join_implied_equalities_broken(PlannerInfo *root, EquivalenceClass *ec, Relids nominal_join_relids, Relids outer_relids, Relids nominal_inner_relids, RelOptInfo *inner_rel)
{































}

/*
 * select_equality_operator
 *	  Select a suitable equality operator for comparing two EC members
 *
 * Returns InvalidOid if no operator can be found for this datatype combination
 */
static Oid
select_equality_operator(EquivalenceClass *ec, Oid lefttype, Oid righttype)
{
























}

/*
 * create_join_clause
 *	  Find or make a RestrictInfo comparing the two given EC members
 *	  with the given operator.
 *
 * parent_ec is either equal to ec (if the clause is a potentially-redundant
 * join clause) or NULL (if not).  We have to treat this as part of the
 * match requirements --- it's possible that a clause comparing the same two
 * EMs is a join clause in one join path and a restriction clause in another.
 */
static RestrictInfo *
create_join_clause(PlannerInfo *root, EquivalenceClass *ec, Oid opno, EquivalenceMember *leftem, EquivalenceMember *rightem, EquivalenceClass *parent_ec)
{























































}

/*
 * reconsider_outer_join_clauses
 *	  Re-examine any outer-join clauses that were set aside by
 *	  distribute_qual_to_rels(), and see if we can derive any
 *	  EquivalenceClasses from them.  Then, if they were not made
 *	  redundant, push them out into the regular join-clause lists.
 *
 * When we have mergejoinable clauses A = B that are outer-join clauses,
 * we can't blindly combine them with other clauses A = C to deduce B = C,
 * since in fact the "equality" A = B won't necessarily hold above the
 * outer join (one of the variables might be NULL instead).  Nonetheless
 * there are cases where we can add qual clauses using transitivity.
 *
 * One case that we look for here is an outer-join clause OUTERVAR = INNERVAR
 * for which there is also an equivalence clause OUTERVAR = CONSTANT.
 * It is safe and useful to push a clause INNERVAR = CONSTANT into the
 * evaluation of the inner (nullable) relation, because any inner rows not
 * meeting this condition will not contribute to the outer-join result anyway.
 * (Any outer rows they could join to will be eliminated by the pushed-down
 * equivalence clause.)
 *
 * Note that the above rule does not work for full outer joins; nor is it
 * very interesting to consider cases where the generated equivalence clause
 * would involve relations outside the outer join, since such clauses couldn't
 * be pushed into the inner side's scan anyway.  So the restriction to
 * outervar = pseudoconstant is not really giving up anything.
 *
 * For full-join cases, we can only do something useful if it's a FULL JOIN
 * USING and a merged column has an equivalence MERGEDVAR = CONSTANT.
 * By the time it gets here, the merged column will look like
 *		COALESCE(LEFTVAR, RIGHTVAR)
 * and we will have a full-join clause LEFTVAR = RIGHTVAR that we can match
 * the COALESCE expression to. In this situation we can push LEFTVAR = CONSTANT
 * and RIGHTVAR = CONSTANT into the input relations, since any rows not
 * meeting these conditions cannot contribute to the join result.
 *
 * Again, there isn't any traction to be gained by trying to deal with
 * clauses comparing a mergedvar to a non-pseudoconstant.  So we can make
 * use of the EquivalenceClasses to search for matching variables that were
 * equivalenced to constants.  The interesting outer-join clauses were
 * accumulated for us by distribute_qual_to_rels.
 *
 * When we find one of these cases, we implement the changes we want by
 * generating a new equivalence clause INNERVAR = CONSTANT (or LEFTVAR, etc)
 * and pushing it into the EquivalenceClass structures.  This is because we
 * may already know that INNERVAR is equivalenced to some other var(s), and
 * we'd like the constant to propagate to them too.  Note that it would be
 * unsafe to merge any existing EC for INNERVAR with the OUTERVAR's EC ---
 * that could result in propagating constant restrictions from
 * INNERVAR to OUTERVAR, which would be very wrong.
 *
 * It's possible that the INNERVAR is also an OUTERVAR for some other
 * outer-join clause, in which case the process can be repeated.  So we repeat
 * looping over the lists of clauses until no further deductions can be made.
 * Whenever we do make a deduction, we remove the generating clause from the
 * lists, since we don't want to make the same deduction twice.
 *
 * If we don't find any match for a set-aside outer join clause, we must
 * throw it back into the regular joinclause processing by passing it to
 * distribute_restrictinfo_to_rels().  If we do generate a derived clause,
 * however, the outer-join clause is redundant.  We still throw it back,
 * because otherwise the join will be seen as a clauseless join and avoided
 * during join order searching; but we mark it as redundant to keep from
 * messing up the joinrel's size estimate.  (This behavior means that the
 * API for this routine is uselessly complex: we could have just put all
 * the clauses into the regular processing initially.  We keep it because
 * someday we might want to do something else, such as inserting "dummy"
 * joinclauses instead of real ones.)
 *
 * Outer join clauses that are marked outerjoin_delayed are special: this
 * condition means that one or both VARs might go to null due to a lower
 * outer join.  We can still push a constant through the clause, but only
 * if its operator is strict; and we *have to* throw the clause back into
 * regular joinclause processing.  By keeping the strict join clause,
 * we ensure that any null-extended rows that are mistakenly generated due
 * to suppressing rows not matching the constant will be rejected at the
 * upper outer join.  (This doesn't work for full-join clauses.)
 */
void
reconsider_outer_join_clauses(PlannerInfo *root)
{






































































































}

/*
 * reconsider_outer_join_clauses for a single LEFT/RIGHT JOIN clause
 *
 * Returns true if we were able to propagate a constant through the clause.
 */
static bool
reconsider_outer_join_clause(PlannerInfo *root, RestrictInfo *rinfo, bool outer_on_left)
{

























































































































}

/*
 * reconsider_outer_join_clauses for a single FULL JOIN clause
 *
 * Returns true if we were able to propagate a constant through the clause.
 */
static bool
reconsider_full_join_clause(PlannerInfo *root, RestrictInfo *rinfo)
{



























































































































































}

/*
 * exprs_known_equal
 *	  Detect whether two expressions are known equal due to equivalence
 *	  relationships.
 *
 * Actually, this only shows that the expressions are equal according
 * to some opfamily's notion of equality --- but we only use it for
 * selectivity estimation, so a fuzzy idea of equality is OK.
 *
 * Note: does not bother to check for "equal(item1, item2)"; caller must
 * check that case if it's possible to pass identical items.
 */
bool
exprs_known_equal(PlannerInfo *root, Node *item1, Node *item2)
{







































}

/*
 * match_eclasses_to_foreign_key_col
 *	  See whether a foreign key column match is proven by any eclass.
 *
 * If the referenced and referencing Vars of the fkey's colno'th column are
 * known equal due to any eclass, return that eclass; otherwise return NULL.
 * (In principle there might be more than one matching eclass if multiple
 * collations are involved, but since collation doesn't matter for equality,
 * we ignore that fine point here.)  This is much like exprs_known_equal,
 * except that we insist on the comparison operator matching the eclass, so
 * that the result is definite not approximate.
 */
EquivalenceClass *
match_eclasses_to_foreign_key_col(PlannerInfo *root, ForeignKeyOptInfo *fkinfo, int colno)
{




















































































}

/*
 * add_child_rel_equivalences
 *	  Search for EC members that reference the root parent of child_rel, and
 *	  add transformed members referencing the child_rel.
 *
 * Note that this function won't be called at all unless we have at least some
 * reason to believe that the EC members it generates will be useful.
 *
 * parent_rel and child_rel could be derived from appinfo, but since the
 * caller has already computed them, we might as well just pass them in.
 *
 * The passed-in AppendRelInfo is not used when the parent_rel is not a
 * top-level baserel, since it shows the mapping from the parent_rel but
 * we need to translate EC expressions that refer to the top-level parent.
 * Using it is faster than using adjust_appendrel_attrs_multilevel(), though,
 * so we prefer it when we can.
 */
void
add_child_rel_equivalences(PlannerInfo *root, AppendRelInfo *appinfo, RelOptInfo *parent_rel, RelOptInfo *child_rel)
{































































































}

/*
 * add_child_join_rel_equivalences
 *	  Like add_child_rel_equivalences(), but for joinrels
 *
 * Here we find the ECs relevant to the top parent joinrel and add transformed
 * member expressions that refer to this child joinrel.
 *
 * Note that this function won't be called at all unless we have at least some
 * reason to believe that the EC members it generates will be useful.
 */
void
add_child_join_rel_equivalences(PlannerInfo *root, int nappinfos, AppendRelInfo **appinfos, RelOptInfo *parent_joinrel, RelOptInfo *child_joinrel)
{

















































































































}

/*
 * generate_implied_equalities_for_column
 *	  Create EC-derived joinclauses usable with a specific column.
 *
 * This is used by indxpath.c to extract potentially indexable joinclauses
 * from ECs, and can be used by foreign data wrappers for similar purposes.
 * We assume that only expressions in Vars of a single table are of interest,
 * but the caller provides a callback function to identify exactly which
 * such expressions it would like to know about.
 *
 * We assume that any given table/index column could appear in only one EC.
 * (This should be true in all but the most pathological cases, and if it
 * isn't, we stop on the first match anyway.)  Therefore, what we return
 * is a redundant list of clauses equating the table/index column to each of
 * the other-relation values it is known to be equal to.  Any one of
 * these clauses can be used to create a parameterized path, and there
 * is no value in using more than one.  (But it *is* worthwhile to create
 * a separate parameterized path for each one, since that leads to different
 * join orders.)
 *
 * The caller can pass a Relids set of rels we aren't interested in joining
 * to, so as to save the work of creating useless clauses.
 */
List *
generate_implied_equalities_for_column(PlannerInfo *root, RelOptInfo *rel, ec_matches_callback_type callback, void *callback_arg, Relids prohibited_rels)
{
































































































































}

/*
 * have_relevant_eclass_joinclause
 *		Detect whether there is an EquivalenceClass that could produce
 *		a joinclause involving the two given relations.
 *
 * This is essentially a very cut-down version of
 * generate_join_implied_equalities().  Note it's OK to occasionally say "yes"
 * incorrectly.  Hence we don't bother with details like whether the lack of a
 * cross-type operator might prevent the clause from actually being generated.
 */
bool
have_relevant_eclass_joinclause(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{










































}

/*
 * has_relevant_eclass_joinclause
 *		Detect whether there is an EquivalenceClass that could produce
 *		a joinclause involving the given relation and anything else.
 *
 * This is the same as have_relevant_eclass_joinclause with the other rel
 * implicitly defined as "everything else in the query".
 */
bool
has_relevant_eclass_joinclause(PlannerInfo *root, RelOptInfo *rel1)
{


























}

/*
 * eclass_useful_for_merging
 *	  Detect whether the EC could produce any mergejoinable join clauses
 *	  against the specified relation.
 *
 * This is just a heuristic test and doesn't have to be exact; it's better
 * to say "yes" incorrectly than "no".  Hence we don't bother with details
 * like whether the lack of a cross-type operator might prevent the clause
 * from actually being generated.
 */
bool
eclass_useful_for_merging(PlannerInfo *root, EquivalenceClass *eclass, RelOptInfo *rel)
{






















































}

/*
 * is_redundant_derived_clause
 *		Test whether rinfo is derived from same EC as any clause in
 *clauselist; if so, it can be presumed to represent a condition that's
 *redundant with that member of the list.
 */
bool
is_redundant_derived_clause(RestrictInfo *rinfo, List *clauselist)
{




















}

/*
 * is_redundant_with_indexclauses
 *		Test whether rinfo is redundant with any clause in the
 *IndexClause list.  Here, for convenience, we test both simple identity and
 *		whether it is derived from the same EC as any member of the
 *list.
 */
bool
is_redundant_with_indexclauses(RestrictInfo *rinfo, List *indexclauses)
{
































}