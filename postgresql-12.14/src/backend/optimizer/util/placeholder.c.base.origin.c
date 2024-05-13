/*-------------------------------------------------------------------------
 *
 * placeholder.c
 *	  PlaceHolderVar and PlaceHolderInfo manipulation routines
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/placeholder.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/nodeFuncs.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/placeholder.h"
#include "optimizer/planmain.h"
#include "utils/lsyscache.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)

/* Local functions */
static void
find_placeholders_recurse(PlannerInfo *root, Node *jtnode);
static void
find_placeholders_in_expr(PlannerInfo *root, Node *expr);

/*
 * make_placeholder_expr
 *		Make a PlaceHolderVar for the given expression.
 *
 * phrels is the syntactic location (as a set of baserels) to attribute
 * to the expression.
 */
PlaceHolderVar *
make_placeholder_expr(PlannerInfo *root, Expr *expr, Relids phrels)
{








}

/*
 * find_placeholder_info
 *		Fetch the PlaceHolderInfo for the given PHV
 *
 * If the PlaceHolderInfo doesn't exist yet, create it if create_new_ph is
 * true, else throw an error.
 *
 * This is separate from make_placeholder_expr because subquery pullup has
 * to make PlaceHolderVars for expressions that might not be used at all in
 * the upper query, or might not remain after const-expression simplification.
 * We build PlaceHolderInfos only for PHVs that are still present in the
 * simplified query passed to query_planner().
 *
 * Note: this should only be called after query_planner() has started.  Also,
 * create_new_ph must not be true after deconstruct_jointree begins, because
 * make_outerjoininfo assumes that we already know about all placeholders.
 */
PlaceHolderInfo *
find_placeholder_info(PlannerInfo *root, PlaceHolderVar *phv, bool create_new_ph)
{





























































}

/*
 * find_placeholders_in_jointree
 *		Search the jointree for PlaceHolderVars, and build
 *PlaceHolderInfos
 *
 * We don't need to look at the targetlist because build_base_rel_tlists()
 * will already have made entries for any PHVs in the tlist.
 *
 * This is called before we begin deconstruct_jointree.  Once we begin
 * deconstruct_jointree, all active placeholders must be present in
 * root->placeholder_list, because make_outerjoininfo and
 * update_placeholder_eval_levels require this info to be available
 * while we crawl up the join tree.
 */
void
find_placeholders_in_jointree(PlannerInfo *root)
{







}

/*
 * find_placeholders_recurse
 *	  One recursion level of find_placeholders_in_jointree.
 *
 * jtnode is the current jointree node to examine.
 */
static void
find_placeholders_recurse(PlannerInfo *root, Node *jtnode)
{











































}

/*
 * find_placeholders_in_expr
 *		Find all PlaceHolderVars in the given expression, and create
 *		PlaceHolderInfo entries for them.
 */
static void
find_placeholders_in_expr(PlannerInfo *root, Node *expr)
{






















}

/*
 * update_placeholder_eval_levels
 *		Adjust the target evaluation levels for placeholders
 *
 * The initial eval_at level set by find_placeholder_info was the set of
 * rels used in the placeholder's expression (or the whole subselect below
 * the placeholder's syntactic location, if the expr is variable-free).
 * If the query contains any outer joins that can null any of those rels,
 * we must delay evaluation to above those joins.
 *
 * We repeat this operation each time we add another outer join to
 * root->join_info_list.  It's somewhat annoying to have to do that, but
 * since we don't have very much information on the placeholders' locations,
 * it's hard to avoid.  Each placeholder's eval_at level must be correct
 * by the time it starts to figure in outer-join delay decisions for higher
 * outer joins.
 *
 * In future we might want to put additional policy/heuristics here to
 * try to determine an optimal evaluation level.  The current rules will
 * result in evaluation at the lowest possible level.  However, pushing a
 * placeholder eval up the tree is likely to further constrain evaluation
 * order for outer joins, so it could easily be counterproductive; and we
 * don't have enough information at this point to make an intelligent choice.
 */
void
update_placeholder_eval_levels(PlannerInfo *root, SpecialJoinInfo *new_sjinfo)
{






























































}

/*
 * fix_placeholder_input_needed_levels
 *		Adjust the "needed at" levels for placeholder inputs
 *
 * This is called after we've finished determining the eval_at levels for
 * all placeholders.  We need to make sure that all vars and placeholders
 * needed to evaluate each placeholder will be available at the scan or join
 * level where the evaluation will be done.  (It might seem that scan-level
 * evaluations aren't interesting, but that's not so: a LATERAL reference
 * within a placeholder's expression needs to cause the referenced var or
 * placeholder to be marked as needed in the scan where it's evaluated.)
 * Note that this loop can have side-effects on the ph_needed sets of other
 * PlaceHolderInfos; that's okay because we don't examine ph_needed here, so
 * there are no ordering issues to worry about.
 */
void
fix_placeholder_input_needed_levels(PlannerInfo *root)
{










}

/*
 * add_placeholders_to_base_rels
 *		Add any required PlaceHolderVars to base rels' targetlists.
 *
 * If any placeholder can be computed at a base rel and is needed above it,
 * add it to that rel's targetlist.  This might look like it could be merged
 * with fix_placeholder_input_needed_levels, but it must be separate because
 * join removal happens in between, and can change the ph_eval_at sets.  There
 * is essentially the same logic in add_placeholders_to_joinrel, but we can't
 * do that part until joinrels are formed.
 */
void
add_placeholders_to_base_rels(PlannerInfo *root)
{
















}

/*
 * add_placeholders_to_joinrel
 *		Add any required PlaceHolderVars to a join rel's targetlist;
 *		and if they contain lateral references, add those references to
 *the joinrel's direct_lateral_relids.
 *
 * A join rel should emit a PlaceHolderVar if (a) the PHV can be computed
 * at or below this join level and (b) the PHV is needed above this level.
 * However, condition (a) is sufficient to add to direct_lateral_relids,
 * as explained below.
 */
void
add_placeholders_to_joinrel(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{
























































}