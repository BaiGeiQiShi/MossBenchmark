/*-------------------------------------------------------------------------
 *
 * joininfo.c
 *	  joininfo list manipulation routines
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/optimizer/util/joininfo.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "optimizer/joininfo.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"

/*
 * have_relevant_joinclause
 *		Detect whether there is a joinclause that involves
 *		the two given relations.
 *
 * Note: the joinclause does not have to be evaluable with only these two
 * relations.  This is intentional.  For example consider
 *		SELECT * FROM a, b, c WHERE a.x = (b.y + c.z)
 * If a is much larger than the other tables, it may be worthwhile to
 * cross-join b and c and then use an inner indexscan on a.x.  Therefore
 * we should consider this joinclause as reason to join b to c, even though
 * it can't be applied at that join step.
 */
bool
have_relevant_joinclause(PlannerInfo *root, RelOptInfo *rel1, RelOptInfo *rel2)
{









































}

/*
 * add_join_clause_to_rels
 *	  Add 'restrictinfo' to the joininfo list of each relation it requires.
 *
 * Note that the same copy of the restrictinfo node is linked to by all the
 * lists it is in.  This allows us to exploit caching of information about
 * the restriction clause (but we must be careful that the information does
 * not depend on context).
 *
 * 'restrictinfo' describes the join clause
 * 'join_relids' is the list of relations participating in the join clause
 *				 (there must be more than one)
 */
void
add_join_clause_to_rels(PlannerInfo *root, RestrictInfo *restrictinfo, Relids join_relids)
{









}

/*
 * remove_join_clause_from_rels
 *	  Delete 'restrictinfo' from all the joininfo lists it is in
 *
 * This reverses the effect of add_join_clause_to_rels.  It's used when we
 * discover that a relation need not be joined at all.
 *
 * 'restrictinfo' describes the join clause
 * 'join_relids' is the list of relations participating in the join clause
 *				 (there must be more than one)
 */
void
remove_join_clause_from_rels(PlannerInfo *root, RestrictInfo *restrictinfo, Relids join_relids)
{














}