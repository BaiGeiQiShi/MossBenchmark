/*-------------------------------------------------------------------------
 *
 * orclauses.c
 *	  Routines to extract restriction OR clauses from join OR clauses
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/orclauses.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/orclauses.h"
#include "optimizer/restrictinfo.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define make_restrictinfo(a, b, c, d, e, f, g, h, i) make_restrictinfo_new(a, b, c, d, e, f, g, h, i)

static bool
is_safe_restriction_clause_for(RestrictInfo *rinfo, RelOptInfo *rel);
static Expr *
extract_or_clause(RestrictInfo *or_rinfo, RelOptInfo *rel);
static void
consider_new_or_clause(PlannerInfo *root, RelOptInfo *rel, Expr *orclause, RestrictInfo *join_or_rinfo);

/*
 * extract_restriction_or_clauses
 *	  Examine join OR-of-AND clauses to see if any useful restriction OR
 *	  clauses can be extracted.  If so, add them to the query.
 *
 * Although a join clause must reference multiple relations overall,
 * an OR of ANDs clause might contain sub-clauses that reference just one
 * relation and can be used to build a restriction clause for that rel.
 * For example consider
 *		WHERE ((a.x = 42 AND b.y = 43) OR (a.x = 44 AND b.z = 45));
 * We can transform this into
 *		WHERE ((a.x = 42 AND b.y = 43) OR (a.x = 44 AND b.z = 45))
 *			AND (a.x = 42 OR a.x = 44)
 *			AND (b.y = 43 OR b.z = 45);
 * which allows the latter clauses to be applied during the scans of a and b,
 * perhaps as index qualifications, and in any case reducing the number of
 * rows arriving at the join.  In essence this is a partial transformation to
 * CNF (AND of ORs format).  It is not complete, however, because we do not
 * unravel the original OR --- doing so would usually bloat the qualification
 * expression to little gain.
 *
 * The added quals are partially redundant with the original OR, and therefore
 * would cause the size of the joinrel to be underestimated when it is finally
 * formed.  (This would be true of a full transformation to CNF as well; the
 * fault is not really in the transformation, but in clauselist_selectivity's
 * inability to recognize redundant conditions.)  We can compensate for this
 * redundancy by changing the cached selectivity of the original OR clause,
 * canceling out the (valid) reduction in the estimated sizes of the base
 * relations so that the estimated joinrel size remains the same.  This is
 * a MAJOR HACK: it depends on the fact that clause selectivities are cached
 * and on the fact that the same RestrictInfo node will appear in every
 * joininfo list that might be used when the joinrel is formed.
 * And it doesn't work in cases where the size estimation is nonlinear
 * (i.e., outer and IN joins).  But it beats not doing anything.
 *
 * We examine each base relation to see if join clauses associated with it
 * contain extractable restriction conditions.  If so, add those conditions
 * to the rel's baserestrictinfo and update the cached selectivities of the
 * join clauses.  Note that the same join clause will be examined afresh
 * from the point of view of each baserel that participates in it, so its
 * cached selectivity may get updated multiple times.
 */
void
extract_restriction_or_clauses(PlannerInfo *root)
{




















































}

/*
 * Is the given primitive (non-OR) RestrictInfo safe to move to the rel?
 */
static bool
is_safe_restriction_clause_for(RestrictInfo *rinfo, RelOptInfo *rel)
{





















}

/*
 * Try to extract a restriction clause mentioning only "rel" from the given
 * join OR-clause.
 *
 * We must be able to extract at least one qual for this rel from each of
 * the arms of the OR, else we can't use it.
 *
 * Returns an OR clause (not a RestrictInfo!) pertaining to rel, or NULL
 * if no OR clause could be extracted.
 */
static Expr *
extract_or_clause(RestrictInfo *or_rinfo, RelOptInfo *rel)
{





































































































}

/*
 * Consider whether a successfully-extracted restriction OR clause is
 * actually worth using.  If so, add it to the planner's data structures,
 * and adjust the original join clause (join_or_rinfo) to compensate.
 */
static void
consider_new_or_clause(PlannerInfo *root, RelOptInfo *rel, Expr *orclause, RestrictInfo *join_or_rinfo)
{
























































































}