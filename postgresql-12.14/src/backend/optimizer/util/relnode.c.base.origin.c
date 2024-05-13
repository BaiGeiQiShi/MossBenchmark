/*-------------------------------------------------------------------------
 *
 * relnode.c
 *	  Relation-node lookup/construction routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/relnode.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "miscadmin.h"
#include "optimizer/appendinfo.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/inherit.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/plancat.h"
#include "optimizer/restrictinfo.h"
#include "optimizer/tlist.h"
#include "partitioning/partbounds.h"
#include "utils/hsearch.h"

typedef struct JoinHashEntry
{
  Relids join_relids; /* hash key --- MUST BE FIRST */
  RelOptInfo *join_rel;
} JoinHashEntry;

static void
build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *input_rel);
static List *
build_joinrel_restrictlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
static void
build_joinrel_joinlist(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
static List *
subbuild_joinrel_restrictlist(RelOptInfo *joinrel, List *joininfo_list, List *new_restrictlist);
static List *
subbuild_joinrel_joinlist(RelOptInfo *joinrel, List *joininfo_list, List *new_joininfo);
static void
set_foreign_rel_properties(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel);
static void
add_join_rel(PlannerInfo *root, RelOptInfo *joinrel);
static void
build_joinrel_partition_info(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel, List *restrictlist, JoinType jointype);
static void
build_child_join_reltarget(PlannerInfo *root, RelOptInfo *parentrel, RelOptInfo *childrel, int nappinfos, AppendRelInfo **appinfos);

/*
 * setup_simple_rel_arrays
 *	  Prepare the arrays we use for quickly accessing base relations.
 */
void
setup_simple_rel_arrays(PlannerInfo *root)
{


















}

/*
 * setup_append_rel_array
 *		Populate the append_rel_array to allow direct lookups of
 *		AppendRelInfos by child relid.
 *
 * The array remains unallocated if there are no AppendRelInfos.
 */
void
setup_append_rel_array(PlannerInfo *root)
{


























}

/*
 * expand_planner_arrays
 *		Expand the PlannerInfo's per-RTE arrays by add_size members
 *		and initialize the newly added entries to NULLs
 */
void
expand_planner_arrays(PlannerInfo *root, int add_size)
{























}

/*
 * build_simple_rel
 *	  Construct a new RelOptInfo for a base relation or 'other' relation.
 */
RelOptInfo *
build_simple_rel(PlannerInfo *root, int relid, RelOptInfo *parent)
{











































































































































































}

/*
 * find_base_rel
 *	  Find a base or other relation entry, which must already exist.
 */
RelOptInfo *
find_base_rel(PlannerInfo *root, int relid)
{
















}

/*
 * build_join_rel_hash
 *	  Construct the auxiliary hash table for join relations.
 */
static void
build_join_rel_hash(PlannerInfo *root)
{


























}

/*
 * find_join_rel
 *	  Returns relation entry corresponding to 'relids' (a set of RT
 *indexes), or NULL if none exists.  This is for join relations.
 */
RelOptInfo *
find_join_rel(PlannerInfo *root, Relids relids)
{












































}

/*
 * set_foreign_rel_properties
 *		Set up foreign-join fields if outer and inner relation are
 *foreign tables (or joins) belonging to the same server and assigned to the
 *same user to check access permissions as.
 *
 * In addition to an exact match of userid, we allow the case where one side
 * has zero userid (implying current user) and the other side has explicit
 * userid that happens to equal the current user; but in that case, pushdown of
 * the join is only valid for the current user.  The useridiscurrent field
 * records whether we had to make such an assumption for this join or any
 * sub-join.
 *
 * Otherwise these fields are left invalid, so GetForeignJoinPaths will not be
 * called for the join relation.
 *
 */
static void
set_foreign_rel_properties(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
























}

/*
 * add_join_rel
 *		Add given join relation to the list of join relations in the
 *given PlannerInfo. Also add it to the auxiliary hashtable if there is one.
 */
static void
add_join_rel(PlannerInfo *root, RelOptInfo *joinrel)
{













}

/*
 * build_join_rel
 *	  Returns relation entry corresponding to the union of two given rels,
 *	  creating a new relation entry if none already exists.
 *
 * 'joinrelids' is the Relids set that uniquely identifies the join
 * 'outer_rel' and 'inner_rel' are relation nodes for the relations to be
 *		joined
 * 'sjinfo': join context info
 * 'restrictlist_ptr': result variable.  If not NULL, *restrictlist_ptr
 *		receives the list of RestrictInfo nodes that apply to this
 *		particular pair of joinable relations.
 *
 * restrictlist_ptr makes the routine's API a little grotty, but it saves
 * duplicated calculation of the restrictlist...
 */
RelOptInfo *
build_join_rel(PlannerInfo *root, Relids joinrelids, RelOptInfo *outer_rel, RelOptInfo *inner_rel, SpecialJoinInfo *sjinfo, List **restrictlist_ptr)
{
















































































































































































}

/*
 * build_child_join_rel
 *	  Builds RelOptInfo representing join between given two child relations.
 *
 * 'outer_rel' and 'inner_rel' are the RelOptInfos of child relations being
 *		joined
 * 'parent_joinrel' is the RelOptInfo representing the join between parent
 *		relations. Some of the members of new RelOptInfo are produced by
 *		translating corresponding members of this RelOptInfo
 * 'sjinfo': child-join context info
 * 'restrictlist': list of RestrictInfo nodes that apply to this particular
 *		pair of joinable relations
 * 'jointype' is the join type (inner, left, full, etc)
 */
RelOptInfo *
build_child_join_rel(PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel, RelOptInfo *parent_joinrel, List *restrictlist, SpecialJoinInfo *sjinfo, JoinType jointype)
{

























































































































}

/*
 * min_join_parameterization
 *
 * Determine the minimum possible parameterization of a joinrel, that is, the
 * set of other rels it contains LATERAL references to.  We save this value in
 * the join's RelOptInfo.  This function is split out of build_join_rel()
 * because join_is_legal() needs the value to check a prospective join.
 */
Relids
min_join_parameterization(PlannerInfo *root, Relids joinrelids, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{

























}

/*
 * build_joinrel_tlist
 *	  Builds a join relation's target list from an input relation.
 *	  (This is invoked twice to handle the two input relations.)
 *
 * The join's targetlist includes all Vars of its member relations that
 * will still be needed above the join.  This subroutine adds all such
 * Vars from the specified input rel's tlist to the join rel's tlist.
 *
 * We also compute the expected width of the join's output, making use
 * of data that was cached at the baserel level by set_rel_width().
 */
static void
build_joinrel_tlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *input_rel)
{









































}

/*
 * build_joinrel_restrictlist
 * build_joinrel_joinlist
 *	  These routines build lists of restriction and join clauses for a
 *	  join relation from the joininfo lists of the relations it joins.
 *
 *	  These routines are separate because the restriction list must be
 *	  built afresh for each pair of input sub-relations we consider, whereas
 *	  the join list need only be computed once for any join RelOptInfo.
 *	  The join list is fully determined by the set of rels making up the
 *	  joinrel, so we should get the same results (up to ordering) from any
 *	  candidate pair of sub-relations.  But the restriction list is whatever
 *	  is not handled in the sub-relations, so it depends on which
 *	  sub-relations are considered.
 *
 *	  If a join clause from an input relation refers to base rels still not
 *	  present in the joinrel, then it is still a join clause for the
 *joinrel; we put it into the joininfo list for the joinrel.  Otherwise, the
 *clause is now a restrict clause for the joined relation, and we return it to
 *the caller of build_joinrel_restrictlist() to be stored in join paths made
 *from this pair of sub-relations.  (It will not need to be considered further
 *up the join tree.)
 *
 *	  In many cases we will find the same RestrictInfos in both input
 *	  relations' joinlists, so be careful to eliminate duplicates.
 *	  Pointer equality should be a sufficient test for dups, since all
 *	  the various joinlist entries ultimately refer to RestrictInfos
 *	  pushed into them by distribute_restrictinfo_to_rels().
 *
 * 'joinrel' is a join relation node
 * 'outer_rel' and 'inner_rel' are a pair of relations that can be joined
 *		to form joinrel.
 *
 * build_joinrel_restrictlist() returns a list of relevant restrictinfos,
 * whereas build_joinrel_joinlist() stores its results in the joinrel's
 * joininfo list.  One or the other must accept each given clause!
 *
 * NB: Formerly, we made deep(!) copies of each input RestrictInfo to pass
 * up to the join relation.  I believe this is no longer necessary, because
 * RestrictInfo nodes are no longer context-dependent.  Instead, just include
 * the original nodes in the lists made for the join relation.
 */
static List *
build_joinrel_restrictlist(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{


















}

static void
build_joinrel_joinlist(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{











}

static List *
subbuild_joinrel_restrictlist(RelOptInfo *joinrel, List *joininfo_list, List *new_restrictlist)
{



























}

static List *
subbuild_joinrel_joinlist(RelOptInfo *joinrel, List *joininfo_list, List *new_joininfo)
{































}

/*
 * fetch_upper_rel
 *		Build a RelOptInfo describing some post-scan/join query
 *processing, or return a pre-existing one if somebody already built it.
 *
 * An "upper" relation is identified by an UpperRelationKind and a Relids set.
 * The meaning of the Relids set is not specified here, and very likely will
 * vary for different relation kinds.
 *
 * Most of the fields in an upper-level RelOptInfo are not used and are not
 * set here (though makeNode should ensure they're zeroes).  We basically only
 * care about fields that are of interest to add_path() and set_cheapest().
 */
RelOptInfo *
fetch_upper_rel(PlannerInfo *root, UpperRelationKind kind, Relids relids)
{







































}

/*
 * find_childrel_parents
 *		Compute the set of parent relids of an appendrel child rel.
 *
 * Since appendrels can be nested, a child could have multiple levels of
 * appendrel ancestors.  This function computes a Relids set of all the
 * parent relation IDs.
 */
Relids
find_childrel_parents(PlannerInfo *root, RelOptInfo *rel)
{



















}

/*
 * get_baserel_parampathinfo
 *		Get the ParamPathInfo for a parameterized path for a base
 *relation, constructing one if we don't have one already.
 *
 * This centralizes estimating the rowcounts for parameterized paths.
 * We need to cache those to be sure we use the same rowcount for all paths
 * of the same parameterization for a given rel.  This is also a convenient
 * place to determine which movable join clauses the parameterized path will
 * be responsible for evaluating.
 */
ParamPathInfo *
get_baserel_parampathinfo(PlannerInfo *root, RelOptInfo *baserel, Relids required_outer)
{
























































}

/*
 * get_joinrel_parampathinfo
 *		Get the ParamPathInfo for a parameterized path for a join
 *relation, constructing one if we don't have one already.
 *
 * This centralizes estimating the rowcounts for parameterized paths.
 * We need to cache those to be sure we use the same rowcount for all paths
 * of the same parameterization for a given rel.  This is also a convenient
 * place to determine which movable join clauses the parameterized path will
 * be responsible for evaluating.
 *
 * outer_path and inner_path are a pair of input paths that can be used to
 * construct the join, and restrict_clauses is the list of regular join
 * clauses (including clauses derived from EquivalenceClasses) that must be
 * applied at the join node when using these inputs.
 *
 * Unlike the situation for base rels, the set of movable join clauses to be
 * enforced at a join varies with the selected pair of input paths, so we
 * must calculate that and pass it back, even if we already have a matching
 * ParamPathInfo.  We handle this by adding any clauses moved down to this
 * join to *restrict_clauses, which is an in/out parameter.  (The addition
 * is done in such a way as to not modify the passed-in List structure.)
 *
 * Note: when considering a nestloop join, the caller must have removed from
 * restrict_clauses any movable clauses that are themselves scheduled to be
 * pushed into the right-hand path.  We do not do that here since it's
 * unnecessary for other join types.
 */
ParamPathInfo *
get_joinrel_parampathinfo(PlannerInfo *root, RelOptInfo *joinrel, Path *outer_path, Path *inner_path, SpecialJoinInfo *sjinfo, Relids required_outer, List **restrict_clauses)
{












































































































































































}

/*
 * get_appendrel_parampathinfo
 *		Get the ParamPathInfo for a parameterized path for an append
 *relation.
 *
 * For an append relation, the rowcount estimate will just be the sum of
 * the estimates for its children.  However, we still need a ParamPathInfo
 * to flag the fact that the path requires parameters.  So this just creates
 * a suitable struct with zero ppi_rows (and no ppi_clauses either, since
 * the Append node isn't responsible for checking quals).
 */
ParamPathInfo *
get_appendrel_parampathinfo(RelOptInfo *appendrel, Relids required_outer)
{



























}

/*
 * Returns a ParamPathInfo for the parameterization given by required_outer, if
 * already available in the given rel. Returns NULL otherwise.
 */
ParamPathInfo *
find_param_path_info(RelOptInfo *rel, Relids required_outer)
{













}

/*
 * build_joinrel_partition_info
 *		If the two relations have same partitioning scheme, their join
 *may be partitioned and will follow the same partitioning scheme as the joining
 *		relations. Set the partition scheme and partition key
 *expressions in the join relation.
 */
static void
build_joinrel_partition_info(RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel, List *restrictlist, JoinType jointype)
{













































































































































}

/*
 * build_child_join_reltarget
 *	  Set up a child-join relation's reltarget from a parent-join relation.
 */
static void
build_child_join_reltarget(PlannerInfo *root, RelOptInfo *parentrel, RelOptInfo *childrel, int nappinfos, AppendRelInfo **appinfos)
{







}