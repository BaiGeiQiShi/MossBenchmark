/*-------------------------------------------------------------------------
 *
 * regprefix.c
 *	  Extract a common prefix, if any, from a compiled regex.
 *
 *
 * Portions Copyright (c) 2012-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1998, 1999 Henry Spencer
 *
 * IDENTIFICATION
 *	  src/backend/regex/regprefix.c
 *
 *-------------------------------------------------------------------------
 */

#include "regex/regguts.h"

/*
 * forward declarations
 */
static int
findprefix(struct cnfa *cnfa, struct colormap *cm, chr *string, size_t *slength);

/*
 * pg_regprefix - get common prefix for regular expression
 *
 * Returns one of:
 *	REG_NOMATCH: there is no common prefix of strings matching the regex
 *	REG_PREFIX: there is a common prefix of strings matching the regex
 *	REG_EXACT: all strings satisfying the regex must match the same string
 *	or a REG_XXX error code
 *
 * In the non-failure cases, *string is set to a malloc'd string containing
 * the common prefix or exact value, of length *slength (measured in chrs
 * not bytes!).
 *
 * This function does not analyze all complex cases (such as lookaround
 * constraints) exactly.  Therefore it is possible that some strings matching
 * the reported prefix or exact-match string do not satisfy the regex.  But
 * it should never be the case that a string satisfying the regex does not
 * match the reported prefix or exact-match string.
 */
int
pg_regprefix(regex_t *re, chr **string, size_t *slength)
{































































}

/*
 * findprefix - extract common prefix from cNFA
 *
 * Results are returned into the preallocated chr array string[], with
 * *slength (which must be preset to zero) incremented for each chr.
 */
static int /* regprefix return code */
findprefix(struct cnfa *cnfa, struct colormap *cm, chr *string, size_t *slength)
{






































































































































































}