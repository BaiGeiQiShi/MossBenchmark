/*-------------------------------------------------------------------------
 *
 * joinrels.c
 *	  Routines to determine which relations should be joined
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/joinrels.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "optimizer/appendinfo.h"
#include "optimizer/joininfo.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "partitioning/partbounds.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

static void
make_rels_by_clause_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels);
static void
make_rels_by_clauseless_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels);
static bool
has_join_restriction(PlannerInfo *root, RelOptInfo *rel);
static bool
has_legal_joinclause(PlannerInfo *root, RelOptInfo *rel);
static bool
restriction_is_constant_false(List *restrictlist, RelOptInfo *joinrel, bool only_pushed_down);
static void
populate_joinrel_with_paths(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *sjinfo, List *restrictlist);
static void
try_partitionwise_join(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *parent_sjinfo, List *parent_restrictlist);
static SpecialJoinInfo *
build_child_join_sjinfo(PlannerInfo *root, SpecialJoinInfo *parent_sjinfo, Relids left_relids, Relids right_relids);
static int
match_expr_to_partition_keys(Expr *expr, RelOptInfo *rel, bool strict_op);

/*
 * join_search_one_level
 *	  Consider ways to produce join relations containing exactly 'level'
 *	  jointree items.  (This is one step of the dynamic-programming method
 *	  embodied in standard_join_search.)  Join rel nodes for each feasible
 *	  combination of lower-level rels are created and returned in a list.
 *	  Implementation paths are created for each such joinrel, too.
 *
 * level: level of rels we want to make this time
 * root->join_rel_level[j], 1 <= j < level, is a list of rels containing j items
 *
 * The result is returned in root->join_rel_level[level].
 */
void
join_search_one_level(PlannerInfo *root, int level)
{



























































































































































































}

/*
 * make_rels_by_clause_joins
 *	  Build joins between the given relation 'old_rel' and other relations
 *	  that participate in join clauses that 'old_rel' also participates in
 *	  (or participate in join-order restrictions with it).
 *	  The join rels are returned in root->join_rel_level[join_cur_level].
 *
 * Note: at levels above 2 we will generate the same joined relation in
 * multiple ways --- for example (a join b) join c is the same RelOptInfo as
 * (b join c) join a, though the second case will add a different set of Paths
 * to it.  This is the reason for using the join_rel_level mechanism, which
 * automatically ensures that each new joinrel is only added to the list once.
 *
 * 'old_rel' is the relation entry for the relation to be joined
 * 'other_rels': the first cell in a linked list containing the other
 * rels to be considered for joining
 *
 * Currently, this is only used with initial rels in other_rels, but it
 * will work for joining to joinrels too.
 */
static void
make_rels_by_clause_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels)
{











}

/*
 * make_rels_by_clauseless_joins
 *	  Given a relation 'old_rel' and a list of other relations
 *	  'other_rels', create a join relation between 'old_rel' and each
 *	  member of 'other_rels' that isn't already included in 'old_rel'.
 *	  The join rels are returned in root->join_rel_level[join_cur_level].
 *
 * 'old_rel' is the relation entry for the relation to be joined
 * 'other_rels': the first cell of a linked list containing the
 * other rels to be considered for joining
 *
 * Currently, this is only used with initial rels in other_rels, but it would
 * work for joining to joinrels too.
 */
static void
make_rels_by_clauseless_joins(PlannerInfo *root, RelOptInfo *old_rel, ListCell *other_rels)
{











}

/*
 * join_is_legal
 *	   Determine whether a proposed join is legal given the query's
 *	   join order constraints; and if it is, determine the join type.
 *
 * Caller must supply not only the two rels, but the union of their relids.
 * (We could simplify the API by computing joinrelids locally, but this
 * would be redundant work in the normal path through make_join_rel.)
 *
 * On success, *sjinfo_p is set to NULL if this is to be a plain inner join,
 * else it's set to point to the associated SpecialJoinInfo node.  Also,
 * *reversed_p is set true if the given relations need to be swapped to
 * match the SpecialJoinInfo node.
 */
static bool
join_is_legal(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, Relids joinrelids, SpecialJoinInfo **sjinfo_p, bool *reversed_p)
{
































































































































































































































































































































}

/*
 * make_join_rel
 *	   Find or create a join RelOptInfo that represents the join of
 *	   the two given rels, and add to it path information for paths
 *	   created with the two rels as outer and inner rel.
 *	   (The join rel may already contain paths generated from other
 *	   pairs of rels that add up to the same set of base rels.)
 *
 * NB: will return NULL if attempted join is not valid.  This can happen
 * when working with outer joins, or with IN or EXISTS clauses that have been
 * turned into joins.
 */
RelOptInfo *
make_join_rel(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{











































































}

/*
 * populate_joinrel_with_paths
 *	  Add paths to the given joinrel for given pair of joining relations.
 *The SpecialJoinInfo provides details about the join and the restrictlist
 *	  contains the join clauses and the other clauses applicable for given
 *pair of the joining relations.
 */
static void
populate_joinrel_with_paths(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *sjinfo, List *restrictlist)
{
























































































































}

/*
 * have_join_order_restriction
 *		Detect whether the two relations should be joined to satisfy
 *		a join-order restriction arising from special or lateral joins.
 *
 * In practice this is always used with have_relevant_joinclause(), and so
 * could be merged with that function, but it seems clearer to separate the
 * two concerns.  We need this test because there are degenerate cases where
 * a clauseless join must be performed to satisfy join-order restrictions.
 * Also, if one rel has a lateral reference to the other, or both are needed
 * to compute some PHV, we should consider joining them even if the join would
 * be clauseless.
 *
 * Note: this is only a problem if one side of a degenerate outer join
 * contains multiple rels, or a clauseless join is required within an
 * IN/EXISTS RHS; else we will find a join path via the "last ditch" case in
 * join_search_one_level().  We could dispense with this test if we were
 * willing to try bushy plans in the "last ditch" case, but that seems much
 * less efficient.
 */
bool
have_join_order_restriction(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{
































































































}

/*
 * has_join_restriction
 *		Detect whether the specified relation has join-order
 *restrictions, due to being inside an outer join or an IN (sub-SELECT), or
 *participating in any LATERAL references or multi-rel PHVs.
 *
 * Essentially, this tests whether have_join_order_restriction() could
 * succeed with this rel and some other one.  It's OK if we sometimes
 * say "true" incorrectly.  (Therefore, we don't bother with the relatively
 * expensive has_legal_joinclause test.)
 */
static bool
has_join_restriction(PlannerInfo *root, RelOptInfo *rel)
{









































}

/*
 * has_legal_joinclause
 *		Detect whether the specified relation can legally be joined
 *		to any other rels using join clauses.
 *
 * We consider only joins to single other relations in the current
 * initial_rels list.  This is sufficient to get a "true" result in most real
 * queries, and an occasional erroneous "false" will only cost a bit more
 * planning time.  The reason for this limitation is that considering joins to
 * other joins would require proving that the other join rel can legally be
 * formed, which seems like too much trouble for something that's only a
 * heuristic to save planning time.  (Note: we must look at initial_rels
 * and not all of the query, since when we are planning a sub-joinlist we
 * may be forced to make clauseless joins within initial_rels even though
 * there are join clauses linking to other parts of the query.)
 */
static bool
has_legal_joinclause(PlannerInfo *root, RelOptInfo *rel)
{

































}

/*
 * There's a pitfall for creating parameterized nestloops: suppose the inner
 * rel (call it A) has a parameter that is a PlaceHolderVar, and that PHV's
 * minimum eval_at set includes the outer rel (B) and some third rel (C).
 * We might think we could create a B/A nestloop join that's parameterized by
 * C.  But we would end up with a plan in which the PHV's expression has to be
 * evaluated as a nestloop parameter at the B/A join; and the executor is only
 * set up to handle simple Vars as NestLoopParams.  Rather than add complexity
 * and overhead to the executor for such corner cases, it seems better to
 * forbid the join.  (Note that we can still make use of A's parameterized
 * path with pre-joined B+C as the outer rel.  have_join_order_restriction()
 * ensures that we will consider making such a join even if there are not
 * other reasons to do so.)
 *
 * So we check whether any PHVs used in the query could pose such a hazard.
 * We don't have any simple way of checking whether a risky PHV would actually
 * be used in the inner plan, and the case is so unusual that it doesn't seem
 * worth working very hard on it.
 *
 * This needs to be checked in two places.  If the inner rel's minimum
 * parameterization would trigger the restriction, then join_is_legal() should
 * reject the join altogether, because there will be no workable paths for it.
 * But joinpath.c has to check again for every proposed nestloop path, because
 * the inner path might have more than the minimum parameterization, causing
 * some PHV to be dangerous for it that otherwise wouldn't be.
 */
bool
have_dangerous_phv(PlannerInfo *root, Relids outer_relids, Relids inner_params)
{
























}

/*
 * is_dummy_rel --- has relation been proven empty?
 */
bool
is_dummy_rel(RelOptInfo *rel)
{







































}

/*
 * Mark a relation as proven empty.
 *
 * During GEQO planning, this can get invoked more than once on the same
 * baserel struct, so it's worth checking to see if the rel is already marked
 * dummy.
 *
 * Also, when called during GEQO join planning, we are in a short-lived
 * memory context.  We must make sure that the dummy path attached to a
 * baserel survives the GEQO cycle, else the baserel is trashed for future
 * GEQO cycles.  On the other hand, when we are marking a joinrel during GEQO,
 * we don't want the dummy path to clutter the main planning context.  Upshot
 * is that the best solution is to explicitly make the dummy path in the same
 * context the given RelOptInfo is in.
 */
void
mark_dummy_rel(RelOptInfo *rel)
{

























}

/*
 * restriction_is_constant_false --- is a restrictlist just FALSE?
 *
 * In cases where a qual is provably constant FALSE, eval_const_expressions
 * will generally have thrown away anything that's ANDed with it.  In outer
 * join situations this will leave us computing cartesian products only to
 * decide there's no match for an outer row, which is pretty stupid.  So,
 * we need to detect the case.
 *
 * If only_pushed_down is true, then consider only quals that are pushed-down
 * from the point of view of the joinrel.
 */
static bool
restriction_is_constant_false(List *restrictlist, RelOptInfo *joinrel, bool only_pushed_down)
{

































}

/*
 * Assess whether join between given two partitioned relations can be broken
 * down into joins between matching partitions; a technique called
 * "partitionwise join"
 *
 * Partitionwise join is possible when a. Joining relations have same
 * partitioning scheme b. There exists an equi-join between the partition keys
 * of the two relations.
 *
 * Partitionwise join is planned as follows (details: optimizer/README.)
 *
 * 1. Create the RelOptInfos for joins between matching partitions i.e
 * child-joins and add paths to them.
 *
 * 2. Construct Append or MergeAppend paths across the set of child joins.
 * This second phase is implemented by generate_partitionwise_join_paths().
 *
 * The RelOptInfo, SpecialJoinInfo and restrictlist for each child join are
 * obtained by translating the respective parent join structures.
 */
static void
try_partitionwise_join(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2, RelOptInfo *joinrel, SpecialJoinInfo *parent_sjinfo, List *parent_restrictlist)
{


































































































































































}

/*
 * Construct the SpecialJoinInfo for a child-join by translating
 * SpecialJoinInfo for the join between parents. left_relids and right_relids
 * are the relids of left and right side of the join respectively.
 */
static SpecialJoinInfo *
build_child_join_sjinfo(PlannerInfo *root, SpecialJoinInfo *parent_sjinfo, Relids left_relids, Relids right_relids)
{




















}

/*
 * Returns true if there exists an equi-join condition for each pair of
 * partition keys from given relations being joined.
 */
bool
have_partkey_equi_join(RelOptInfo *joinrel, RelOptInfo *rel1, RelOptInfo *rel2, JoinType jointype, List *restrictlist)
{

























































































































}

/*
 * Find the partition key from the given relation matching the given
 * expression. If found, return the index of the partition key, else return -1.
 */
static int
match_expr_to_partition_keys(Expr *expr, RelOptInfo *rel, bool strict_op)
{















































}