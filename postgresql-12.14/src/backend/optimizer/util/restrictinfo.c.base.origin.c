/*-------------------------------------------------------------------------
 *
 * restrictinfo.c
 *	  RestrictInfo node manipulation routines.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/restrictinfo.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/restrictinfo.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)

static RestrictInfo *
make_restrictinfo_internal(PlannerInfo *root, Expr *clause, Expr *orclause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids);
static Expr *
make_sub_restrictinfos(PlannerInfo *root, Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids);

/*
 * make_restrictinfo
 *
 * Build a RestrictInfo node containing the given subexpression.
 *
 * The is_pushed_down, outerjoin_delayed, and pseudoconstant flags for the
 * RestrictInfo must be supplied by the caller, as well as the correct values
 * for security_level, outer_relids, and nullable_relids.
 * required_relids can be NULL, in which case it defaults to the actual clause
 * contents (i.e., clause_relids).
 *
 * We initialize fields that depend only on the given subexpression, leaving
 * others that depend on context (or may never be needed at all) to be filled
 * later.
 */
RestrictInfo *
make_restrictinfo(Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{

}

RestrictInfo *
make_restrictinfo_new(PlannerInfo *root, Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{













}

/*
 * make_restrictinfo_internal
 *
 * Common code for the main entry points and the recursive cases.
 */
static RestrictInfo *
make_restrictinfo_internal(PlannerInfo *root, Expr *clause, Expr *orclause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{




































































































}

/*
 * Recursively insert sub-RestrictInfo nodes into a boolean expression.
 *
 * We put RestrictInfos above simple (non-AND/OR) clauses and above
 * sub-OR clauses, but not above sub-AND clauses, because there's no need.
 * This may seem odd but it is closely related to the fact that we use
 * implicit-AND lists at top level of RestrictInfo lists.  Only ORs and
 * simple clauses are valid RestrictInfos.
 *
 * The same is_pushed_down, outerjoin_delayed, and pseudoconstant flag
 * values can be applied to all RestrictInfo nodes in the result.  Likewise
 * for security_level, outer_relids, and nullable_relids.
 *
 * The given required_relids are attached to our top-level output,
 * but any OR-clause constituents are allowed to default to just the
 * contained rels.
 */
static Expr *
make_sub_restrictinfos(PlannerInfo *root, Expr *clause, bool is_pushed_down, bool outerjoin_delayed, bool pseudoconstant, Index security_level, Relids required_relids, Relids outer_relids, Relids nullable_relids)
{


























}

/*
 * commute_restrictinfo
 *
 * Given a RestrictInfo containing a binary opclause, produce a RestrictInfo
 * representing the commutation of that clause.  The caller must pass the
 * OID of the commutator operator (which it's presumably looked up, else
 * it would not know this is valid).
 *
 * Beware that the result shares sub-structure with the given RestrictInfo.
 * That's okay for the intended usage with derived index quals, but might
 * be hazardous if the source is subject to change.  Also notice that we
 * assume without checking that the commutator op is a member of the same
 * btree and hash opclasses as the original op.
 */
RestrictInfo *
commute_restrictinfo(RestrictInfo *rinfo, Oid comm_op)
{
















































}

/*
 * restriction_is_or_clause
 *
 * Returns t iff the restrictinfo node contains an 'or' clause.
 */
bool
restriction_is_or_clause(RestrictInfo *restrictinfo)
{








}

/*
 * restriction_is_securely_promotable
 *
 * Returns true if it's okay to evaluate this clause "early", that is before
 * other restriction clauses attached to the specified relation.
 */
bool
restriction_is_securely_promotable(RestrictInfo *restrictinfo, RelOptInfo *rel)
{












}

/*
 * get_actual_clauses
 *
 * Returns a list containing the bare clauses from 'restrictinfo_list'.
 *
 * This is only to be used in cases where none of the RestrictInfos can
 * be pseudoconstant clauses (for instance, it's OK on indexqual lists).
 */
List *
get_actual_clauses(List *restrictinfo_list)
{












}

/*
 * extract_actual_clauses
 *
 * Extract bare clauses from 'restrictinfo_list', returning either the
 * regular ones or the pseudoconstant ones per 'pseudoconstant'.
 */
List *
extract_actual_clauses(List *restrictinfo_list, bool pseudoconstant)
{













}

/*
 * extract_actual_join_clauses
 *
 * Extract bare clauses from 'restrictinfo_list', separating those that
 * semantically match the join level from those that were pushed down.
 * Pseudoconstant clauses are excluded from the results.
 *
 * This is only used at outer joins, since for plain joins we don't care
 * about pushed-down-ness.
 */
void
extract_actual_join_clauses(List *restrictinfo_list, Relids joinrelids, List **joinquals, List **otherquals)
{























}

/*
 * join_clause_is_movable_to
 *		Test whether a join clause is a safe candidate for
 *parameterization of a scan on the specified base relation.
 *
 * A movable join clause is one that can safely be evaluated at a rel below
 * its normal semantic level (ie, its required_relids), if the values of
 * variables that it would need from other rels are provided.
 *
 * We insist that the clause actually reference the target relation; this
 * prevents undesirable movement of degenerate join clauses, and ensures
 * that there is a unique place that a clause can be moved down to.
 *
 * We cannot move an outer-join clause into the non-nullable side of its
 * outer join, as that would change the results (rows would be suppressed
 * rather than being null-extended).
 *
 * Also there must not be an outer join below the clause that would null the
 * Vars coming from the target relation.  Otherwise the clause might give
 * results different from what it would give at its normal semantic level.
 *
 * Also, the join clause must not use any relations that have LATERAL
 * references to the target relation, since we could not put such rels on
 * the outer side of a nestloop with the target relation.
 */
bool
join_clause_is_movable_to(RestrictInfo *rinfo, RelOptInfo *baserel)
{

























}

/*
 * join_clause_is_movable_into
 *		Test whether a join clause is movable and can be evaluated
 *within the current join context.
 *
 * currentrelids: the relids of the proposed evaluation location
 * current_and_outer: the union of currentrelids and the required_outer
 *		relids (parameterization's outer relations)
 *
 * The API would be a bit clearer if we passed the current relids and the
 * outer relids separately and did bms_union internally; but since most
 * callers need to apply this function to multiple clauses, we make the
 * caller perform the union.
 *
 * Obviously, the clause must only refer to Vars available from the current
 * relation plus the outer rels.  We also check that it does reference at
 * least one current Var, ensuring that the clause will be pushed down to
 * a unique place in a parameterized join tree.  And we check that we're
 * not pushing the clause into its outer-join outer side, nor down into
 * a lower outer join's inner side.
 *
 * The check about pushing a clause down into a lower outer join's inner side
 * is only approximate; it sometimes returns "false" when actually it would
 * be safe to use the clause here because we're still above the outer join
 * in question.  This is okay as long as the answers at different join levels
 * are consistent: it just means we might sometimes fail to push a clause as
 * far down as it could safely be pushed.  It's unclear whether it would be
 * worthwhile to do this more precisely.  (But if it's ever fixed to be
 * exactly accurate, there's an Assert in get_joinrel_parampathinfo() that
 * should be re-enabled.)
 *
 * There's no check here equivalent to join_clause_is_movable_to's test on
 * lateral_referencers.  We assume the caller wouldn't be inquiring unless
 * it'd verified that the proposed outer rels don't have lateral references
 * to the current rel(s).  (If we are considering join paths with the outer
 * rels on the outside and the current rels on the inside, then this should
 * have been checked at the outset of such consideration; see join_is_legal
 * and the path parameterization checks in joinpath.c.)  On the other hand,
 * in join_clause_is_movable_to we are asking whether the clause could be
 * moved for some valid set of outer rels, so we don't have the benefit of
 * relying on prior checks for lateral-reference validity.
 *
 * Note: if this returns true, it means that the clause could be moved to
 * this join relation, but that doesn't mean that this is the lowest join
 * it could be moved to.  Caller may need to make additional calls to verify
 * that this doesn't succeed on either of the inputs of a proposed join.
 *
 * Note: get_joinrel_parampathinfo depends on the fact that if
 * current_and_outer is NULL, this function will always return false
 * (since one or the other of the first two tests must fail).
 */
bool
join_clause_is_movable_into(RestrictInfo *rinfo, Relids currentrelids, Relids current_and_outer)
{






























}