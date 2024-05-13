/*-------------------------------------------------------------------------
 *
 * regexport.c
 *	  Functions for exporting info about a regex's NFA
 *
 * In this implementation, the NFA defines a necessary but not sufficient
 * condition for a string to match the regex: that is, there can be strings
 * that match the NFA but don't match the full regex, but not vice versa.
 * Thus, for example, it is okay for the functions below to treat lookaround
 * constraints as no-ops, since they merely constrain the string some more.
 *
 * Notice that these functions return info into caller-provided arrays
 * rather than doing their own malloc's.  This simplifies the APIs by
 * eliminating a class of error conditions, and in the case of colors
 * allows the caller to decide how big is too big to bother with.
 *
 *
 * Portions Copyright (c) 2013-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1998, 1999 Henry Spencer
 *
 * IDENTIFICATION
 *	  src/backend/regex/regexport.c
 *
 *-------------------------------------------------------------------------
 */

#include "regex/regguts.h"

#include "regex/regexport.h"

/*
 * Get total number of NFA states.
 */
int
pg_reg_getnumstates(const regex_t *regex)
{






}

/*
 * Get initial state of NFA.
 */
int
pg_reg_getinitialstate(const regex_t *regex)
{






}

/*
 * Get final state of NFA.
 */
int
pg_reg_getfinalstate(const regex_t *regex)
{






}

/*
 * pg_reg_getnumoutarcs() and pg_reg_getoutarcs() mask the existence of LACON
 * arcs from the caller, treating any LACON as being automatically satisfied.
 * Since the output representation does not support arcs that consume no
 * character when traversed, we have to recursively traverse LACON arcs here,
 * and report whatever normal arcs are reachable by traversing LACON arcs.
 * Note that this wouldn't work if it were possible to reach the final state
 * via LACON traversal, but the regex library never builds NFAs that have
 * LACON arcs leading directly to the final state.  (This is because the
 * regex executor is designed to consume one character beyond the nominal
 * match end --- possibly an EOS indicator --- so there is always a set of
 * ordinary arcs leading to the final state.)
 *
 * traverse_lacons is a recursive subroutine used by both exported functions
 * to count and then emit the reachable regular arcs.  *arcs_count is
 * incremented by the number of reachable arcs, and as many as will fit in
 * arcs_len (possibly 0) are emitted into arcs[].
 */
static void
traverse_lacons(struct cnfa *cnfa, int st, int *arcs_count, regex_arc_t *arcs, int arcs_len)
{































}

/*
 * Get number of outgoing NFA arcs of state number "st".
 */
int
pg_reg_getnumoutarcs(const regex_t *regex, int st)
{













}

/*
 * Write array of outgoing NFA arcs of state number "st" into arcs[],
 * whose length arcs_len must be at least as long as indicated by
 * pg_reg_getnumoutarcs(), else not all arcs will be returned.
 */
void
pg_reg_getoutarcs(const regex_t *regex, int st, regex_arc_t *arcs, int arcs_len)
{












}

/*
 * Get total number of colors.
 */
int
pg_reg_getnumcolors(const regex_t *regex)
{






}

/*
 * Check if color is beginning of line/string.
 *
 * (We might at some point need to offer more refined handling of pseudocolors,
 * but this will do for now.)
 */
int
pg_reg_colorisbegin(const regex_t *regex, int co)
{













}

/*
 * Check if color is end of line/string.
 */
int
pg_reg_colorisend(const regex_t *regex, int co)
{













}

/*
 * Get number of member chrs of color number "co".
 *
 * Note: we return -1 if the color number is invalid, or if it is a special
 * color (WHITE or a pseudocolor), or if the number of members is uncertain.
 * Callers should not try to extract the members if -1 is returned.
 */
int
pg_reg_getnumcharacters(const regex_t *regex, int co)
{




























}

/*
 * Write array of member chrs of color number "co" into chars[],
 * whose length chars_len must be at least as long as indicated by
 * pg_reg_getnumcharacters(), else not all chars will be returned.
 *
 * Fetching the members of WHITE or a pseudocolor is not supported.
 *
 * Caution: this is a relatively expensive operation.
 */
void
pg_reg_getcharacters(const regex_t *regex, int co, pg_wchar *chars, int chars_len)
{






























}
