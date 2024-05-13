/*------------------------------------------------------------------------
 *
 * geqo_eval.c
 *	  Routines to evaluate query trees
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/optimizer/geqo/geqo_eval.c
 *
 *-------------------------------------------------------------------------
 */

/* contributed by:
   =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=
   *  Martin Utesch				 * Institute of Automatic
   Control	   * =							 =
   University of Mining and Technology =
   *  utesch@aut.tu-freiberg.de  * Freiberg, Germany *
   =*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=
 */

#include "postgres.h"

#include <float.h>
#include <limits.h>
#include <math.h>

#include "optimizer/geqo.h"
#include "optimizer/joininfo.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "utils/memutils.h"

/* A "clump" of already-joined relations within gimme_tree */
typedef struct
{
  RelOptInfo *joinrel; /* joinrel for the set of relations */
  int size;            /* number of input relations in clump */
} Clump;

static List *
merge_clump(PlannerInfo *root, List *clumps, Clump *new_clump, int num_gene, bool force);
static bool
desirable_join(PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel);

/*
 * geqo_eval
 *
 * Returns cost of a query tree as an individual of the population.
 *
 * If no legal join order can be extracted from the proposed tour,
 * returns DBL_MAX.
 */
Cost
geqo_eval(PlannerInfo *root, Gene *tour, int num_gene)
{









































































}

/*
 * gimme_tree
 *	  Form planner estimates for a join tree constructed in the specified
 *	  order.
 *
 *	 'tour' is the proposed join order, of length 'num_gene'
 *
 * Returns a new join relation whose cheapest path is the best plan for
 * this join order.  NB: will return NULL if join order is invalid and
 * we can't modify it into a valid order.
 *
 * The original implementation of this routine always joined in the specified
 * order, and so could only build left-sided plans (and right-sided and
 * mixtures, as a byproduct of the fact that make_join_rel() is symmetric).
 * It could never produce a "bushy" plan.  This had a couple of big problems,
 * of which the worst was that there are situations involving join order
 * restrictions where the only valid plans are bushy.
 *
 * The present implementation takes the given tour as a guideline, but
 * postpones joins that are illegal or seem unsuitable according to some
 * heuristic rules.  This allows correct bushy plans to be generated at need,
 * and as a nice side-effect it seems to materially improve the quality of the
 * generated plans.  Note however that since it's just a heuristic, it can
 * still fail in some cases.  (In particular, we might clump together
 * relations that actually mustn't be joined yet due to LATERAL restrictions;
 * since there's no provision for un-clumping, this must lead to failure.)
 */
RelOptInfo *
gimme_tree(PlannerInfo *root, Gene *tour, int num_gene)
{



























































}

/*
 * Merge a "clump" into the list of existing clumps for gimme_tree.
 *
 * We try to merge the clump into some existing clump, and repeat if
 * successful.  When no more merging is possible, insert the clump
 * into the list, preserving the list ordering rule (namely, that
 * clumps of larger size appear earlier).
 *
 * If force is true, merge anywhere a join is legal, even if it causes
 * a cartesian join to be performed.  When force is false, do only
 * "desirable" joins.
 */
static List *
merge_clump(PlannerInfo *root, List *clumps, Clump *new_clump, int num_gene, bool force)
{



























































































}

/*
 * Heuristics for gimme_tree: do we want to join these two relations?
 */
static bool
desirable_join(PlannerInfo *root, RelOptInfo *outer_rel, RelOptInfo *inner_rel)
{











}