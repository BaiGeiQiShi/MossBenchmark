/*-------------------------------------------------------------------------
 *
 * tidpath.c
 *	  Routines to determine which TID conditions are usable for scanning
 *	  a given relation, and create TidPaths accordingly.
 *
 * What we are looking for here is WHERE conditions of the form
 * "CTID = pseudoconstant", which can be implemented by just fetching
 * the tuple directly via heap_fetch().  We can also handle OR'd conditions
 * such as (CTID = const1) OR (CTID = const2), as well as ScalarArrayOpExpr
 * conditions of the form CTID = ANY(pseudoconstant_array).  In particular
 * this allows
 *		WHERE ctid IN (tid1, tid2, ...)
 *
 * As with indexscans, our definition of "pseudoconstant" is pretty liberal:
 * we allow anything that doesn't involve a volatile function or a Var of
 * the relation under consideration.  Vars belonging to other relations of
 * the query are allowed, giving rise to parameterized TID scans.
 *
 * We also support "WHERE CURRENT OF cursor" conditions (CurrentOfExpr),
 * which amount to "CTID = run-time-determined-TID".  These could in
 * theory be translated to a simple comparison of CTID to the result of
 * a function, but in practice it works better to keep the special node
 * representation all the way through to execution.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/path/tidpath.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/sysattr.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/restrictinfo.h"

/* source-code-compatibility hacks for pull_varnos() API change */
#define pull_varnos(a, b) pull_varnos_new(a, b)

/*
 * Does this Var represent the CTID column of the specified baserel?
 */
static inline bool
IsCTIDVar(Var *var, RelOptInfo *rel)
{






}

/*
 * Check to see if a RestrictInfo is of the form
 *		CTID = pseudoconstant
 * or
 *		pseudoconstant = CTID
 * where the CTID Var belongs to relation "rel", and nothing on the
 * other side of the clause does.
 */
static bool
IsTidEqualClause(RestrictInfo *rinfo, RelOptInfo *rel)
{













































}

/*
 * Check to see if a RestrictInfo is of the form
 *		CTID = ANY (pseudoconstant_array)
 * where the CTID Var belongs to relation "rel", and nothing on the
 * other side of the clause does.
 */
static bool
IsTidEqualAnyClause(PlannerInfo *root, RestrictInfo *rinfo, RelOptInfo *rel)
{




































}

/*
 * Check to see if a RestrictInfo is a CurrentOfExpr referencing "rel".
 */
static bool
IsCurrentOfClause(RestrictInfo *rinfo, RelOptInfo *rel)
{
















}

/*
 * Extract a set of CTID conditions from the given RestrictInfo
 *
 * Returns a List of CTID qual RestrictInfos for the specified rel (with
 * implicit OR semantics across the list), or NIL if there are no usable
 * conditions.
 *
 * This function considers only base cases; AND/OR combination is handled
 * below.  Therefore the returned List never has more than one element.
 * (Using a List may seem a bit weird, but it simplifies the caller.)
 */
static List *
TidQualFromRestrictInfo(PlannerInfo *root, RestrictInfo *rinfo, RelOptInfo *rel)
{



























}

/*
 * Extract a set of CTID conditions from implicit-AND List of RestrictInfos
 *
 * Returns a List of CTID qual RestrictInfos for the specified rel (with
 * implicit OR semantics across the list), or NIL if there are no usable
 * conditions.
 *
 * This function is just concerned with handling AND/OR recursion.
 */
static List *
TidQualFromRestrictInfoList(PlannerInfo *root, List *rlist, RelOptInfo *rel)
{








































































}

/*
 * Given a list of join clauses involving our rel, create a parameterized
 * TidPath for each one that is a suitable TidEqual clause.
 *
 * In principle we could combine clauses that reference the same outer rels,
 * but it doesn't seem like such cases would arise often enough to be worth
 * troubling over.
 */
static void
BuildParameterizedTidPaths(PlannerInfo *root, RelOptInfo *rel, List *clauses)
{














































}

/*
 * Test whether an EquivalenceClass member matches our rel's CTID Var.
 *
 * This is a callback for use by generate_implied_equalities_for_column.
 */
static bool
ec_member_matches_ctid(PlannerInfo *root, RelOptInfo *rel, EquivalenceClass *ec, EquivalenceMember *em, void *arg)
{





}

/*
 * create_tidscan_paths
 *	  Create paths corresponding to direct TID scans of the given rel.
 *
 *	  Candidate paths are added to the rel's pathlist (using add_path).
 */
void
create_tidscan_paths(PlannerInfo *root, RelOptInfo *rel)
{









































}