/*-------------------------------------------------------------------------
 *
 * bipartite_match.c
 *	  Hopcroft-Karp maximum cardinality algorithm for bipartite graphs
 *
 * This implementation is based on pseudocode found at:
 *
 * https://en.wikipedia.org/w/index.php?title=Hopcroft%E2%80%93Karp_algorithm&oldid=593898016
 *
 * Copyright (c) 2015-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/lib/bipartite_match.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "lib/bipartite_match.h"
#include "miscadmin.h"

/*
 * The distances computed in hk_breadth_search can easily be seen to never
 * exceed u_size.  Since we restrict u_size to be less than SHRT_MAX, we
 * can therefore use SHRT_MAX as the "infinity" distance needed as a marker.
 */
#define HK_INFINITY SHRT_MAX

static bool
hk_breadth_search(BipartiteMatchState *state);
static bool
hk_depth_search(BipartiteMatchState *state, int u);

/*
 * Given the size of U and V, where each is indexed 1..size, and an adjacency
 * list, perform the matching and return the resulting state.
 */
BipartiteMatchState *
BipartiteMatch(int u_size, int v_size, short **adjacency)
{



































}

/*
 * Free a state returned by BipartiteMatch, except for the original adjacency
 * list, which is owned by the caller. This only frees memory, so it's optional.
 */
void
BipartiteMatchFree(BipartiteMatchState *state)
{






}

/*
 * Perform the breadth-first search step of H-K matching.
 * Returns true if successful.
 */
static bool
hk_breadth_search(BipartiteMatchState *state)
{














































}

/*
 * Perform the depth-first search step of H-K matching.
 * Returns true if successful.
 */
static bool
hk_depth_search(BipartiteMatchState *state, int u)
{




































}